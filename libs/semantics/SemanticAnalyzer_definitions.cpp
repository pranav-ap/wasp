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
#include <tuple>
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

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
}

// ============================================================================
// CLASS
// ============================================================================

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = initialize_class_type(def);

    auto class_type_obj = make_object(class_type);
    def.symbol->set_type(class_type_obj);

    // Hoist methods

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    auto [return_type, parameter_types] = get_function_signature(m);

                    Object_ptr signature = make_object(
                        std::make_shared<MethodType>(parameter_types, return_type)
                    );

                    Symbol_ptr symbol = SymbolFactory::create_method(
                        m.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    auto overloads = class_type->get_overloads(m.name);

                    type_checker->validate_new_method_overload(current_scope, overloads, symbol);

                    class_type->add_overload(m.name, signature);
                    m.symbol = symbol;
                },
                [&](PureMethodDefinition& m)
                {
                    auto [return_type, parameter_types] = get_function_signature(m);

                    Object_ptr signature = make_object(
                        std::make_shared<MethodType>(parameter_types, return_type)
                    );

                    Symbol_ptr symbol = SymbolFactory::create_method(
                        m.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    auto overloads = class_type->get_overloads(m.name);

                    type_checker->validate_new_method_overload(current_scope, overloads, symbol);

                    class_type->add_overload(m.name, signature);
                    m.symbol = symbol;
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }

    // Analyse method bodies

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    enter_scope(ScopeType::METHOD);

                    auto signature = m.symbol->get_type();
                    auto [return_type, param_types] = get_function_signature(signature);
                    return_type_stack.push_back(return_type);

                    Doctor::get().assert(
                        m.parameter_symbols.empty(),
                        WaspStage::Semantics,
                        "Expect parameter symbols to be empty at this stage"
                    );

                    auto define_param =
                        [&](const std::string& name, Object_ptr type, bool is_mutable)
                    {
                        auto sym = SymbolFactory::create_variable(
                            name,
                            type,
                            is_mutable,
                            current_scope->get_closure_depth(),
                            current_scope->get_lexical_depth()
                        );

                        current_scope->define(sym);
                        m.parameter_symbols.push_back(sym);
                    };

                    define_param("my", class_type_obj, false);

                    for (size_t i = 0; i < m.parameters.size(); ++i)
                    {
                        define_param(m.parameters[i].first, param_types[i], true);
                    }

                    // Analyze Body

                    visit(m.body);
                    return_type_stack.pop_back();

                    leave_scope();
                },
                [&](PureMethodDefinition& m)
                {
                    enter_scope(ScopeType::PURE_METHOD);

                    auto signature = m.symbol->get_type();
                    auto [return_type, param_types] = get_function_signature(signature);
                    return_type_stack.push_back(return_type);

                    Doctor::get().assert(
                        m.parameter_symbols.empty(),
                        WaspStage::Semantics,
                        "Expect parameter symbols to be empty at this stage"
                    );

                    auto define_param =
                        [&](const std::string& name, Object_ptr type, bool is_mutable)
                    {
                        auto sym = SymbolFactory::create_variable(
                            name,
                            type,
                            is_mutable,
                            current_scope->get_closure_depth(),
                            current_scope->get_lexical_depth()
                        );

                        current_scope->define(sym);
                        m.parameter_symbols.push_back(sym);
                    };

                    for (size_t i = 0; i < m.parameters.size(); ++i)
                    {
                        define_param(m.parameters[i].first, param_types[i], true);
                    }

                    // Analyze Body

                    visit(m.body);
                    return_type_stack.pop_back();

                    leave_scope();
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

std::shared_ptr<ClassType> SemanticAnalyzer::initialize_class_type(ClassDefinition& def)
{
    ObjectStringMap members;
    StringVector fields;
    StringVector methods;
    StringVector pures;

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& f)
                {
                    auto field_type = visit(f.type);

                    Doctor::get().assert(
                        std::find(fields.begin(), fields.end(), f.name) == fields.end(),
                        WaspStage::Semantics,
                        "Duplicate field name " + f.name + " in class " + def.name
                    );

                    fields.push_back(f.name);
                    members[f.name] = field_type;
                },
                [&](MethodDefinition& m)
                {
                    if (std::find(methods.begin(), methods.end(), m.name) == methods.end())
                    {
                        methods.push_back(m.name);
                        members[m.name] = make_object(std::make_shared<ObjectOverloadList>(m.name));
                    }
                },
                [&](PureMethodDefinition& p)
                {
                    if (std::find(pures.begin(), pures.end(), p.name) == methods.end())
                    {
                        pures.push_back(p.name);
                        members[p.name] = make_object(std::make_shared<ObjectOverloadList>(p.name));
                    }
                },
                [&](auto&)
                {
                    Doctor::get().fatal(WaspStage::Semantics, "Invalid statement in class body.");
                }
            },
            stmt->data
        );
    }

    return std::make_shared<ClassType>(
        def.name,
        std::move(members),
        std::move(fields),
        std::move(methods),
        std::move(pures)
    );
}

// ============================================================================
// FUNCTIONS
// ============================================================================

template <typename T>
void SemanticAnalyzer::analyze_function_base(T& def, ScopeType scope_type, bool is_mutable)
{
    enter_scope(scope_type);

    auto signature = def.symbol->get_type();
    auto [return_type, parameter_types] = get_function_signature(signature);
    return_type_stack.push_back(return_type);

    Doctor::get().assert(
        def.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty at this stage"
    );

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            parameter_types[i],
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        def.parameter_symbols.push_back(symbol);
    }

    visit(def.body);
    return_type_stack.pop_back();

    leave_scope();
}

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    analyze_function_base(def, ScopeType::FUNCTION, true);
}

void SemanticAnalyzer::visit(PureFunctionDefinition& def)
{
    analyze_function_base(def, ScopeType::PURE_FUNCTION, false);
}

// -------------------------------------------------------------------
// Other Visitors
// -------------------------------------------------------------------

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
