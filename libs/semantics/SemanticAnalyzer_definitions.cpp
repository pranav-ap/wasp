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

void SemanticAnalyzer::visit(FunctionDefinition& func)
{
    auto [return_type, param_types] = evaluate_signature(func);
    auto signature = make_object(std::make_shared<FunctionType>(param_types, return_type));

    Object_ptr current_class = (!class_type_stack.empty()) ? class_type_stack.back() : nullptr;

    if (!func.symbol)
    {
        // Pass the class and the flag directly to the factory!
        func.symbol = SymbolFactory::create_function(
            func.name,
            signature,
            false,
            current_class,
            func.is_our,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        if (current_scope->contains_in_current_scope(func.name))
        {
            type_checker->validate_overload_group(current_scope, func.name, func.symbol);
        }

        current_scope->define(func.symbol);
    }
    else
    {
        func.symbol->set_type(signature);

        // Update the clean, simplified FunctionData payload
        auto& fd = func.symbol->get_payload_as<FunctionData>();
        fd.bound_class = current_class;
        fd.is_our = func.is_our;

        type_checker->validate_overload_group(current_scope, func.name, func.symbol);
    }

    func.group_symbol = current_scope->lookup(func.name);
    Doctor::get().fatal_if_nullptr(func.group_symbol, WaspStage::Semantics);

    // Prepare Scope
    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);
    func.parameter_symbols.clear();

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
        func.parameter_symbols.push_back(sym);
    };

    // -------------------------------------------------------------------
    // Inject parameters cleanly based on the streamlined state
    // -------------------------------------------------------------------
    if (current_class)
    {
        if (!func.is_our)
        {
            define_param("my", current_class, false); // Slot 0
        }
        define_param("our", current_class, false); // Slot 1 (instance) or Slot 0 (class)
    }

    for (size_t i = 0; i < func.parameters.size(); ++i)
    {
        define_param(func.parameters[i].first, param_types[i], true);
    }

    class_type_stack.push_back(nullptr);

    hoist_statements(func.body);

    for (auto& stmt : func.body)
    {
        visit(stmt);
    }

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

// ============================================================================
// Stubs
// ============================================================================

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
