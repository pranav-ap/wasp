#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeAnnotation.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
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

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::evaluate_function_signature(
    AbstractFunctionDefinition& func
)
{
    Object_ptr return_type = func.return_type ? visit(func.return_type) : make_object(NoneType());
    ObjectVector param_types;

    for (const auto& [param_name, type_ann] : func.parameters)
    {
        param_types.push_back(type_ann ? visit(type_ann) : make_object(AnyType()));
    }

    return {return_type, param_types};
}

// ============================================================================
// Definitions
// ============================================================================

void SemanticAnalyzer::visit(ClassDefinition& class_def)
{
    std::vector<std::pair<std::string, MemberInfo>> ordered_members(
        class_def.members.begin(),
        class_def.members.end()
    );

    std::sort(
        ordered_members.begin(),
        ordered_members.end(),
        [](const auto& a, const auto& b)
        {
            return a.second.declaration_rank < b.second.declaration_rank;
        }
    );

    ObjectStringMap member_types;
    StringVector values_declaration_order;
    StringVector is_ours;

    for (const auto& [name, info] : ordered_members)
    {
        member_types[name] = visit(info.type);
        values_declaration_order.push_back(name);

        if (info.is_our)
        {
            is_ours.push_back(name);
        }
    }

    auto class_type = make_object(
        std::make_shared<ClassType>(
            class_def.name,
            std::move(member_types),
            std::move(values_declaration_order),
            std::move(is_ours)
        )
    );

    Doctor::get().fatal_if_nullptr(class_def.symbol, WaspStage::Semantics);
    class_def.symbol->set_type(class_type);
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

    auto method_symbol = SymbolFactory::create_function(
        method_def.name,
        signature,
        false,
        is_our,
        current_class,
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

    if (!class_type->contains_member(original_name))
    {
        class_type->methods_declaration_order.push_back(original_name);
    }

    class_type->members[original_name] = method_def.group_symbol->get_type();
}

void SemanticAnalyzer::visit(ImplDefinition& impl_def)
{
    Symbol_ptr class_symbol = current_scope->lookup(impl_def.class_name);

    Doctor::get().assert(
        class_symbol && class_symbol->payload_is<ClassData>(),
        WaspStage::Semantics,
        "Impl block target '" + impl_def.class_name + "' is not a defined class"
    );

    auto class_type_obj = class_symbol->get_type();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    class_type_stack.push_back(class_type_obj);

    // -------------------------------------------------------------------
    // PASS 1: Hoisting
    // -------------------------------------------------------------------
    for (auto& stmt : impl_def.methods)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& method_def)
                {
                    process_method_hoisting(method_def, false, impl_def.class_name, class_type);
                },
                [&](OurMethodDefinition& method_def)
                {
                    process_method_hoisting(method_def, true, impl_def.class_name, class_type);
                },
                [&](auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "Impl blocks can only contain method definitions."
                    );
                }
            },
            stmt->data
        );
    }

    // -------------------------------------------------------------------
    // PASS 2: Methods Analysis
    // -------------------------------------------------------------------
    for (auto& method_stmt : impl_def.methods)
    {
        visit(method_stmt);
    }

    class_type_stack.pop_back();
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

void SemanticAnalyzer::visit(TraitDefinition& statement)
{
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
