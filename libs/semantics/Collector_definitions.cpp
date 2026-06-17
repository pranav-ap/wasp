#include "AST.h"
#include "ASTCloner.h"
#include "Collector.h"
#include "Doctor.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{
TemplateType_ptr create_template_type(
    const FieldVector& generics,
    TypeSystem_ptr type_system
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
                Doctor::semantics().assert(
                    inner_type->is<TraitType_ptr>(),
                    "Only an interesection of traits is supported"
                );
            }
        }
        else if (constraint_type->is<VariantType_ptr>())
        {
            auto types = constraint_type->as<VariantType_ptr>()->types;

            for (auto& inner_type : types)
            {
                Doctor::semantics().assert(
                    type_system->is_primitive_type(inner_type),
                    "Only an union of primitives is supported"
                );
            }
        }

        Doctor::semantics().assert(
            !generics_map.contains(generic.name),
            "Duplicate generic parameter name: " + generic.name
        );

        if (generic.is_variadic)
        {
            Doctor::semantics().assert(
                !seen_variadic_generic,
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

StringVector collect_enum_names(
    const EnumDefinition& def,
    const std::string& prefix
)
{
    std::string current_prefix = prefix.empty() ? def.name
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
        auto nested_names = collect_enum_names(nested, current_prefix);
        out_list.insert(
            out_list.end(),
            nested_names.begin(),
            nested_names.end()
        );
    }

    return out_list;
}

} // namespace

void Collector::visit(FunctionDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
    auto signature = extract_signature(def);

    FunctionType_ptr function_type = std::make_shared<FunctionType>(
        def.name,
        signature,
        def.is_pure,
        false
    );

    def.symbol->set_type(make_type(function_type));

    if (def.block.statements.size() == 1)
    {
        auto lonely = def.block.statements[0];

        if (lonely->is<Native>())
        {
            function_type->is_native = true;
        }
    }

    visit(def.block);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Collector::visit(MethodDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
    auto signature = extract_signature(def);

    MethodType_ptr method_type = std::make_shared<MethodType>(
        def.name,
        signature,
        def.is_shared,
        def.is_pure,
        false,
        false
    );

    def.symbol->set_type(make_type(method_type));

    if (def.block.statements.size() == 1)
    {
        auto lonely = def.block.statements[0];

        if (lonely->is<Required>())
        {
            method_type->is_required = true;
        }
        else if (lonely->is<Native>())
        {
            method_type->is_native = true;
        }
    }

    visit(def.block);
}

void Collector::visit(OperatorDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
    auto signature = extract_signature(def);

    bool is_pure = true;

    FunctionType_ptr function_type = std::make_shared<FunctionType>(
        def.name,
        signature,
        is_pure,
        false
    );

    def.symbol->set_type(make_type(function_type));

    if (def.block.statements.size() == 1)
    {
        auto lonely = def.block.statements[0];

        if (lonely->is<Native>())
        {
            function_type->is_native = true;
        }
    }

    visit(def.block);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Collector::visit(ClassDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::CLASS);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    // init class type

    ClassType_ptr class_type = std::make_shared<ClassType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(make_type(class_type));

    // visit methods

    for (auto& method : def.methods)
    {
        visit(method);
    }

    // conform traits

    conform_traits(def, class_type);

    // clone AST

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }

    leave_scope();
}

void Collector::visit(TraitDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::TRAIT);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    TraitType_ptr trait_type = std::make_shared<TraitType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(make_type(trait_type));

    for (auto& method : def.methods)
    {
        visit(method);
    }

    conform_traits(def, trait_type);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }

    leave_scope();
}

void Collector::visit(PrimitiveDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::PRIMITIVE);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    FieldMap_ptr fields = track_fields(def.fields);
    MethodMap_ptr methods = track_methods(def.methods);
    TypeVector traits = track_traits(def.traits);

    PrimitiveType_ptr primitive_type = std::make_shared<PrimitiveType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(make_type(primitive_type));

    for (auto& method : def.methods)
    {
        visit(method);
    }

    conform_traits(def, primitive_type);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }

    leave_scope();
}

void Collector::visit(EnumDefinition& def)
{
    StringVector full_names = collect_enum_names(def, "");

    auto enum_type_obj = def.symbol->get_type();
    Doctor::semantics().fatal_if_nullptr(enum_type_obj);

    Doctor::semantics().assert(
        enum_type_obj->is<EnumType_ptr>(),
        "Expected EnumType_ptr for enum definition"
    );

    auto enum_type = enum_type_obj->as<EnumType_ptr>();
    enum_type->members = std::move(full_names);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Collector::visit(TypeAliasDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    auto alias_type = visit(def.ref_type);
    def.symbol->set_type(alias_type);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }

    leave_scope();
}

// ============================================================================
// Utils
// ============================================================================

Signature_ptr Collector::extract_signature(FunctionDefinition& def)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (const auto& param : def.parameters)
    {
        auto type = visit(param.type);
        param_types.push_back(type);

        param.symbol->set_type(type);
    }

    visit(def.block);

    leave_scope();

    auto signature = std::make_shared<Signature>(
        param_types,
        return_type,
        template_type
    );

    return signature;
}

Signature_ptr Collector::extract_signature(MethodDefinition& def)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;

    enter_scope(scope_type);

    auto template_type = create_template_type({}, type_system);
    current_scope->define(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (const auto& param : def.parameters)
    {
        auto type = visit(param.type);
        param_types.push_back(type);

        param.symbol->set_type(type);
    }

    visit(def.block);

    leave_scope();

    auto signature = std::make_shared<Signature>(
        param_types,
        return_type,
        template_type
    );

    return signature;
}

Signature_ptr Collector::extract_signature(OperatorDefinition& def)
{
    enter_scope(ScopeType::PURE_FUNCTION);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (const auto& param : def.operands)
    {
        auto type = visit(param.type);
        param_types.push_back(type);

        param.symbol->set_type(type);
    }

    visit(def.block);

    leave_scope();

    auto signature = std::make_shared<Signature>(
        param_types,
        return_type,
        template_type
    );

    return signature;
}

FieldMap_ptr Collector::track_fields(FieldVector fields)
{
    TypeStringMap field_map;
    StringVector ordered_keys;

    for (const auto& field : fields)
    {
        Doctor::semantics().assert(
            !field_map.contains(field.name),
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

MethodMap_ptr Collector::track_methods(MethodDefinitionVector methods)
{
    std::map<std::string, MethodOverloadType_ptr> method_map;
    StringVector ordered_keys;

    for (auto& method : methods)
    {
        if (!method_map.contains(method.name))
        {
            ordered_keys.push_back(method.name);
            method_map[method.name] = std::make_shared<MethodOverloadType>();
        }

        auto signature = extract_signature(method);

        MethodOverloadType_ptr method_overload_type = method_map[method.name];
        method_overload_type->add(
            std::make_shared<MethodType>(
                method.name,
                signature,
                method.is_shared,
                method.is_pure,
                false
            )
        );
    }

    return std::make_shared<MethodMap>(
        std::move(method_map),
        std::move(ordered_keys)
    );
}

TypeVector Collector::track_traits(TypeNodeVector traits)
{
    TypeVector trait_types;

    for (const auto& trait : traits)
    {
        Type_ptr trait_type = visit(trait);
        trait_types.push_back(trait_type);
    }

    return trait_types;
}

// ============================================================================
// Trait Conformance
// ============================================================================

void Collector::conform_traits(TypeDefinition& def, OopsType_ptr target_type)
{
    if (target_type->traits.empty())
    {
        return;
    }

    std::vector<MethodType_ptr> required_method_types = collect_required_methods(
        target_type
    );

    validate_required_methods(def, required_method_types);
    merge_trait_methods(def, target_type, current_scope);
}

std::vector<MethodType_ptr> Collector::collect_required_methods(
    OopsType_ptr target_type
)
{
    std::vector<MethodType_ptr> required_method_types;

    for (const auto& trait_obj : target_type->traits)
    {
        auto trait_type = trait_obj->as<TraitType_ptr>();

        auto trait_required_methods = collect_required_methods(trait_type);
        required_method_types.insert(
            required_method_types.end(),
            trait_required_methods.begin(),
            trait_required_methods.end()
        );
    }

    return required_method_types;
}

std::vector<MethodType_ptr> Collector::collect_required_methods(
    TraitType_ptr trait_type
)
{
    std::vector<MethodType_ptr> required_method_types;

    for (auto [method_name, signatures] : trait_type->methods->signatures)
    {
        for (const auto& method_type : signatures->method_types)
        {
            if (method_type->is_required)
            {
                required_method_types.push_back(method_type);
            }
        }
    }

    return required_method_types;
}

void Collector::validate_required_methods(
    const TypeDefinition& def,
    const std::vector<MethodType_ptr>& required_method_types
)
{
    for (const auto& required_method_type : required_method_types)
    {
        bool found_the_required_method = false;

        for (const auto& method : def.methods)
        {
            if (method.name == required_method_type->name)
            {
                Signature_ptr candidate_signature = method.symbol->get_type()
                                                        ->as<MethodType_ptr>()
                                                        ->signature;

                found_the_required_method = type_system->signatures_match(
                    current_scope,
                    required_method_type->signature,
                    candidate_signature
                );

                if (found_the_required_method)
                {
                    break;
                }
            }
        }

        Doctor::semantics().assert(
            found_the_required_method,
            "Class '" + def.name + "' does not implement required method '" +
                required_method_type->name + "' from trait"
        );
    }
}

void Collector::merge_trait_methods(
    TypeDefinition& target_def,
    OopsType_ptr target_type,
    SymbolScope_ptr // definition_scope
)
{
    for (const auto& trait_obj : target_type->traits)
    {
        auto trait_type = trait_obj->as<TraitType_ptr>();

        Symbol_ptr trait_symbol = current_scope->lookup_required(trait_type->name);
        auto [ast, definition_scope] = get_tree(trait_symbol);

        auto trait_def = ast->as<TraitDefinition>();

        merge_trait_methods(target_def, target_type, trait_def, definition_scope);
    }
}

void Collector::merge_trait_methods(
    TypeDefinition& target_def,
    OopsType_ptr target_type,
    TraitDefinition& trait_def,
    SymbolScope_ptr // definition_scope
)
{
    for (const auto& trait_method : trait_def.methods)
    {
        MethodType_ptr trait_method_type = trait_method.symbol->get_type()
                                               ->as<MethodType_ptr>();

        if (trait_method_type->is_required)
        {
            // already checked for conformance, so we can skip required methods
            continue;
        }

        bool already_exists = false;

        for (const MethodDefinition& target_method : target_def.methods)
        {
            if (target_method.name != trait_method.name)
            {
                continue;
            }

            MethodType_ptr target_method_type = target_method.symbol->get_type()
                                                    ->as<MethodType_ptr>();

            already_exists = type_system->signatures_match(
                current_scope,
                trait_method_type->signature,
                target_method_type->signature
            );

            if (already_exists)
            {
                break;
            }
        }

        if (already_exists)
        {
            continue;
        }

        auto trait_method_statement_clone = ASTCloner::get().clone(trait_method);

        Doctor::semantics().assert(
            trait_method_statement_clone->is<MethodDefinition>(),
            "Expected MethodDefinition when cloning trait method"
        );

        auto trait_method_clone = trait_method_statement_clone
                                      ->as<MethodDefinition>();

        target_def.methods.push_back(trait_method_clone);

        target_type->methods->get_type(trait_method.name)->add(trait_method_type);
    }
}

} // namespace Wasp
