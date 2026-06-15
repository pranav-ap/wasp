#include "TypeCollection.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeAnnotation.h"
#include "Workspace.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void TypeCollection::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void TypeCollection::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );
    current_scope = new_scope;
}

void TypeCollection::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

// ============================================================================
// Statements
// ============================================================================

void TypeCollection::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void TypeCollection::visit(Statement_ptr statement)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        statement->data
    );
}

void TypeCollection::visit(Import&)
{
    // TODO: Implement
}

void TypeCollection::visit(FunctionDefinition& def)
{
    Signature_ptr signature = analyze(def);
    def.symbol->set_type(make_type(signature));
}

Signature_ptr TypeCollection::analyze(FunctionDefinition& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (const auto& param : def.parameters)
    {
        param_types.push_back(visit(param.type));
    }

    leave_scope();

    auto signature = std::make_shared<Signature>(
        param_types,
        return_type,
        template_type
    );

    return signature;
}

void TypeCollection::visit(OperatorDefinition& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (const auto& param : def.operands)
    {
        param_types.push_back(visit(param.type));
    }

    leave_scope();

    auto signature = make_shared_type<Signature>(
        param_types,
        return_type,
        template_type
    );

    def.symbol->set_type(signature);
}

void TypeCollection::visit(ClassDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    auto class_type = make_shared_type<ClassType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(class_type);

    leave_scope();
}

void TypeCollection::visit(TraitDefinition& def)
{
    enter_scope(ScopeType::TRAIT);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    auto trait_type = make_shared_type<TraitType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(trait_type);

    leave_scope();
}

void TypeCollection::visit(PrimitiveDefinition& def)
{
    enter_scope(ScopeType::PRIMITIVE);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    auto primitive_type = make_shared_type<PrimitiveType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(primitive_type);

    leave_scope();
}

FieldMap_ptr TypeCollection::track_fields(FieldVector fields)
{
    TypeStringMap field_map;
    StringVector ordered_keys;

    for (const auto& field : fields)
    {
        Doctor::get().assert(
            !field_map.contains(field.name),
            WaspStage::Semantics,
            "Duplicate field name: " + field.name
        );

        auto field_type = visit(field.type);
        field_map[field.name] = field_type;
        ordered_keys.push_back(field.name);
    }

    return std::make_shared<FieldMap>(
        std::move(field_map),
        std::move(ordered_keys)
    );
}

MethodMap_ptr TypeCollection::track_methods(
    FunctionDefinitionVector methods
)
{
    std::map<std::string, SignatureSet_ptr> method_map;
    StringVector ordered_keys;

    for (auto& method : methods)
    {
        if (!method_map.contains(method.name))
        {
            ordered_keys.push_back(method.name);
            method_map[method.name] = std::make_shared<
                SignatureSet>();
        }

        auto signature = analyze(method);

        SignatureSet_ptr signatures_set = method_map[method.name];
        signatures_set->add(signature);
    }

    return std::make_shared<MethodMap>(
        std::move(method_map),
        std::move(ordered_keys)
    );
}

TypeVector TypeCollection::track_traits(TypeAnnotationVector traits)
{
    TypeVector trait_types;

    for (const auto& trait : traits)
    {
        Type_ptr trait_type = visit(trait);
        trait_types.push_back(trait_type);
    }

    return trait_types;
}

StringVector collect_enum_names(
    const EnumDefinition& def,
    const std::string& prefix
)
{
    std::string current_prefix = prefix.empty()
                                     ? def.name
                                     : prefix + "." + def.name;

    StringVector out_list;

    // Add current members
    for (const auto& member : def.members)
    {
        out_list.push_back(current_prefix + "." + member);
    }

    // Recurse into nested enums
    for (const auto& nested : def.nested_enums)
    {
        auto nested_names = collect_enum_names(
            nested,
            current_prefix
        );
        out_list.insert(
            out_list.end(),
            nested_names.begin(),
            nested_names.end()
        );
    }

    return out_list;
}

void TypeCollection::visit(EnumDefinition& def)
{
    StringVector full_names = collect_enum_names(def, "");

    auto enum_type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(
        enum_type_obj,
        WaspStage::Semantics
    );

    Doctor::get().assert(
        enum_type_obj->is<EnumType_ptr>(),
        WaspStage::Semantics,
        "Expected EnumType_ptr for enum definition"
    );

    auto enum_type = enum_type_obj->as<EnumType_ptr>();
    enum_type->members = std::move(full_names);
}

void TypeCollection::visit(TypeAliasDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    auto template_type = create_template_type(def.generics);
    define_template_type(template_type);

    auto alias_type = visit(def.ref_type);
    def.symbol->set_type(alias_type);

    leave_scope();
}

void TypeCollection::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void TypeCollection::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void TypeCollection::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

// ============================================================================
// Expressions
// ============================================================================

void TypeCollection::visit(std::vector<Expression_ptr>& expressions)
{
    for (auto& expr : expressions)
    {
        visit(expr);
    }
}

void TypeCollection::visit(Expression_ptr expression)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        expression->data
    );
}

void TypeCollection::visit(Binding& binding)
{
    Doctor::get().assert(
        binding.lhs->is<Identifier>(),
        WaspStage::Parser,
        "Left-hand side of a binding must be an identifier"
    );

    auto& id = binding.lhs->as<Identifier>();

    if (!binding.declared_type)
    {
        return;
    }

    auto type = visit(binding.declared_type);
    id.symbol->set_type(type);
}

// ============================================================================
// Types
// ============================================================================

TypeVector TypeCollection::visit(
    const TypeAnnotationVector& type_nodes
)
{
    TypeVector resolved_types;

    for (const auto& node : type_nodes)
    {
        auto type = visit(node);
        resolved_types.push_back(type);
    }

    return resolved_types;
}

Type_ptr TypeCollection::visit(const TypeAnnotation_ptr type_node)
{
    Doctor::get().fatal_if_nullptr(type_node, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](std::monostate&) -> Type_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Type node is in monostate"
                );
            },
            [&](auto& node) -> Type_ptr
            {
                return this->visit(node);
            }
        },
        type_node->data
    );
}

Type_ptr TypeCollection::visit(NoneTypeNode&)
{
    return make_shared_type<NoneType>();
}

Type_ptr TypeCollection::visit(TypeIdentifierNode& type_node)
{
    auto symbol = current_scope->lookup(type_node.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined type: " + type_node.name
    );

    auto type = symbol->get_type();

    Doctor::get().fatal_if_nullptr(
        type,
        WaspStage::Semantics,
        "Symbol is not a type: " + type_node.name
    );

    return type;
}

Type_ptr TypeCollection::visit(ListTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<ListType>(element_type);
}

Type_ptr TypeCollection::visit(TupleTypeNode& type_node)
{
    TypeVector element_types = visit(type_node.element_types);
    return make_shared_type<TupleType>(element_types);
}

Type_ptr TypeCollection::visit(SetTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<SetType>(element_type);
}

Type_ptr TypeCollection::visit(MapTypeNode& type_node)
{
    Type_ptr key_type = visit(type_node.key_type);
    Type_ptr value_type = visit(type_node.value_type);
    return make_shared_type<MapType>(key_type, value_type);
}

Type_ptr TypeCollection::visit(VariantTypeNode& type_node)
{
    TypeVector options = visit(type_node.options);
    return make_shared_type<VariantType>(options);
}

Type_ptr TypeCollection::visit(IntersectionTypeNode& type_node)
{
    TypeVector types = visit(type_node.types);
    return make_shared_type<IntersectionType>(types);
}

Type_ptr TypeCollection::visit(FunctionTypeNode& type_node)
{
    TypeVector input_types = visit(type_node.input_types);
    Type_ptr return_type = visit(type_node.return_type);
    return make_shared_type<Signature>(input_types, return_type);
}

Type_ptr TypeCollection::visit(AngularTypeNode& type_node)
{
    TypeVector type_arguments = visit(type_node.type_arguments);

    auto type = make_shared_type<AngularType>(
        type_node.name,
        type_arguments
    );

    type_node.symbol->set_type(type);

    return type;
}

// ============================================================================
// Utils
// ============================================================================

TemplateType_ptr TypeCollection::create_template_type(
    const FieldVector& generics
)
{
    TypeStringMap generics_map;
    StringVector ordered_names;
    bool seen_variadic_generic = false;

    for (const auto& generic : generics)
    {
        Type_ptr constraint_type = make_shared_type<GenericType>(
            generic.name
        );

        if (constraint_type->is<IntersectionType_ptr>())
        {
            auto types = constraint_type->as<IntersectionType_ptr>()
                             ->types;

            for (auto& inner_type : types)
            {
                Doctor::get().assert(
                    inner_type->is<TraitType_ptr>(),
                    WaspStage::Semantics,
                    "Only an interesection of traits is supported"
                );
            }
        }
        else if (constraint_type->is<VariantType_ptr>())
        {
            auto types = constraint_type->as<VariantType_ptr>()
                             ->types;

            for (auto& inner_type : types)
            {
                Doctor::get().assert(
                    type_system->is_primitive_type(inner_type),
                    WaspStage::Semantics,
                    "Only an union of primitives is supported"
                );
            }
        }

        Doctor::get().assert(
            !generics_map.contains(generic.name),
            WaspStage::Semantics,
            "Duplicate generic parameter name: " + generic.name
        );

        if (generic.is_variadic)
        {
            Doctor::get().assert(
                !seen_variadic_generic,
                WaspStage::Semantics,
                "Only one variadic generic parameter is allowed"
            );

            seen_variadic_generic = true;
        }

        auto generic_type = make_shared_type<GenericType>(
            generic.name,
            constraint_type,
            generic.is_variadic
        );

        generics_map[generic.name] = generic_type;
        ordered_names.push_back(generic.name);
    }

    return std::make_shared<TemplateType>(
        std::move(generics_map),
        std::move(ordered_names)
    );
}

void TypeCollection::define_template_type(
    TemplateType_ptr template_type
)
{
    if (template_type->empty())
    {
        return;
    }

    auto ordered_generics = template_type->get_ordered_generics();

    for (const auto& [name, generic_type] : ordered_generics)
    {
        auto symbol = SymbolFactory::create_type(name, generic_type);
        current_scope->define(symbol);
    }
}

} // namespace Wasp
