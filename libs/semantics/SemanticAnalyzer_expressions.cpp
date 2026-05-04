#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Infrastructure & Binding Helpers
// ============================================================================

void SemanticAnalyzer::bind_identifier(Identifier& id, Symbol_ptr symbol)
{
    id.symbol = symbol;
    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        id.must_be_captured = true;
    }
}

Symbol_ptr SemanticAnalyzer::resolve_module_export(MemberAccess& access)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.member() only."
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& member_identifier = access.right->as<Identifier>();

    Symbol_ptr module_symbol = current_scope->lookup(module_identifier.name);
    Doctor::get().fatal_if_nullptr(module_symbol, WaspStage::Semantics);

    bind_identifier(module_identifier, module_symbol);

    auto& module_data = module_symbol->get_payload_as<ModuleData>();
    Symbol_ptr export_symbol = module_data.mod->get_member(member_identifier.name);

    Doctor::get().fatal_if_nullptr(
        export_symbol,
        WaspStage::Semantics,
        "Member '" + member_identifier.name + "' not found in module."
    );

    access.member_index = module_data.mod->get_member_index(member_identifier.name);
    return export_symbol;
}

// ============================================================================
// Main Visitor Dispatcher
// ============================================================================

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        overloaded{
            // Primitives
            [&](int& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](double& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](std::string& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](bool& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](NoneLiteral& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](DotLiteral& node) -> Object_ptr
            {
                return visit(node);
            },

            // Identifiers & Access
            [&](Identifier& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](MemberAccess& node) -> Object_ptr
            {
                return visit(node);
            },

            // Call-sites
            [&](Call& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](Constructor& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](TemplateInstantiation& node) -> Object_ptr
            {
                return visit(node);
            },

            // Operators
            [&](Prefix& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](Infix& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](Postfix& node) -> Object_ptr
            {
                return visit(node);
            },

            // Collections
            [&](ListLiteral& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](TupleLiteral& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](MapLiteral& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](SetLiteral& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](RangeLiteral& node) -> Object_ptr
            {
                return visit(node);
            },

            // Assignments & Patterns
            [&](VariableDefinitionExpression& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](UntypedAssignment& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](TypedAssignment& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](TypePattern& node) -> Object_ptr
            {
                return visit(node);
            },

            // Control Flow
            [&](IfTernaryBranch& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](ElseTernaryBranch& node) -> Object_ptr
            {
                return visit(node);
            },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression in Semantic Analyzer!"
                );
                return nullptr;
            }
        },
        expr->data
    );
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
{
    ObjectVector computed_types;
    computed_types.reserve(expressions.size());
    for (const auto& expr : expressions)
    {
        computed_types.push_back(visit(expr));
    }
    return computed_types;
}

// ============================================================================
// Identifiers & Member Access
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr resolved_symbol = current_scope->lookup(identifier.name);
    Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

    bind_identifier(identifier, resolved_symbol);
    return resolved_symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr)
{
    Object_ptr left_type = visit(expr.left);

    Doctor::get().assert(
        expr.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier."
    );

    std::string member_name = expr.right->as<Identifier>().name;

    if (left_type->is<ModuleType_ptr>())
    {
        const auto type = left_type->as<ModuleType_ptr>();
        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }

    if (left_type->is<ClassType_ptr>())
    {
        const auto type = left_type->as<ClassType_ptr>();
        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }

    if (left_type->is<TemplateType_ptr>())
    {
        const auto type = left_type->as<TemplateType_ptr>()->underlying_type;
        if (type->is<ClassType_ptr>())
        {
            auto class_type = type->as<ClassType_ptr>();
            expr.member_index = class_type->get_member_index(member_name);
            return class_type->get_member(member_name);
        }
        if (type->is<TraitType_ptr>())
        {
            auto trait_type = type->as<TraitType_ptr>();
            expr.member_index = trait_type->get_member_index(member_name);
            return trait_type->get_member(member_name);
        }
    }

    if (left_type->is<EnumType_ptr>())
    {
        const auto type = left_type->as<EnumType_ptr>();
        std::string full_name = type->name + "." + member_name;

        if (type->nested_enums.contains(full_name))
            return make_object(type->nested_enums.at(full_name));

        Doctor::get().assert(
            type->members.contains(full_name),
            WaspStage::Semantics,
            "Enum '" + type->name + "' does not contain member '" + member_name + "'."
        );

        expr.member_index = type->members.at(full_name);
        expr.is_enum_value = true;
        return left_type;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + member_name + "'. LHS is not a module, class, trait, or enum."
    );
    return nullptr;
}

// ============================================================================
// Function Calls & Method Evaluation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto resolved_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);
                return evaluate_function_call(call, target, argument_types, resolved_symbol);
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);

                ClassType_ptr class_type = nullptr;
                TraitType_ptr trait_type = nullptr;

                // Resolve Receiver Type
                if (left_type->is<ClassType_ptr>())
                {
                    class_type = left_type->as<ClassType_ptr>();
                }
                else if (
                    left_type->is<TemplateType_ptr>() &&
                    left_type->as<TemplateType_ptr>()->underlying_type->is<ClassType_ptr>()
                )
                {
                    class_type = left_type->as<TemplateType_ptr>()
                                     ->underlying_type->as<ClassType_ptr>();
                }

                if (!class_type)
                {
                    if (left_type->is<TraitType_ptr>())
                    {
                        trait_type = left_type->as<TraitType_ptr>();
                    }
                    else if (
                        left_type->is<TemplateType_ptr>() &&
                        left_type->as<TemplateType_ptr>()->underlying_type->is<TraitType_ptr>()
                    )
                    {
                        trait_type = left_type->as<TemplateType_ptr>()
                                         ->underlying_type->as<TraitType_ptr>();
                    }
                }

                if (class_type)
                {
                    if (access.right->is<Identifier>())
                    {
                        std::string name = access.right->as<Identifier>().name;
                        if (!class_type->is_pure(name) && !class_type->is_static(name))
                            call.is_method_call = true;
                    }
                    return evaluate_class_method_call(call, access, argument_types, class_type);
                }

                if (trait_type)
                {
                    if (access.right->is<Identifier>())
                    {
                        std::string name = access.right->as<Identifier>().name;
                        if (!trait_type->is_pure(name) && !trait_type->is_static(name))
                            call.is_method_call = true;
                    }
                    return evaluate_trait_method_call(call, access, argument_types, trait_type);
                }

                if (left_type->is<ModuleType_ptr>())
                    return evaluate_module_function_call(call, access, argument_types);

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Receiver is not a class, trait, or module."
                );
                return nullptr;
            },
            [&](TemplateInstantiation& template_instantiation) -> Object_ptr
            {
                return evaluate_template_call(call, template_instantiation, argument_types);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable."
                );
                return nullptr;
            }
        },
        call.callable->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_function_call(
    Call& call,
    Identifier& identifier,
    const ObjectVector& args,
    Symbol_ptr symbol
)
{
    bind_identifier(identifier, symbol);
    const auto& overloads = symbol->get_payload_as<OverloadsData>().get_overloads();
    auto [func, idx] = type_checker->get_best_function_signature(current_scope, overloads, args);
    call.overload_index = idx;
    return get_function_return_type(func);
}

Object_ptr SemanticAnalyzer::evaluate_module_function_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& args
)
{
    Symbol_ptr export_symbol = resolve_module_export(access);
    Doctor::get().assert(
        export_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol is not an overload group"
    );

    const auto& group_data = export_symbol->get_payload_as<OverloadsData>();
    auto [resolved_function, idx] = type_checker->get_best_function_signature(
        current_scope,
        group_data.get_overloads(),
        args
    );

    call.overload_index = idx;
    access.right->as<Identifier>().symbol = resolved_function;
    return get_function_return_type(resolved_function);
}

Object_ptr SemanticAnalyzer::evaluate_class_method_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& args,
    ClassType_ptr class_type
)
{
    auto method_name = access.right->as<Identifier>().name;
    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method not found on class."
    );

    auto member = class_type->get_member(method_name);
    Doctor::get().assert(
        member->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Member must be an overload group."
    );

    const auto object_overloads = member->as<ObjectOverloadList_ptr>();
    ObjectVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < object_overloads->overloads.size(); ++i)
    {
        auto overload = object_overloads->overloads[i];
        auto [ret, params] = get_function_signature(overload);
        if (type_checker->assignable(current_scope, params, args))
        {
            valid_matches.push_back(overload);
            match_indices.push_back(static_cast<int>(i));
        }
    }

    Doctor::get()
        .assert(!valid_matches.empty(), WaspStage::Semantics, "No matching signature found.");
    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous function call.");

    access.member_index = class_type->get_member_index(method_name);
    call.overload_index = match_indices.front();
    call.is_method_call = true;
    call.is_pure_method_call = class_type->is_pure(method_name);

    Object_ptr ret = get_function_signature(valid_matches.front()).first;
    return ret->is<TemplateType_ptr>() ? make_object(class_type) : ret;
}

Object_ptr SemanticAnalyzer::evaluate_trait_method_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& args,
    TraitType_ptr trait_type
)
{
    auto method_name = access.right->as<Identifier>().name;
    Doctor::get().assert(
        trait_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method not found on trait."
    );

    auto member = trait_type->get_member(method_name);
    const auto object_overloads = member->as<ObjectOverloadList_ptr>();

    ObjectVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < object_overloads->overloads.size(); ++i)
    {
        auto [ret, params] = get_function_signature(object_overloads->overloads[i]);
        if (type_checker->assignable(current_scope, params, args))
        {
            valid_matches.push_back(object_overloads->overloads[i]);
            match_indices.push_back(static_cast<int>(i));
        }
    }

    Doctor::get().assert(!valid_matches.empty(), WaspStage::Semantics, "No matching signature.");
    access.member_index = trait_type->get_member_index(method_name);
    call.overload_index = match_indices.front();
    call.is_method_call = true;
    call.is_pure_method_call = trait_type->is_pure(method_name);

    return get_function_signature(valid_matches.front()).first;
}

// ============================================================================
// Template Instantiation & Evaluation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(TemplateInstantiation& template_instantiation)
{
    ObjectVector generic_args;
    for (const auto& arg : template_instantiation.arguments)
        generic_args.push_back(visit(arg));

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);
                return evaluate_type_template_instantiation(
                    template_instantiation,
                    target,
                    symbol,
                    generic_args
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Symbol_ptr symbol = resolve_module_export(access);
                return evaluate_type_template_instantiation(
                    template_instantiation,
                    access.right->as<Identifier>(),
                    symbol,
                    generic_args
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected Identifier or MemberAccess in template."
                );
                return nullptr;
            }
        },
        template_instantiation.target->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_template_call(
    Call& call,
    TemplateInstantiation& ti,
    const ObjectVector& argument_types
)
{
    ObjectVector generic_args;
    for (const auto& arg : ti.arguments)
        generic_args.push_back(visit(arg));

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto sym = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(sym, WaspStage::Semantics);
                return evaluate_template_function_call(call, ti, target, argument_types, sym);
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);
                if (left_type->is<ClassType_ptr>())
                    return evaluate_template_method_call(
                        call,
                        ti,
                        access,
                        argument_types,
                        left_type->as<ClassType_ptr>()
                    );
                if (left_type->is<ModuleType_ptr>())
                    return evaluate_template_module_function_call(call, ti, access, argument_types);

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Receiver is neither a class nor a module."
                );
                return nullptr;
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess inside TemplateInstantiation."
                );
                return nullptr;
            }
        },
        ti.target->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_type_template_instantiation(
    TemplateInstantiation& ti,
    Identifier& target,
    Symbol_ptr symbol,
    const ObjectVector& generic_args
)
{
    ti.group_symbol = symbol;
    bind_identifier(target, symbol);

    auto type_obj = symbol->get_type();
    Doctor::get().fatal_if_nullptr(type_obj, WaspStage::Semantics);

    if (type_obj->is<TemplateType_ptr>())
    {
        auto t_type = type_obj->as<TemplateType_ptr>();
        Doctor::get().assert(
            t_type->generics.size() == generic_args.size(),
            WaspStage::Semantics,
            "Generic arguments count mismatch."
        );

        size_t idx = 0;
        for (const auto& [name, generic_obj] : t_type->generics)
        {
            auto gen_type = generic_obj->as<GenericType_ptr>();
            Doctor::get().assert(
                type_checker
                    ->assignable(current_scope, gen_type->constraint_type, generic_args[idx]),
                WaspStage::Semantics,
                "Type bound violated for parameter '" + name + "'."
            );
            idx++;
        }

        if (t_type->underlying_type->is<ClassType_ptr>())
        {
            auto base = t_type->underlying_type->as<ClassType_ptr>();
            ObjectStringMap concrete;
            for (const auto& [name, type] : base->members)
                concrete[name] = type_checker->substitute_generics(type, t_type, generic_args);
            return make_object(
                std::make_shared<ClassType>(
                    base->name,
                    concrete,
                    base->fields,
                    base->methods,
                    base->pures,
                    base->statics
                )
            );
        }

        if (t_type->underlying_type->is<TraitType_ptr>())
        {
            auto base = t_type->underlying_type->as<TraitType_ptr>();
            ObjectStringMap concrete;
            for (const auto& [name, type] : base->members)
                concrete[name] = type_checker->substitute_generics(type, t_type, generic_args);
            return make_object(
                std::make_shared<
                    TraitType>(base->name, concrete, base->methods, base->pures, base->statics)
            );
        }

        return type_checker->substitute_generics(t_type->underlying_type, t_type, generic_args);
    }

    Doctor::get().fatal(WaspStage::Semantics, "Symbol is not a valid type template.");
    return nullptr;
}

Object_ptr SemanticAnalyzer::evaluate_template_function_call(
    Call& call,
    TemplateInstantiation& ti,
    Identifier& target,
    const ObjectVector& argument_types,
    Symbol_ptr group_symbol
)
{
    ObjectVector generic_args;
    for (const auto& arg : ti.arguments)
        generic_args.push_back(visit(arg));

    Doctor::get().assert(
        group_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Expected overload group."
    );
    const auto& overloads = group_symbol->get_payload_as<OverloadsData>().get_overloads();

    ObjectVector valid_matches;
    std::vector<int> match_indices;
    Object_ptr final_ret = nullptr;
    Symbol_ptr matched_sym = nullptr;
    ObjectVector final_params;

    for (size_t i = 0; i < overloads.size(); ++i)
    {
        auto overload = overloads[i];
        auto t = overload->get_type()->try_as<TemplateType_ptr>();
        if (!t)
            continue;

        auto ft = *t;
        if (ft->generics.size() != generic_args.size())
            continue;

        bool bounds_met = true;
        size_t idx = 0;
        for (const auto& [name, gen_obj] : ft->generics)
        {
            if (!type_checker->assignable(
                    current_scope,
                    gen_obj->as<GenericType_ptr>()->constraint_type,
                    generic_args[idx]
                ))
            {
                bounds_met = false;
                break;
            }
            idx++;
        }
        if (!bounds_met)
            continue;

        ObjectVector concrete_params;
        auto underlying = ft->underlying_type->as<Signature_ptr>();
        for (const auto& p : underlying->parameter_types)
            concrete_params.push_back(type_checker->substitute_generics(p, ft, generic_args));

        if (type_checker->assignable(current_scope, concrete_params, argument_types))
        {
            valid_matches.push_back(overload->get_type());
            match_indices.push_back(static_cast<int>(i));
            matched_sym = overload;
            final_params = concrete_params;
            final_ret = type_checker
                            ->substitute_generics(underlying->return_type, ft, generic_args);
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching template signature found."
    );
    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous template call.");

    auto concrete_type = make_object(
        std::make_shared<Signature>(Signature{final_params, final_ret})
    );
    auto concrete_symbol = SymbolFactory::create_function(
        group_symbol->name,
        concrete_type,
        matched_sym->is_native(),
        group_symbol->closure_depth,
        group_symbol->lexical_depth
    );

    concrete_symbol->id = group_symbol->id;
    ti.symbol = concrete_symbol;
    ti.group_symbol = group_symbol;
    target.symbol = concrete_symbol;
    if (group_symbol->should_be_captured(current_scope->get_closure_depth()))
        target.must_be_captured = true;

    call.overload_index = matched_sym->is_native_function_or_method() ? -1 : match_indices.front();
    return final_ret;
}

Object_ptr SemanticAnalyzer::evaluate_template_method_call(
    Call& call,
    TemplateInstantiation& ti,
    MemberAccess& access,
    const ObjectVector& args,
    ClassType_ptr class_type
)
{
    auto method_name = access.right->as<Identifier>().name;
    auto member = class_type->get_member(method_name);
    const auto object_overloads = member->as<ObjectOverloadList_ptr>();

    ObjectVector generic_args;
    for (const auto& arg : ti.arguments)
        generic_args.push_back(visit(arg));

    ObjectVector valid_matches;
    std::vector<int> match_indices;
    Object_ptr final_ret = nullptr;

    for (size_t i = 0; i < object_overloads->overloads.size(); ++i)
    {
        auto overload = object_overloads->overloads[i];
        if (!overload->is<TemplateType_ptr>())
            continue;
        auto ft = overload->as<TemplateType_ptr>();

        if (ft->generics.size() != generic_args.size())
            continue;

        ObjectVector concrete_params;
        auto underlying = ft->underlying_type->as<Signature_ptr>();
        for (const auto& p : underlying->parameter_types)
            concrete_params.push_back(type_checker->substitute_generics(p, ft, generic_args));

        if (type_checker->assignable(current_scope, concrete_params, args))
        {
            valid_matches.push_back(overload);
            match_indices.push_back(static_cast<int>(i));
            final_ret = type_checker
                            ->substitute_generics(underlying->return_type, ft, generic_args);
        }
    }

    Doctor::get()
        .assert(!valid_matches.empty(), WaspStage::Semantics, "No matching generic method found.");
    access.member_index = class_type->get_member_index(method_name);
    call.overload_index = match_indices.front();
    call.is_method_call = true;
    call.is_pure_method_call = class_type->is_pure(method_name);

    return final_ret;
}

Object_ptr SemanticAnalyzer::evaluate_template_module_function_call(
    Call& call,
    TemplateInstantiation& ti,
    MemberAccess& access,
    const ObjectVector& args
)
{
    Symbol_ptr symbol = resolve_module_export(access);
    return evaluate_template_function_call(call, ti, access.right->as<Identifier>(), args, symbol);
}

// ============================================================================
// Constructor & Instance Creation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    ObjectVector argument_types = visit(constructor.values);

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto sym = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(sym, WaspStage::Semantics);
                Doctor::get().assert(
                    sym->payload_is<ClassData>(),
                    WaspStage::Semantics,
                    "Target must be a class."
                );
                return evaluate_instance_creation(constructor, target, sym, argument_types);
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                return evaluate_module_instance_creation(constructor, access, argument_types);
            },
            [&](TemplateInstantiation& ti) -> Object_ptr
            {
                ObjectVector generic_args;
                for (const auto& arg : ti.arguments)
                    generic_args.push_back(visit(arg));

                return std::visit(
                    overloaded{
                        [&](Identifier& target) -> Object_ptr
                        {
                            auto sym = current_scope->lookup(target.name);
                            Doctor::get().fatal_if_nullptr(sym, WaspStage::Semantics);
                            return evaluate_class_template_instantiation(
                                constructor,
                                ti,
                                target,
                                argument_types,
                                generic_args,
                                sym
                            );
                        },
                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "Expected Identifier for class template target."
                            );
                            return nullptr;
                        }
                    },
                    ti.target->data
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid constructor target.");
                return nullptr;
            }
        },
        constructor.construtable->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_instance_creation(
    Constructor& constructor,
    Identifier& target,
    Symbol_ptr class_symbol,
    const ObjectVector& args
)
{
    bind_identifier(target, class_symbol);
    auto class_type = class_symbol->get_type()->as<ClassType_ptr>();
    validate_constructor_args(class_type, args);
    return class_symbol->get_type();
}

Object_ptr SemanticAnalyzer::evaluate_module_instance_creation(
    Constructor& constructor,
    MemberAccess& access,
    const ObjectVector& args
)
{
    Symbol_ptr sym = resolve_module_export(access);
    Doctor::get()
        .assert(sym->payload_is<ClassData>(), WaspStage::Semantics, "Symbol is not a class.");
    return evaluate_instance_creation(constructor, access.right->as<Identifier>(), sym, args);
}

Object_ptr SemanticAnalyzer::evaluate_class_template_instantiation(
    Constructor& constructor,
    TemplateInstantiation& ti,
    Identifier& target,
    const ObjectVector& args,
    const ObjectVector& generic_args,
    Symbol_ptr template_symbol
)
{
    auto class_template_type = template_symbol->get_type()->as<TemplateType_ptr>();
    Doctor::get().assert(
        class_template_type->generics.size() == generic_args.size(),
        WaspStage::Semantics,
        "Generic mismatch."
    );

    auto base_class_type = class_template_type->underlying_type->as<ClassType_ptr>();
    auto concrete = std::make_shared<ClassType>(
        base_class_type->name,
        ObjectStringMap{},
        base_class_type->fields,
        base_class_type->methods,
        base_class_type->pures,
        base_class_type->statics
    );
    auto concrete_obj = make_object(concrete);

    ObjectStringMap members;
    for (const auto& [name, type] : base_class_type->members)
        members[name] = type_checker->substitute_generics(type, class_template_type, generic_args);
    concrete->members = members;

    validate_constructor_args(concrete, args);

    auto concrete_symbol = SymbolFactory::create_class(
        template_symbol->name,
        concrete_obj,
        template_symbol->closure_depth,
        template_symbol->lexical_depth
    );
    concrete_symbol->id = template_symbol->id;
    ti.symbol = concrete_symbol;
    ti.group_symbol = template_symbol;
    bind_identifier(target, concrete_symbol);

    return concrete_obj;
}

void SemanticAnalyzer::validate_constructor_args(
    ClassType_ptr class_type,
    const ObjectVector& arg_types
)
{
    Doctor::get().assert(
        arg_types.size() == class_type->fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch."
    );
    for (size_t i = 0; i < class_type->fields.size(); ++i)
    {
        Object_ptr expected = class_type->get_member(class_type->fields[i]);
        Doctor::get().assert(
            type_checker->assignable(current_scope, expected, arg_types[i]),
            WaspStage::Semantics,
            "Constructor type mismatch."
        );
    }
}

// ============================================================================
// Primitives & Operators
// ============================================================================

Object_ptr SemanticAnalyzer::visit(int expr)
{
    return make_object(IntLiteralType(expr));
}
Object_ptr SemanticAnalyzer::visit(double expr)
{
    return make_object(FloatLiteralType(expr));
}
Object_ptr SemanticAnalyzer::visit(std::string expr)
{
    return make_object(StringLiteralType(expr));
}
Object_ptr SemanticAnalyzer::visit(bool expr)
{
    return expr ? workspace->pool->get_true_literal_type()
                : workspace->pool->get_false_literal_type();
}
Object_ptr SemanticAnalyzer::visit(NoneLiteral& expr)
{
    return workspace->pool->get_none_type();
}
Object_ptr SemanticAnalyzer::visit(DotLiteral& expr)
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return type_checker->infer(current_scope, visit(expr.operand), expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    return type_checker->infer(current_scope, visit(expr.left), expr.op.type, visit(expr.right));
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    Object_ptr left_type = visit(expr.operand);
    type_checker->expect_number_type(left_type);
    return left_type;
}

// ============================================================================
// Collections
// ============================================================================

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(ListType(make_object(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);
    if (unique_types.size() == 1)
        return make_object(ListType(unique_types[0]));
    return make_object(ListType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));
    return make_object(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    if (expr.pairs.empty())
        return make_object(MapType(make_object(AnyType()), make_object(AnyType())));

    ObjectVector key_types, val_types;
    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);
        type_checker->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    ObjectVector u_keys = type_checker->remove_duplicates(current_scope, key_types);
    ObjectVector u_vals = type_checker->remove_duplicates(current_scope, val_types);

    Object_ptr fk = u_keys.size() == 1 ? u_keys[0] : make_object(VariantType(u_keys));
    Object_ptr fv = u_vals.size() == 1 ? u_vals[0] : make_object(VariantType(u_vals));
    return make_object(MapType(fk, fv));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(SetType(make_object(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        Object_ptr el_type = visit(element);
        type_checker->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);
    if (unique_types.size() == 1)
        return make_object(SetType(unique_types[0]));
    return make_object(SetType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr)
{
    Object_ptr start_type = expr.start ? visit(expr.start) : nullptr;
    Object_ptr end_type = expr.end ? visit(expr.end) : nullptr;

    if (start_type)
        type_checker->expect_number_type(start_type);
    if (end_type)
        type_checker->expect_number_type(end_type);

    if (type_checker->is_float_type(start_type) || type_checker->is_float_type(end_type))
        return make_object(ListType(make_object(FloatType())));

    return make_object(ListType(make_object(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Type patterns can only be used in match patterns.");
}

} // namespace Wasp
