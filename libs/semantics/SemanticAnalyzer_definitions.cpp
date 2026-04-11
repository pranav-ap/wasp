#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
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

void SemanticAnalyzer::analyze_membered_definition(MemberedDefinition& def, bool is_trait)
{
    ObjectStringMap member_types;
    StringVector instance_variables_declaration_order;
    StringVector class_variables_declaration_order;
    StringVector methods_declaration_order;
    std::unordered_set<std::string> shared_members;

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& field)
                {
                    member_types[field.name] = visit(field.type);

                    if (field.is_our)
                    {
                        class_variables_declaration_order.push_back(field.name);
                        shared_members.insert(field.name);
                    }
                    else
                    {
                        instance_variables_declaration_order.push_back(field.name);
                    }
                },
                [&](MyMethodDefinition& method)
                {
                    if (std::find(
                            methods_declaration_order.begin(),
                            methods_declaration_order.end(),
                            method.name
                        ) == methods_declaration_order.end())
                        methods_declaration_order.push_back(method.name);
                },
                [&](OurMethodDefinition& method)
                {
                    if (std::find(
                            methods_declaration_order.begin(),
                            methods_declaration_order.end(),
                            method.name
                        ) == methods_declaration_order.end())
                        methods_declaration_order.push_back(method.name);
                },
                [&](auto&)
                {
                    Doctor::get().fatal(WaspStage::Semantics, "Invalid statement in class body.");
                }
            },
            stmt->data
        );
    }

    // 2. Build the specific backend type
    Object_ptr target_type_obj;
    std::shared_ptr<ClassType> class_type = nullptr;

    if (is_trait)
    {
        Doctor::get().fatal(WaspStage::Semantics, "TraitType backend not yet implemented!");
    }
    else
    {
        class_type = std::make_shared<ClassType>(
            def.name,
            std::move(member_types),
            std::move(instance_variables_declaration_order),
            std::move(class_variables_declaration_order),
            std::move(methods_declaration_order),
            std::move(shared_members)
        );
        target_type_obj = make_object(class_type);
    }

    Doctor::get().fatal_if_nullptr(def.symbol, WaspStage::Semantics);
    def.symbol->set_type(target_type_obj);
    class_type_stack.push_back(target_type_obj);

    // Inline Hoisting Engine
    auto hoist_method = [&](AbstractFunctionDefinition& method_def, bool is_our)
    {
        std::string original_name = method_def.name;
        method_def.name = def.name + "::" + original_name;

        auto [ret_type, param_types] = evaluate_function_signature(method_def);
        auto signature = make_object(std::make_shared<FunctionType>(param_types, ret_type));

        Symbol_ptr method_symbol = is_our ? SymbolFactory::create_our_method(
                                                method_def.name,
                                                signature,
                                                target_type_obj,
                                                false,
                                                current_scope->get_closure_depth(),
                                                current_scope->get_lexical_depth()
                                            )
                                          : SymbolFactory::create_my_method(
                                                method_def.name,
                                                signature,
                                                target_type_obj,
                                                false,
                                                current_scope->get_closure_depth(),
                                                current_scope->get_lexical_depth()
                                            );

        if (current_scope->contains_in_current_scope(method_def.name))
        {
            type_checker->validate_overload_group(current_scope, method_def.name, method_symbol);
        }

        current_scope->define(method_symbol);
        method_def.symbol = method_symbol;
        method_def.group_symbol = current_scope->lookup(method_def.name);

        if (!is_trait && class_type)
        {
            class_type->members[original_name] = method_def.group_symbol->get_type();
        }
    };

    // 3. Hoist Pass
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& m)
                {
                    hoist_method(m, false);
                },
                [&](OurMethodDefinition& m)
                {
                    hoist_method(m, true);
                },
                [&](auto&) { /* Fields already handled */ }
            },
            stmt->data
        );
    }

    // 4. Analysis Pass
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& m)
                {
                    visit(m);
                },
                [&](OurMethodDefinition& m)
                {
                    visit(m);
                },
                [&](auto&) { /* Fields have no body */ }
            },
            stmt->data
        );
    }

    class_type_stack.pop_back();
}

void SemanticAnalyzer::visit(ClassDefinition& class_def)
{
    analyze_membered_definition(class_def, false);
}

void SemanticAnalyzer::visit(TraitDefinition& trait_def)
{
    analyze_membered_definition(trait_def, true);
}

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::evaluate_function_signature(
    AbstractFunctionDefinition& func
)
{
    Object_ptr return_type = func.return_type ? visit(func.return_type)
                                              : workspace->pool->get_none_type();
    ObjectVector param_types;

    for (const auto& [param_name, type_ann] : func.parameters)
    {
        param_types.push_back(type_ann ? visit(type_ann) : workspace->pool->get_any_type());
    }

    return {return_type, param_types};
}

void SemanticAnalyzer::hoist_function_body(
    AbstractFunctionDefinition& method_def,
    bool is_our,
    const std::string& class_name,
    std::shared_ptr<ClassType>& class_type
)
{
    std::string original_name = method_def.name;
    method_def.name = class_name + "::" + original_name;

    auto [ret_type, param_types] = evaluate_function_signature(method_def);
    auto signature = make_object(std::make_shared<FunctionType>(param_types, ret_type));

    Object_ptr current_class = class_type_stack.back();

    Symbol_ptr method_symbol;
    if (is_our)
    {
        method_symbol = SymbolFactory::create_our_method(
            method_def.name,
            signature,
            current_class,
            false,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );
    }
    else
    {
        method_symbol = SymbolFactory::create_my_method(
            method_def.name,
            signature,
            current_class,
            false,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );
    }

    if (current_scope->contains_in_current_scope(method_def.name))
    {
        type_checker->validate_overload_group(current_scope, method_def.name, method_symbol);
    }

    current_scope->define(method_symbol);
    method_def.symbol = method_symbol;
    method_def.group_symbol = current_scope->lookup(method_def.name);

    if (!class_type->contains_member(original_name))
    {
        class_type->methods_declaration_order.push_back(original_name);
    }

    class_type->members[original_name] = method_def.group_symbol->get_type();
}

void SemanticAnalyzer::analyze_abstract_function_body(
    AbstractFunctionDefinition& fun_def,
    bool inject_my,
    bool inject_our
)
{
    Object_ptr return_type;
    ObjectVector param_types;

    if (fun_def.symbol->get_type() == nullptr)
    {
        // Top-level function (SymbolHoister left the type as nullptr)
        auto evaluated = evaluate_function_signature(fun_def);
        return_type = evaluated.first;
        param_types = evaluated.second;

        auto signature = make_object(std::make_shared<FunctionType>(param_types, return_type));
        fun_def.symbol->set_type(signature);

        fun_def.group_symbol = current_scope->lookup(fun_def.name);
    }
    else
    {
        // Impl method or local function (already evaluated!)
        auto signature = fun_def.symbol->get_type()->as<std::shared_ptr<FunctionType>>();
        return_type = signature->return_type.has_value() ? signature->return_type.value()
                                                         : workspace->pool->get_none_type();
        param_types = signature->input_types;
    }

    type_checker->validate_overload_group(current_scope, fun_def.name, fun_def.symbol);
    Doctor::get().fatal_if_nullptr(fun_def.group_symbol, WaspStage::Semantics);

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);
    fun_def.parameter_symbols.clear();

    auto define_param = [&](const std::string& name, Object_ptr type, bool is_mutable)
    {
        auto sym = SymbolFactory::create_variable(
            name,
            type,
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(sym);
        fun_def.parameter_symbols.push_back(sym);
    };

    if (!class_type_stack.empty())
    {
        Object_ptr current_class = class_type_stack.back();

        if (inject_my)
            define_param("my", current_class, false);
        if (inject_our)
            define_param("our", current_class, false);
    }

    for (size_t i = 0; i < fun_def.parameters.size(); ++i)
    {
        define_param(fun_def.parameters[i].first, param_types[i], true);
    }

    // Mask class context for nested functions
    class_type_stack.push_back(nullptr);

    hoist_statements(fun_def.body);

    for (auto& stmt : fun_def.body)
    {
        visit(stmt);
    }

    class_type_stack.pop_back();
    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::visit(FunctionDefinition& fun_def)
{
    analyze_abstract_function_body(fun_def, false, false);
}

void SemanticAnalyzer::visit(MyMethodDefinition& method_def)
{
    analyze_abstract_function_body(method_def, true, true);
}

void SemanticAnalyzer::visit(OurMethodDefinition& method_def)
{
    analyze_abstract_function_body(method_def, false, true);
}

// -------------------------------------------------------------------
// Other Visitors
// -------------------------------------------------------------------

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : make_object(NoneType());

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
}

void SemanticAnalyzer::visit(AliasDefinition& statement)
{
}
void SemanticAnalyzer::visit(EnumDefinition& statement)
{
}
void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

} // namespace Wasp
