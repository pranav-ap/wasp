#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
// ---------------------------------------------------------------------------
// Core Semantic Checks & Utilities
// ---------------------------------------------------------------------------

void SemanticAnalyzer::validate_purity_constraints(Symbol_ptr target_symbol) const
{
    if (target_symbol->closure_depth >= current_scope->get_closure_depth())
    {
        return;
    }

    auto scope = current_scope;

    while (scope != nullptr && scope->get_closure_depth() > target_symbol->closure_depth)
    {
        if (scope->get_type() == ScopeType::PURE_FUNCTION ||
            scope->get_type() == ScopeType::PURE_METHOD)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Pure functions cannot mutate outer state variable '" + target_symbol->name + "'"
            );
        }

        scope = scope->get_enclosing();
    }
}

// ---------------------------------------------------------------------------
// Variable Definitions & Assignments
// ---------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::define_variable(Expression_ptr assignment_node, bool is_mutable)
{
    Expression_ptr identifier_expr = nullptr;
    Expression_ptr rhs_expr = nullptr;
    Object_ptr declared_type = nullptr;

    std::visit(
        overloaded{
            [&](UntypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
            },
            [&](TypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
                declared_type = visit(assign.type_node);
            },
            [](auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid variable definition expression");
            }
        },
        assignment_node->data
    );

    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of definition must be an Identifier."
    );

    std::string symbol_name = identifier_expr->as<Identifier>().name;

    Object_ptr initializer_type = visit(rhs_expr);
    Object_ptr resolved_type = initializer_type;

    if (declared_type)
    {
        Doctor::get().assert(
            type_checker->assignable(current_scope, declared_type, initializer_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for " + symbol_name
        );

        resolved_type = declared_type;
    }
    else
    {
        if (initializer_type->is<IntLiteralType>())
        {
            resolved_type = workspace->pool->get_int_type();
        }
        else if (initializer_type->is<FloatLiteralType>())
        {
            resolved_type = workspace->pool->get_float_type();
        }
        else if (initializer_type->is<StringLiteralType>())
        {
            resolved_type = workspace->pool->get_string_type();
        }
        else if (initializer_type->is<BooleanLiteralType>())
        {
            resolved_type = workspace->pool->get_boolean_type();
        }
    }

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name))
    {
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is hoisted but already has a type!"
        );

        hoisted_symbol->set_type(resolved_type);
        identifier_expr->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(local_symbol);
    identifier_expr->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

Object_ptr SemanticAnalyzer::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr
)
{
    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier."
    );

    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::get().fatal_if_nullptr(
        target_symbol,
        WaspStage::Semantics,
        "Cannot assign to undefined variable '" + symbol_name + "'"
    );

    Doctor::get().assert(
        target_symbol->payload_is<VariableData>(),
        WaspStage::Semantics,
        "Cannot assign to non-variable symbol '" + symbol_name + "'"
    );

    auto& var_data = target_symbol->get_payload_as<VariableData>();

    Doctor::get().assert(
        var_data.is_mutable,
        WaspStage::Semantics,
        "Cannot reassign immutable variable '" + symbol_name + "'"
    );

    validate_purity_constraints(target_symbol);

    identifier_node.symbol = target_symbol;

    Object_ptr assigned_type = visit(assigned_expr);

    Doctor::get().assert(
        type_checker->assignable(current_scope, target_symbol->get_type(), assigned_type),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr)
{
    auto& mac = lhs_expr->as<MemberAccess>();

    Object_ptr expected_type = visit(mac);

    if (mac.member_index == -1)
    {
        auto symbol = mac.right->as<Identifier>().symbol;
        if (symbol && symbol->payload_is<VariableData>())
        {
            Doctor::get().assert(
                symbol->get_payload_as<VariableData>().is_mutable,
                WaspStage::Semantics,
                "Cannot reassign immutable shared member: " + symbol->name
            );
        }
    }

    Object_ptr actual_type = visit(rhs_expr);

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Type mismatch in member assignment."
    );

    return expected_type;
}

void SemanticAnalyzer::visit(VariableDefinition& statement)
{
    define_variable(statement.expression, statement.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr)
{
    return define_variable(expr.assignment, expr.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    if (expr.lhs_expression->is<Identifier>())
    {
        return mutate_variable(expr.lhs_expression, expr.rhs_expression);
    }

    if (expr.lhs_expression->is<MemberAccess>())
    {
        return mutate_member(expr.lhs_expression, expr.rhs_expression);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier or MemberAccess."
    );
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "TypedAssignment cannot be visited directly");
}

// ---------------------------------------------------------------------------
// Functions & Methods
// ---------------------------------------------------------------------------

void SemanticAnalyzer::analyze_function(
    FunctionDefinition& def,
    ScopeType scope_type,
    bool parameters_are_mutable
)
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
            parameters_are_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        def.parameter_symbols.push_back(symbol);
    }

    if (def.body.size() == 1 && def.body.front()->is<Native>())
    {
        def.symbol->mark_as_native();
    }

    visit(def.body);
    return_type_stack.pop_back();

    leave_scope();
}

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    ScopeType scope = def.is_pure ? ScopeType::PURE_FUNCTION : ScopeType::FUNCTION;
    bool parameters_are_mutable = !def.is_pure;

    analyze_function(def, scope, parameters_are_mutable);
}

// ---------------------------------------------------------------------------
// Classes & Traits
// ---------------------------------------------------------------------------

void SemanticAnalyzer::analyze_membered_type(ClassDefinition& def, ClassType_ptr class_type)
{
    auto class_type_obj = make_object(class_type);

    // --- Pass 1: Hoisting ---
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    auto [return_type, parameter_types] = get_function_signature(m);

                    Object_ptr signature = make_object(
                        std::make_shared<Signature>(Signature{parameter_types, return_type})
                    );

                    Symbol_ptr symbol = SymbolFactory::create_method(
                        m.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    type_checker->validate_new_method_overload(
                        current_scope,
                        class_type->get_overloads(m.name),
                        symbol
                    );

                    class_type->add_overload(m.name, signature);
                    m.symbol = symbol;
                },
                [](auto&)
                {
                }
            },
            stmt->data
        );
    }

    // --- Pass 2: Analysis ---
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    ScopeType scope = m.is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;
                    bool is_mutable = !m.is_pure;

                    enter_scope(scope);

                    auto signature = m.symbol->get_type();
                    auto [return_type, param_types] = get_function_signature(signature);
                    return_type_stack.push_back(return_type);

                    auto define_param = [&](const std::string& name, Object_ptr type)
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

                    if (class_type_obj)
                    {
                        std::string self_name = m.is_static ? "our" : "my";
                        define_param(self_name, class_type_obj);
                    }

                    for (size_t i = 0; i < m.parameters.size(); ++i)
                    {
                        define_param(m.parameters[i].first, param_types[i]);
                    }

                    if (m.body.size() == 1 && m.body.front()->is<Native>())
                    {
                        m.symbol->mark_as_native();
                    }

                    visit(m.body);

                    return_type_stack.pop_back();
                    leave_scope();
                },
                [](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

ClassType_ptr SemanticAnalyzer::initialize_class_type(ClassDefinition& def)
{
    ObjectStringMap members;
    StringVector fields;
    StringVector methods;
    StringVector pures;
    StringVector statics;

    auto add_method = [&](const std::string& name, StringVector& target_vec)
    {
        if (std::find(target_vec.begin(), target_vec.end(), name) == target_vec.end())
        {
            target_vec.push_back(name);
            members[name] = make_object(std::make_shared<ObjectOverloadList>());
        }
    };

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
                        "Duplicate field name " + f.name
                    );

                    fields.push_back(f.name);
                    members[f.name] = field_type;
                },
                [&](MethodDefinition& m)
                {
                    if (m.is_pure)
                    {
                        add_method(m.name, pures);
                    }
                    else
                    {
                        add_method(m.name, methods);
                    }

                    if (m.is_static)
                    {
                        if (std::find(statics.begin(), statics.end(), m.name) == statics.end())
                        {
                            statics.push_back(m.name);
                        }
                    }
                },
                [](auto&)
                {
                    Doctor::get().fatal(WaspStage::Semantics, "Invalid statement in body.");
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
        std::move(pures),
        std::move(statics)
    );
}

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = initialize_class_type(def);
    def.symbol->set_type(make_object(class_type));
    analyze_membered_type(def, class_type);
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    Doctor::get().fatal(WaspStage::Semantics, "Trait definitions are not supported for now");
}

// ---------------------------------------------------------------------------
// Templates
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(TemplateDefinition& template_def)
{
    enter_scope(ScopeType::TEMPLATE);

    auto template_type = template_def.symbol->get_type()->as<TemplateType_ptr>();

    for (auto& field : template_def.members)
    {
        auto generic_type_obj = template_type->generics.at(field.name);
        auto symbol = SymbolFactory::create_generic(field.name, generic_type_obj);
        field.symbol = current_scope->define(symbol);
    }

    std::visit(
        overloaded{
            [&](FunctionDefinition& def)
            {
                visit(def);
            },
            [&](ClassDefinition& def)
            {
                visit(def);
            },
            [&](TraitDefinition& def)
            {
                visit(def);
            },
            [&](TypeAliasDefinition& def)
            {
                visit(def);
            },
            [&](auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid template target");
            }
        },
        template_def.target->data
    );

    leave_scope();
}

// ---------------------------------------------------------------------------
// Others
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(TypeAliasDefinition& def)
{
    Object_ptr aliased_type = visit(def.ref_type);

    def.symbol->set_type(
        make_object(std::make_shared<TypeAlias>(TypeAlias{def.name, aliased_type}))
    );
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    int global_enum_value = 0;

    std::function<EnumType_ptr(const EnumDefinition&, const std::string&)> build_enum =
        [&](const EnumDefinition& e_def, const std::string& prefix) -> EnumType_ptr
    {
        std::string current_name = prefix.empty() ? e_def.name : prefix + "." + e_def.name;
        auto enum_type = std::make_shared<EnumType>(current_name);

        for (const auto& [name, old_val] : e_def.members)
        {
            enum_type->members[current_name + "." + name] = global_enum_value++;
        }

        for (const auto& nested_def : e_def.nested_enums)
        {
            enum_type->nested_enums[current_name + "." + nested_def.name] = build_enum(
                nested_def,
                current_name
            );
        }

        return enum_type;
    };

    auto enum_type = build_enum(def, "");
    def.symbol->set_type(make_object(enum_type));
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
        "Return type mismatch"
    );
}

void SemanticAnalyzer::visit(Native& statement)
{
    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    std::string path = current_module->absolute_filepath.generic_string();

    Doctor::get().assert(
        path.find("/libs/core/") != std::string::npos,
        WaspStage::Semantics,
        "The 'native' keyword is strictly reserved for internal core libraries."
    );
}

void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

// ---------------------------------------------------------------------------
// Cannot Visit
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(FieldDefinition& stat)
{
    Doctor::get().fatal(WaspStage::Semantics, "Fields cannot be defined outside of a class.");
}

void SemanticAnalyzer::visit(MethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

} // namespace Wasp
