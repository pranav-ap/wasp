#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cstddef>
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

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::evaluate_signature(FunctionDefinition& func)
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
    ObjectStringMap member_types;

    for (const auto& [name, type_ann] : class_def.members)
    {
        member_types[name] = visit(type_ann);
    }

    auto class_type = make_object(
        std::make_shared<ClassType>(
            class_def.name,
            std::move(member_types),
            class_def.members_declaration_order
        )
    );

    Doctor::get().fatal_if_nullptr(
        class_def.symbol,
        WaspStage::Semantics,
        "Class definition missing symbol even after hoisting was done"
    );

    class_def.symbol->set_type(class_type);
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
        Doctor::get().assert(
            stmt->is<FunctionDefinition>(),
            WaspStage::Semantics,
            "Impl blocks can only contain function definitions."
        );

        auto& method_def = stmt->as<FunctionDefinition>();

        std::string original_name = method_def.name;
        method_def.name = impl_def.class_name + "::" + original_name;

        auto [ret_type, param_types] = evaluate_signature(method_def);
        auto signature = make_object(std::make_shared<FunctionType>(param_types, ret_type));

        Object_ptr current_class = class_type_stack.back();

        auto method_symbol = SymbolFactory::create_function(
            method_def.name,
            signature,
            false,
            method_def.is_our,
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

    // -------------------------------------------------------------------
    // PASS 2: Methods Analysis
    // -------------------------------------------------------------------
    for (auto& method_stmt : impl_def.methods)
    {
        visit(method_stmt);
    }

    class_type_stack.pop_back();
}

void SemanticAnalyzer::visit(FunctionDefinition& fun_def)
{
    Object_ptr return_type;
    ObjectVector param_types;

    if (fun_def.symbol->get_type() == nullptr)
    {
        // Top-level function (SymbolHoister left the type as nullptr)
        auto evaluated = evaluate_signature(fun_def);
        return_type = evaluated.first;
        param_types = evaluated.second;

        auto signature = make_object(std::make_shared<FunctionType>(param_types, return_type));
        fun_def.symbol->set_type(signature);

        fun_def.group_symbol = current_scope->lookup(fun_def.name);
    }
    else
    {
        // Impl method or local function (hoist_statements already evaluated this)
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

    Object_ptr current_class = (!class_type_stack.empty()) ? class_type_stack.back() : nullptr;

    // Inject 'my' and 'our' if this is a method

    if (current_class)
    {
        if (!fun_def.is_our)
        {
            define_param("my", current_class, false);
        }

        define_param("our", current_class, false);
    }

    // Inject Explicit Parameters

    for (size_t i = 0; i < fun_def.parameters.size(); ++i)
    {
        define_param(fun_def.parameters[i].first, param_types[i], true);
    }

    // Evaluate Body

    // Mask class context for nested functions
    class_type_stack.push_back(nullptr);

    hoist_statements(fun_def.body);

    for (auto& stmt : fun_def.body)
    {
        visit(stmt);
    }

    // Cleanup
    class_type_stack.pop_back();
    return_type_stack.pop_back();

    leave_scope();
}

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
