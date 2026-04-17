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

    auto class_symbol = SymbolFactory::create_class(
        def.name,
        make_object(class_type),
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    // Hoist methods

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    auto [return_type, parameter_types] = extract_function_signature(m);

                    Object_ptr signature = make_object(
                        std::make_shared<MethodType>(parameter_types, return_type)
                    );

                    Symbol_ptr method_symbol = SymbolFactory::create_method(
                        def.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    auto overloads = class_type->get_overloads(m.name);

                    type_checker
                        ->validate_new_method_overload(current_scope, overloads, method_symbol);

                    class_type->add_overload(m.name, signature);
                    def.symbol = method_symbol;
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

    def.symbol->set_type(class_symbol->get_type());
    current_scope->define(class_symbol);
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
    auto signature = def.symbol->get_type();
    analyze_abstract_function_body(def, false, signature);
}

void SemanticAnalyzer::visit(MethodDefinition& def)
{
    auto signature = def.symbol->get_type();
    analyze_abstract_function_body(def, true, signature);
}

void SemanticAnalyzer::analyze_abstract_function_body(
    AbstractFunctionDefinition& fun_def,
    bool is_method,
    Object_ptr class_type
)
{
    Object_ptr return_type;
    ObjectVector param_types;

    if (fun_def.symbol->get_type() == nullptr)
    {
        // Top-level function (SymbolHoister left the type as nullptr)
        std::tie(return_type, param_types) = extract_function_signature(fun_def);
        auto signature = make_object(std::make_shared<FunctionType>(param_types, return_type));
        fun_def.symbol->set_type(signature);
    }
    else
    {
        // Impl method or local function (already evaluated, extract types cleanly)
        std::tie(return_type, param_types) = get_function_signature(fun_def.symbol->get_type());
    }

    type_checker->validate_new_function_overload(current_scope, fun_def.name, fun_def.symbol);

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

    if (is_method)
    {
        define_param("my", class_type, false);
    }

    for (size_t i = 0; i < fun_def.parameters.size(); ++i)
    {
        define_param(fun_def.parameters[i].first, param_types[i], true);
    }

    // Analyze Body

    visit(fun_def.body);
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
