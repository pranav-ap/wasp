#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"

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

namespace
{

StringVector collect_enum_names(const EnumDefinition& def, const std::string& prefix)
{
    std::string current_prefix = prefix.empty() ? def.name : prefix + "." + def.name;

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
        out_list.insert(out_list.end(), nested_names.begin(), nested_names.end());
    }

    return out_list;
}

} // namespace

// ============================================================================
// Statements
// ============================================================================

void SemanticsAnalyzer::collect(Block& block)
{
    for (Statement_ptr& statement : block.statements)
    {
        collect(statement);
    }
}

void SemanticsAnalyzer::collect(Statement_ptr statement)
{
    std::visit(
        overloaded{[&](auto& node)
                   {
                       if constexpr (requires { collect(node); })
                       {
                           collect(node);
                       }
                   }},
        statement->data
    );
}

void SemanticsAnalyzer::collect(EnumDefinition& def)
{
    enter_scope(ScopeType::ENUM);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    // TODO : template type not yet supported in enum

    StringVector full_names = collect_enum_names(def, "");

    Type_ptr enum_type_obj = def.symbol->get_type();
    Doctor::semantics().fatal_if_nullptr(enum_type_obj);

    Doctor::semantics().check(
        enum_type_obj->is<EnumType_ptr>(),
        "Expected EnumType_ptr for enum definition"
    );

    EnumType_ptr enum_type = enum_type_obj->as<EnumType_ptr>();
    enum_type->members = std::move(full_names);

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

void SemanticsAnalyzer::collect(TypeAliasDefinition& def)
{
    enter_scope(ScopeType::TYPE_ALIAS);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    // TODO : template type not yet supported in type alias

    auto alias_type = visit(def.ref_type);
    def.symbol->set_type(alias_type);

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);

    leave_scope();
}

void SemanticsAnalyzer::collect(FunctionDefinition& def)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (Field& param : def.parameters)
    {
        auto param_type = visit(param.type);
        param_types.push_back(param_type);

        param.symbol = SymbolFactory::create_variable(
            param.name,
            param_type,
            false, // TODO : immutable by default?
            current_scope->closure_depth,
            current_scope->lexical_depth
        );
    }

    Type_ptr type = def.symbol->get_type();

    FunctionType_ptr function_type = type->as<FunctionType_ptr>();
    function_type->parameter_types = param_types;
    function_type->return_type = return_type;
    function_type->template_type = template_type;
    function_type->is_pure = def.is_pure;
    function_type->is_native = false;

    if (def.block.statements.size() == 1)
    {
        auto lonely = def.block.statements[0];

        if (lonely->is<Native>())
        {
            function_type->is_native = true;
        }
    }

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

void SemanticsAnalyzer::collect(OperatorDefinition& def)
{
    enter_scope(ScopeType::PURE_FUNCTION);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    TypeVector param_types;

    for (Field& operand : def.operands)
    {
        auto operand_type = visit(operand.type);
        param_types.push_back(operand_type);

        operand.symbol = SymbolFactory::create_variable(
            operand.name,
            operand_type,
            false,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );
    }

    Type_ptr type = def.symbol->get_type();

    FunctionType_ptr function_type = type->as<FunctionType_ptr>();
    function_type->parameter_types = param_types;
    function_type->return_type = return_type;
    function_type->template_type = template_type;
    function_type->is_pure = true;
    function_type->is_native = false;

    if (def.block.statements.size() == 1)
    {
        auto lonely = def.block.statements[0];

        if (lonely->is<Native>())
        {
            function_type->is_native = true;
        }
    }

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

void SemanticsAnalyzer::collect(ClassDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    FieldMap_ptr fields = collect(def.fields);

    hoist(def.methods);
    MethodMap_ptr methods = collect(def.methods, def.symbol);

    TypeVector traits = visit(def.traits);

    Type_ptr type = def.symbol->get_type();
    ClassType_ptr class_type = type->as<ClassType_ptr>();
    class_type->fields = fields;
    class_type->methods = methods;
    class_type->traits = traits;
    class_type->template_type = template_type;

    conform_to_traits(def, class_type);

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

void SemanticsAnalyzer::collect(TraitDefinition& def)
{
    enter_scope(ScopeType::TRAIT);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    FieldMap_ptr fields = collect(def.fields);

    hoist(def.methods);
    MethodMap_ptr methods = collect(def.methods, def.symbol);

    TypeVector traits = visit(def.traits);

    Type_ptr type = def.symbol->get_type();
    TraitType_ptr trait_type = type->as<TraitType_ptr>();
    trait_type->fields = fields;
    trait_type->methods = methods;
    trait_type->traits = traits;
    trait_type->template_type = template_type;

    conform_to_traits(def, trait_type);

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

void SemanticsAnalyzer::collect(PrimitiveDefinition& def)
{
    enter_scope(ScopeType::PRIMITIVE);

    TemplateType_ptr template_type = create_template_type(def.generics);
    current_scope->define(template_type);

    FieldMap_ptr fields = collect(def.fields);

    hoist(def.methods);
    MethodMap_ptr methods = collect(def.methods, def.symbol);

    TypeVector traits = visit(def.traits);

    Type_ptr type = def.symbol->get_type();
    PrimitiveType_ptr primitive_type = type->as<PrimitiveType_ptr>();
    primitive_type->fields = fields;
    primitive_type->methods = methods;
    primitive_type->traits = traits;
    primitive_type->template_type = template_type;

    conform_to_traits(def, primitive_type);

    leave_scope();

    add_tree(def.symbol, ASTCloner::get().clone(def), current_scope);
}

FieldMap_ptr SemanticsAnalyzer::collect(FieldVector& fields)
{
    TypeStringMap field_map;
    StringVector ordered_keys;

    for (const auto& field : fields)
    {
        Doctor::semantics().check(
            !field_map.contains(field.name),
            "Duplicate field name: " + field.name
        );

        Type_ptr field_type = visit(field.type);
        field_map[field.name] = field_type;
        ordered_keys.push_back(field.name);
    }

    return std::make_shared<FieldMap>(std::move(field_map), std::move(ordered_keys));
}

MethodMap_ptr SemanticsAnalyzer::collect(
    MethodDefinitionVector& methods,
    Symbol_ptr owner_symbol
)
{
    std::map<std::string, MethodTypeVector> method_map;
    StringVector ordered_keys;

    for (MethodDefinition& method : methods)
    {
        if (!method_map.contains(method.name))
        {
            ordered_keys.push_back(method.name);
            method_map[method.name] = {};
        }

        MethodType_ptr method_type = collect(method, owner_symbol);

        MethodTypeVector& method_types = method_map[method.name];
        method_types.push_back(method_type);
    }

    return std::make_shared<MethodMap>(
        std::move(method_map),
        std::move(ordered_keys)
    );
}

MethodType_ptr SemanticsAnalyzer::collect(
    MethodDefinition& def,
    Symbol_ptr owner_symbol
)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;

    enter_scope(scope_type);

    Type_ptr return_type = make_shared_type<NoneType>();

    if (def.return_type)
    {
        return_type = visit(def.return_type);
    }

    def.our_context_symbol = SymbolFactory::create_variable(
        "our",
        owner_symbol->get_type(),
        false, // TODO : immutable by default?
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    if (!def.is_shared)
    {
        def.self_context_symbol = SymbolFactory::create_variable(
            "self",
            owner_symbol->get_type(),
            true, // TODO : immutable by default?
            current_scope->closure_depth,
            current_scope->lexical_depth
        );
    }

    TypeVector param_types;

    for (Field& param : def.parameters)
    {
        auto param_type = visit(param.type);
        param_types.push_back(param_type);

        param.symbol = SymbolFactory::create_variable(
            param.name,
            param_type,
            false, // TODO : immutable by default?
            current_scope->closure_depth,
            current_scope->lexical_depth
        );
    }

    Type_ptr type = def.symbol->get_type();

    MethodType_ptr method_type = type->as<MethodType_ptr>();
    method_type->parameter_types = param_types;
    method_type->return_type = return_type;
    method_type->template_type = std::make_shared<TemplateType>();
    method_type->is_shared = def.is_shared;
    method_type->is_pure = def.is_pure;
    method_type->is_native = false;
    method_type->is_required = false;

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

    leave_scope();

    return method_type;
}

// ============================================================================
// Template Type
// ============================================================================

TemplateType_ptr SemanticsAnalyzer::create_template_type(FieldVector& generics)
{
    TypeStringMap generics_map;
    StringVector ordered_names;
    bool seen_variadic_generic = false;

    for (Field& generic : generics)
    {
        GenericType_ptr generic_type = std::make_shared<GenericType>(generic.name);

        Type_ptr declared_constraint_type = visit(generic.type);

        // TODO tpp restrictive?

        if (declared_constraint_type->is<IntersectionType_ptr>())
        {
            auto types = declared_constraint_type->as<IntersectionType_ptr>()->types;

            for (auto& inner_type : types)
            {
                Doctor::semantics().check(
                    inner_type->is<TraitType_ptr>(),
                    "Only an interesection of traits is supported"
                );
            }
        }
        else if (declared_constraint_type->is<VariantType_ptr>())
        {
            auto types = declared_constraint_type->as<VariantType_ptr>()->types;

            for (auto& inner_type : types)
            {
                Doctor::semantics().check(
                    type_system->is_primitive_type(inner_type),
                    "Only an union of primitives is supported"
                );
            }
        }

        Doctor::semantics().check(
            !generics_map.contains(generic.name),
            "Duplicate generic parameter name: " + generic.name
        );

        if (generic.is_variadic)
        {
            Doctor::semantics().check(
                !seen_variadic_generic,
                "Only one variadic generic parameter is allowed"
            );

            seen_variadic_generic = true;
        }

        generic_type->constraint_type = declared_constraint_type;
        generic_type->is_variadic = generic.is_variadic;

        generics_map[generic.name] = make_type(generic_type);
        ordered_names.push_back(generic.name);
    }

    return std::make_shared<TemplateType>(
        std::move(generics_map),
        std::move(ordered_names)
    );
}

// ============================================================================
// Validate New Callables
// ============================================================================

void SemanticsAnalyzer::validate_new_function_type(Symbol_ptr candidate)
{
    Symbol_ptr friends = current_scope->lookup_local(candidate->name);
    Symbol_ptr parents = current_scope->lookup_parent_overload(candidate->name);

    Type_ptr candidate_type = candidate->get_type();

    Doctor::semantics().check(
        candidate_type->is<FunctionType_ptr>(),
        "Expected FunctionType for symbol: " + candidate->name
    );

    FunctionType_ptr candidate_function_type = candidate_type->as<FunctionType_ptr>();

    if (friends)
    {
        validate_new_function_type_friends(candidate, friends, candidate_function_type);
    }

    if (parents)
    {
        shadow_new_function_type_parents(candidate, parents, candidate_function_type);
    }
}

void SemanticsAnalyzer::validate_new_function_type_friends(
    Symbol_ptr candidate,
    Symbol_ptr friends,
    FunctionType_ptr candidate_function_type
)
{
    OverloadSymbol& friends_overload = friends->as<OverloadSymbol>();

    for (const Symbol_ptr& friend_symbol : friends_overload.overloads)
    {
        Type_ptr friend_type = friend_symbol->get_type();

        Doctor::semantics().check(
            friend_type->is<FunctionType_ptr>(),
            "Expected FunctionType for symbol: " + friend_symbol->name
        );

        FunctionType_ptr friend_function_type = friend_type->as<FunctionType_ptr>();

        bool signatures_match = type_system->signatures_match(
            current_scope,
            friend_function_type,
            candidate_function_type
        );

        Doctor::semantics().check(
            !signatures_match,
            "Function '" + candidate->name + "' has a duplicate signature"
        );
    }
}

void SemanticsAnalyzer::shadow_new_function_type_parents(
    Symbol_ptr candidate,
    Symbol_ptr parents,
    FunctionType_ptr candidate_function_type
)
{
    OverloadSymbol& parents_overload = parents->as<OverloadSymbol>();

    for (const Symbol_ptr& parent_symbol : parents_overload.overloads)
    {
        Type_ptr parent_type = parent_symbol->get_type();

        Doctor::semantics().check(
            parent_type->is<FunctionType_ptr>(),
            "Expected FunctionType for symbol: " + parent_symbol->name
        );

        FunctionType_ptr parent_function_type = parent_type->as<FunctionType_ptr>();

        bool signatures_match = type_system->signatures_match(
            current_scope,
            parent_function_type,
            candidate_function_type
        );

        if (!signatures_match)
        {
            OverloadSymbol& candidate_overload = candidate->as<OverloadSymbol>();
            candidate_overload.overloads.push_back(parent_symbol);
        }
    }
}

// ============================================================================
// Trait Conformance
// ============================================================================

void SemanticsAnalyzer::conform_to_traits(
    TypeDefinition& def,
    OopsType_ptr target_type
)
{
    if (target_type->traits.empty())
    {
        return;
    }

    std::vector<MethodType_ptr> required_method_types = collect_required_methods(
        target_type
    );

    validate_required_methods(def, required_method_types);
    merge_trait_methods(def, target_type);
}

std::vector<MethodType_ptr> SemanticsAnalyzer::collect_required_methods(
    OopsType_ptr target_type
)
{
    std::vector<MethodType_ptr> required_method_types;

    for (const Type_ptr& trait_obj : target_type->traits)
    {
        TraitType_ptr trait_type = trait_obj->as<TraitType_ptr>();

        std::vector<MethodType_ptr>
            current_trait_required_method_types = collect_required_methods(
                trait_type
            );

        required_method_types.insert(
            required_method_types.end(),
            current_trait_required_method_types.begin(),
            current_trait_required_method_types.end()
        );
    }

    return required_method_types;
}

MethodTypeVector SemanticsAnalyzer::collect_required_methods(TraitType_ptr trait_type)
{
    MethodTypeVector required_method_types;

    for (auto [method_name, method_overload_types] :
         trait_type->methods->method_overload_types)
    {
        for (const MethodType_ptr& method_type : method_overload_types)
        {
            if (method_type->is_required)
            {
                required_method_types.push_back(method_type);
            }
        }
    }

    return required_method_types;
}

void SemanticsAnalyzer::validate_required_methods(
    const TypeDefinition& def,
    const std::vector<MethodType_ptr>& required_method_types
)
{
    for (const MethodType_ptr& required_method_type : required_method_types)
    {
        bool found_the_required_method = false;

        for (const MethodDefinition& method : def.methods)
        {
            if (method.name == required_method_type->name)
            {
                MethodType_ptr candidate_method_type = method.symbol->get_type()
                                                           ->as<MethodType_ptr>();

                found_the_required_method = type_system->signatures_match(
                    current_scope,
                    required_method_type,
                    candidate_method_type
                );

                if (found_the_required_method)
                {
                    break;
                }
            }
        }

        Doctor::semantics().check(
            found_the_required_method,
            "Class '" + def.name + "' does not implement required method '" +
                required_method_type->name + "' from trait"
        );
    }
}

void SemanticsAnalyzer::merge_trait_methods(
    TypeDefinition& target_def,
    OopsType_ptr target_type
)
{
    for (const Type_ptr& trait_obj : target_type->traits)
    {
        TraitType_ptr trait_type = trait_obj->as<TraitType_ptr>();

        Symbol_ptr trait_symbol = current_scope->lookup_required(trait_type->name);
        auto [ast, definition_scope] = get_tree(trait_symbol);

        TraitDefinition& trait_def = ast->as<TraitDefinition>();

        merge_trait_methods(target_def, target_type, trait_def);
    }
}

void SemanticsAnalyzer::merge_trait_methods(
    TypeDefinition& target_def,
    OopsType_ptr target_type,
    TraitDefinition& trait_def
)
{
    for (const MethodDefinition& trait_method : trait_def.methods)
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
                trait_method_type,
                target_method_type
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

        Statement_ptr trait_method_statement_clone = ASTCloner::get().clone(
            trait_method
        );

        Doctor::semantics().check(
            trait_method_statement_clone->is<MethodDefinition>(),
            "Expected MethodDefinition when cloning trait method"
        );

        MethodDefinition& trait_method_clone = trait_method_statement_clone
                                                   ->as<MethodDefinition>();

        target_def.methods.push_back(trait_method_clone);

        if (!target_type->methods->contains(trait_method.name))
        {
            target_type->methods->add(trait_method.name);
        }

        target_type->methods->get_type(trait_method.name).push_back(trait_method_type);
    }
}

} // namespace Wasp
