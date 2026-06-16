#include "AST.h"
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
    auto signature = analyze(def);
    def.symbol->set_type(make_type(signature));
}

void Collector::visit(OperatorDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
    auto signature = analyze(def);
    def.symbol->set_type(make_type(signature));
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

    auto class_type = make_shared_type<ClassType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(class_type);

    for (auto& method : def.methods)
    {
        visit(method);
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

    auto class_type = make_shared_type<TraitType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(class_type);

    for (auto& method : def.methods)
    {
        visit(method);
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

    auto class_type = make_shared_type<PrimitiveType>(
        def.name,
        fields,
        methods,
        traits,
        template_type
    );

    def.symbol->set_type(class_type);

    for (auto& method : def.methods)
    {
        visit(method);
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
}

void Collector::visit(TypeAliasDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    auto template_type = create_template_type(def.generics, type_system);
    current_scope->define(template_type);

    auto alias_type = visit(def.ref_type);
    def.symbol->set_type(alias_type);

    leave_scope();
}

// ============================================================================
// Utils
// ============================================================================

Signature_ptr Collector::analyze(FunctionDefinition& def)
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

Signature_ptr Collector::analyze(OperatorDefinition& def)
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

MethodMap_ptr Collector::track_methods(FunctionDefinitionVector methods)
{
    std::map<std::string, SignatureSet_ptr> method_map;
    StringVector ordered_keys;

    for (auto& method : methods)
    {
        if (!method_map.contains(method.name))
        {
            ordered_keys.push_back(method.name);
            method_map[method.name] = std::make_shared<SignatureSet>();
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

} // namespace Wasp
