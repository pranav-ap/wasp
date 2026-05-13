#include "AST.h"
#include "ASTCloner.h"
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

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                auto unresolved = current_scope->lookup(id.name);
                Doctor::get().fatal_if_nullptr(
                    unresolved,
                    WaspStage::Semantics,
                    "Undefined callable: '" + id.name + "'"
                );

                auto symbol = unresolved->resolve();
                bind_identifier(id, symbol);

                return resolve_standard_overload(call, symbol, argument_types);
            },
            [&](TemplateAngular& ta)
            {
                return call_template_function(call, ta, argument_types);
            },
            [&](MemberAccess& ma) -> Object_ptr
            {
                Object_ptr left_type = visit(ma.left);

                return std::visit(
                    overloaded{
                        [&](ClassType_ptr class_type) -> Object_ptr
                        {
                            return call_method(call, ma, argument_types, class_type);
                        },
                        [&](ModuleType_ptr module_type) -> Object_ptr
                        {
                            auto member_symbol = get_module_member_symbol(ma);
                            ma.right->as<Identifier>().symbol = member_symbol;

                            return resolve_standard_overload(
                                call,
                                member_symbol,
                                argument_types
                            );
                        },
                        [&](TemplateParameterType_ptr template_parameter_type)
                            -> Object_ptr
                        {
                            return call_template_method(
                                call,
                                ma,
                                argument_types,
                                template_parameter_type
                            );
                        },
                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "LHS of call must be a module, class, or template"
                            );
                        }
                    },
                    left_type->value
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier, TemplateAngular, or MemberAccess as "
                    "the callable."
                );
            }
        },
        call.callable->data
    );
}

Object_ptr SemanticAnalyzer::resolve_standard_overload(
    Call& call,
    Symbol_ptr overload_symbol,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol must hold function overloads."
    );

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

    auto [function_symbol, overload_index] = type_system->get_best_function_symbol(
        current_scope,
        candidates,
        argument_types
    );

    call.overload_index = overload_index;
    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    ClassType_ptr class_type
)
{
    auto method_name = access.right->as<Identifier>().name;

    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class '" +
            class_type->name + "'."
    );

    auto member = class_type->get_member(method_name);

    Doctor::get().assert(
        member->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be an object overload group."
    );

    const auto& overloads = member->as<ObjectOverloadList_ptr>()->overloads;

    auto [signature_obj, overload_index] = type_system->get_best_function_object(
        current_scope,
        overloads,
        argument_types
    );

    access.member_index = class_type->get_member_index(method_name);
    call.overload_index = overload_index;

    call.is_method_call = true;

    return signature_obj->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_template_function(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    ObjectVector concrete_arguments;

    for (const auto& node : template_angular.angular_nodes)
    {
        concrete_arguments.push_back(visit(node));
    }

    Symbol_ptr overload_symbol = resolve_target_symbol(template_angular.target);
    template_angular.symbol = overload_symbol;

    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol '" + overload_symbol->name + "' must hold function overloads."
    );

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

    auto generic_candidates = type_system->filter_by_generic_arity(
        candidates,
        concrete_arguments.size()
    );

    Doctor::get().assert(
        !generic_candidates.empty(),
        WaspStage::Semantics,
        "No generic functions with required arity found for '" +
            overload_symbol->name + "'"
    );

    auto [specialized_candidates, original_indices] = type_system
                                                          ->specialize_candidates(
                                                              generic_candidates,
                                                              concrete_arguments
                                                          );

    auto [function_object, subset_index] = type_system->get_best_function_object(
        current_scope,
        specialized_candidates,
        argument_types
    );

    // --- 1. Get the original generic symbol ---
    Symbol_ptr generic_symbol = generic_candidates[subset_index].first;

    // --- 2. Build the Substitution Map (T -> int) ---
    ObjectStringMap substitutions;
    auto generic_names = type_system->get_generics_declaration_order(
        generic_symbol->get_type()
    );

    for (size_t i = 0; i < generic_names.size(); ++i)
    {
        substitutions[generic_names[i]] = concrete_arguments[i];
    }

    // --- 3. Deep Clone the AST Blueprint ---
    ASTCloner cloner(substitutions);
    Statement_ptr cloned_stmt = nullptr;

    if (generic_symbol->payload_is<FunctionData>())
    {
        auto& data = generic_symbol->get_payload_as<FunctionData>();
        Doctor::get().assert(
            data.ast_blueprint.has_value(),
            WaspStage::Semantics,
            "AST Blueprint is missing from the generic function '" +
                generic_symbol->name + "'."
        );

        Statement_ptr wrapper = make_statement(data.ast_blueprint.value());
        cloned_stmt = cloner.clone(wrapper);
    }
    else if (generic_symbol->payload_is<MethodData>())
    {
        auto& data = generic_symbol->get_payload_as<MethodData>();
        Doctor::get().assert(
            data.ast_blueprint.has_value(),
            WaspStage::Semantics,
            "AST Blueprint is missing from the generic method '" +
                generic_symbol->name + "'."
        );

        Statement_ptr wrapper = make_statement(data.ast_blueprint.value());
        cloned_stmt = cloner.clone(wrapper);
    }

    // --- 4. Re-Analyze the Cloned Body (Scope Trap Fix) ---

    // A. Save the caller's scope so variables don't leak into the template
    SymbolScope_ptr caller_scope = current_scope;

    // B. Jump back to the Module scope (or the root if Module isn't found)
    while (current_scope->get_enclosing() != nullptr &&
           current_scope->get_type() != ScopeType::MODULE)
    {
        current_scope = current_scope->get_enclosing();
    }

    // C. Re-analyze the fully cloned and specialized definition
    if (cloned_stmt && cloned_stmt->is<FunctionDefinition>())
    {
        auto& specialized_def = cloned_stmt->as<FunctionDefinition>();

        // CRITICAL FIX: The cloner wiped the symbol. Give it a new concrete one!
        specialized_def.symbol = SymbolFactory::create_function(
            generic_symbol->name,
            function_object, // Pass the specialized Signature_ptr
            generic_symbol->is_native(),
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        // It is no longer generic, clear the AST field so `analyze_callable`
        // analyzes the body immediately
        specialized_def.generics.clear();

        analyze_callable(
            specialized_def,
            specialized_def.is_pure ? ScopeType::PURE_FUNCTION : ScopeType::FUNCTION,
            nullptr,
            false
        );
    }
    else if (cloned_stmt && cloned_stmt->is<MethodDefinition>())
    {
        auto& specialized_def = cloned_stmt->as<MethodDefinition>();

        // CRITICAL FIX: The cloner wiped the symbol. Give it a new concrete one!
        specialized_def.symbol = SymbolFactory::create_method(
            generic_symbol->name,
            function_object, // Pass the specialized Signature_ptr
            generic_symbol->is_native(),
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        // It is no longer generic, clear the AST field so `analyze_callable`
        // analyzes the body immediately
        specialized_def.generics.clear();

        analyze_callable(
            specialized_def,
            specialized_def.is_pure ? ScopeType::PURE_FUNCTION : ScopeType::FUNCTION,
            nullptr, // Context type (fetch if needed for methods)
            specialized_def.is_static
        );
    }

    // D. Restore the caller's scope so the rest of the file analyzes correctly
    current_scope = caller_scope;
    if (current_module && cloned_stmt)
    {
        current_module->stmts.push_back(cloned_stmt);
    }
    call.overload_index = original_indices[subset_index];

    return function_object->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_template_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    TemplateParameterType_ptr template_parameter_type
)
{
    std::string method_name = access.right->as<Identifier>().name;

    return std::visit(
        overloaded{
            [&](VariantType& union_type) -> Object_ptr
            {
                ObjectVector all_return_types;

                for (auto& variant : union_type.types)
                {
                    auto class_type = try_unwrap_ptr<ClassType_ptr>(variant);

                    Object_ptr ret_type = call_method(
                        call,
                        access,
                        argument_types,
                        class_type
                    );

                    all_return_types.push_back(ret_type);
                }

                ObjectVector unique_return_types = type_system->remove_duplicates(
                    current_scope,
                    all_return_types
                );

                if (unique_return_types.size() == 1)
                {
                    return unique_return_types[0];
                }

                return make_object(VariantType{unique_return_types});
            },
            [&](ClassType_ptr cls) -> Object_ptr
            {
                return call_method(call, access, argument_types, cls);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Generic constraint must resolve to a class or variant type."
                );
            }
        },
        template_parameter_type->constraint_type->value
    );
}

} // namespace Wasp
