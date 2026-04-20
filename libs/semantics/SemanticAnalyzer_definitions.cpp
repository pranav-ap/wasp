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

// ============================================================================
// CLASS
// ============================================================================

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = initialize_class_type(def);

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
                        def.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    auto overloads = class_type->get_overloads(m.name);

                    type_checker->validate_new_method_overload(current_scope, overloads, symbol);

                    class_type->add_overload(m.name, signature);
                    def.symbol = symbol;
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
                    visit(m);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }

    def.symbol->set_type(make_object(class_type));
}

std::shared_ptr<ClassType> SemanticAnalyzer::initialize_class_type(ClassDefinition& def)
{
    ObjectStringMap members;
    StringVector fields;
    StringVector methods;

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& field)
                {
                    auto field_type = visit(field.type);

                    Doctor::get().assert(
                        std::find(fields.begin(), fields.end(), field.name) == fields.end(),
                        WaspStage::Semantics,
                        "Duplicate field name " + field.name + " in class " + def.name
                    );

                    fields.push_back(field.name);
                    members[field.name] = field_type;
                },
                [&](MethodDefinition& method)
                {
                    if (std::find(methods.begin(), methods.end(), method.name) == methods.end())
                    {
                        methods.push_back(method.name);
                        members[method.name] = make_object(std::shared_ptr<ObjectOverloadList>());
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
        std::move(methods)
    );
}

// ============================================================================
// FUNCTIONS
// ============================================================================

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto signature = def.symbol->get_type();
    auto [return_type, param_types] = get_function_signature(signature);
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
            param_types[i],
            true,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        def.parameter_symbols.push_back(symbol);
    }

    // Analyze Body

    visit(def.body);
    return_type_stack.pop_back();

    leave_scope();
}

void SemanticAnalyzer::visit(MethodDefinition& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto signature = def.symbol->get_type();
    auto [return_type, param_types] = get_function_signature(signature);
    return_type_stack.push_back(return_type);

    Doctor::get().assert(
        def.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty at this stage"
    );

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
        def.parameter_symbols.push_back(sym);
    };

    define_param("my", signature, false);

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        define_param(def.parameters[i].first, param_types[i], true);
    }

    // Analyze Body

    visit(def.body);
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
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
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
