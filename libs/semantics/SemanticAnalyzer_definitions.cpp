#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ---------------------------------------------------------------------------
// Variable Definitions & Assignments
// ---------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(Assignment& expr)
{
    if (expr.is_definition)
    {
        return define_variable(expr);
    }

    if (expr.lhs->is<Identifier>())
    {
        return mutate_variable(expr.lhs, expr.rhs);
    }

    if (expr.lhs->is<MemberAccess>())
    {
        return mutate_member(expr.lhs, expr.rhs);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier or MemberAccess."
    );
    return nullptr;
}

Object_ptr SemanticAnalyzer::define_variable(Assignment& assign)
{
    Doctor::get().assert(
        assign.lhs->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of definition must be an Identifier."
    );

    std::string symbol_name = assign.lhs->as<Identifier>().name;

    Object_ptr resolved_type = visit(assign.rhs);
    resolved_type = unwrap_type_alias(resolved_type);

    if (assign.declared_type.has_value())
    {
        Object_ptr explicit_type = visit(assign.declared_type.value());

        Doctor::get().assert(
            type_system
                ->assignable(current_scope, explicit_type, resolved_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for " + symbol_name
        );

        resolved_type = explicit_type;
    }

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name))
    {
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is hoisted but already has a type!"
        );

        hoisted_symbol->set_type(resolved_type);
        assign.lhs->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        assign.is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(local_symbol);
    assign.lhs->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

void SemanticAnalyzer::validate_purity_constraints(
    Symbol_ptr target_symbol
) const
{
    if (target_symbol->closure_depth >= current_scope->get_closure_depth())
    {
        return;
    }

    auto scope = current_scope;

    while (scope != nullptr &&
           scope->get_closure_depth() > target_symbol->closure_depth)
    {
        if (scope->get_type() == ScopeType::PURE_FUNCTION ||
            scope->get_type() == ScopeType::PURE_METHOD)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Pure functions cannot mutate outer state variable '" +
                    target_symbol->name + "'"
            );
        }
        scope = scope->get_enclosing();
    }
}

Object_ptr SemanticAnalyzer::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr
)
{
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
        type_system->assignable(
            current_scope,
            target_symbol->get_type(),
            assigned_type
        ),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::mutate_member(
    Expression_ptr lhs_expr,
    Expression_ptr rhs_expr
)
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
        type_system->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Type mismatch in member assignment."
    );

    return expected_type;
}

// ---------------------------------------------------------------------------
// Others
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(TypeAliasDefinition& def)
{
    auto type_alias_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(
        type_alias_obj,
        WaspStage::Semantics,
        "Type alias symbol has no type"
    );

    auto type_alias_type = type_alias_obj->as<TypeAlias_ptr>();
    bool has_generics = !def.generics.empty();

    if (has_generics)
    {
        enter_scope(ScopeType::TEMPLATE);
        for (const auto& [name, generic_type] : type_alias_type->generics)
        {
            current_scope->define(
                SymbolFactory::create_generic(name, generic_type)
            );
        }
    }

    Object_ptr aliased_type = visit(def.ref_type);
    type_alias_type->underlying_type = aliased_type;

    if (has_generics)
    {
        leave_scope();
    }
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    int global_enum_value = 0;

    std::function<EnumType_ptr(const EnumDefinition&, const std::string&)>
        build_enum = [&](const EnumDefinition& e_def,
                         const std::string& prefix) -> EnumType_ptr
    {
        std::string current_name = prefix.empty() ? e_def.name
                                                  : prefix + "." + e_def.name;
        auto enum_type = std::make_shared<EnumType>(current_name);

        for (const auto& [name, old_val] : e_def.members)
        {
            enum_type->members[current_name + "." + name] = global_enum_value++;
        }

        for (const auto& nested_def : e_def.nested_enums)
        {
            enum_type->nested_enums
                [current_name + "." + nested_def.name] = build_enum(
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
    Object_ptr actual = statement.expression
                            ? visit(statement.expression.value())
                            : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_system->assignable(current_scope, expected, actual),
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

void SemanticAnalyzer::visit(FieldDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Fields cannot be defined outside of a class."
    );
}

bool SemanticAnalyzer::prepare_generic_scope(const ObjectStringMap& generics)
{
    if (generics.empty())
    {
        return false;
    }

    enter_scope(ScopeType::TEMPLATE);

    for (const auto& [name, generic_type] : generics)
    {
        auto symbol = SymbolFactory::create_generic(name, generic_type);
        current_scope->define(symbol);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Function
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();
    bool has_generics = prepare_generic_scope(signature->generics);

    bool parameters_are_mutable = !def.is_pure;
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    Doctor::get().assert(
        def.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty"
    );

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            signature->parameter_types[i],
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

    leave_scope(); // Function Scope
    if (has_generics)
    {
        leave_scope(); // Template Scope
    }
}

// ============================================================================
// Operator Definition
// ============================================================================

void SemanticAnalyzer::visit(OperatorDefinition& def)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();
    bool has_generics = prepare_generic_scope(signature->generics);

    // Validate parameter count based on fixity
    if (def.fixity == TokenType::INFIX)
    {
        Doctor::get().assert(
            def.parameters.size() == 2,
            WaspStage::Semantics,
            "Infix operator '" + def.name + "' must have exactly 2 parameters."
        );
    }
    else
    {
        Doctor::get().assert(
            def.parameters.size() == 1,
            WaspStage::Semantics,
            "Unary operator '" + def.name + "' must have exactly 1 parameter."
        );
    }

    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    def.parameter_symbols.clear();
    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto param_symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            signature->parameter_types[i],
            false, // Operators parameters are immutable by convention
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.parameter_symbols.push_back(current_scope->define(param_symbol));
    }

    if (def.body.size() == 1 && def.body.front()->is<Native>())
    {
        def.symbol->mark_as_native();
    }

    visit(def.body);

    return_type_stack.pop_back();
    leave_scope();

    if (has_generics)
    {
        leave_scope();
    }
}

// ---------------------------------------------------------------------------
// Classes & Traits
// ---------------------------------------------------------------------------

void SemanticAnalyzer::analyze_oop_definition(AbstractOOPDefinition& def)
{
    auto type_obj = def.symbol->get_type();
    BaseOOPType_ptr oop_type;

    if (type_obj->is<ClassType_ptr>())
    {
        oop_type = type_obj->as<ClassType_ptr>();
    }
    else
    {
        oop_type = type_obj->as<TraitType_ptr>();
    }

    bool has_generics = prepare_generic_scope(oop_type->generics);

    // --- Pass 0: Fill up type structure ---
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& f)
                {
                    Doctor::get().assert(
                        std::find(
                            oop_type->fields.begin(),
                            oop_type->fields.end(),
                            f.name
                        ) == oop_type->fields.end(),
                        WaspStage::Semantics,
                        "Duplicate field '" + f.name + "'."
                    );

                    auto field_type = visit(f.type);
                    oop_type->fields.push_back(f.name);
                    oop_type->member_types[f.name] = field_type;
                },
                [&](FunctionDefinition& f)
                {
                    if (!f.is_method)
                    {
                        return; // Only process methods here
                    }

                    auto push_unique =
                        [](StringVector& vec, const std::string& name)
                    {
                        if (std::find(vec.begin(), vec.end(), name) ==
                            vec.end())
                        {
                            vec.push_back(name);
                        }
                    };

                    push_unique(oop_type->methods, f.name);
                    if (f.is_pure)
                    {
                        push_unique(oop_type->pures, f.name);
                    }
                    if (f.is_static)
                    {
                        push_unique(oop_type->statics, f.name);
                    }
                },
                [](auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "Invalid statement in OOP body."
                    );
                }
            },
            stmt->data
        );
    }

    // --- Pass 1: Signature Hoisting ---
    for (auto& stmt : def.members)
    {
        if (auto* func_def = stmt->try_as<FunctionDefinition>())
        {
            if (!func_def->is_method)
            {
                continue;
            }

            Object_ptr return_type = func_def->return_type
                                         ? visit(func_def->return_type)
                                         : workspace->pool->get_none_type();
            ObjectVector param_types;

            for (const auto& [name, type_node] : func_def->parameters)
            {
                param_types.push_back(visit(type_node));
            }

            auto signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    return_type,
                    oop_type->generics,
                    oop_type->expected_generic_names_order
                )
            );

            // Logic matches Method creation but uses unified FunctionDefinition
            // node
            func_def->symbol = SymbolFactory::create_method(
                func_def->name,
                signature,
                false,
                current_scope->get_closure_depth(),
                current_scope->get_lexical_depth()
            );

            oop_type->add_overload(func_def->name, signature);
        }
    }

    // --- Pass 2: Body Analysis ---
    for (auto& stmt : def.members)
    {
        if (auto* func_def = stmt->try_as<FunctionDefinition>())
        {
            if (!func_def->is_method)
            {
                continue;
            }

            auto signature = func_def->symbol->get_type()->as<Signature_ptr>();
            bool has_method_generics = prepare_generic_scope(
                signature->generics
            );

            enter_scope(
                func_def->is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD
            );
            return_type_stack.push_back(signature->return_type);

            auto define_param = [&](const std::string& name, Object_ptr type)
            {
                auto symbol = SymbolFactory::create_variable(
                    name,
                    type,
                    !func_def->is_pure,
                    current_scope->get_closure_depth(),
                    current_scope->get_lexical_depth()
                );
                current_scope->define(symbol);
                func_def->parameter_symbols.push_back(symbol);
            };

            // Unified injection of 'my' or 'our'
            define_param(func_def->is_static ? "our" : "my", type_obj);

            for (size_t i = 0; i < func_def->parameters.size(); ++i)
            {
                define_param(
                    func_def->parameters[i].first,
                    signature->parameter_types[i]
                );
            }

            visit(func_def->body);

            return_type_stack.pop_back();
            leave_scope();

            if (has_method_generics)
            {
                leave_scope();
            }
        }
    }

    if (has_generics)
    {
        leave_scope();
    }
}

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    analyze_oop_definition(def);
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    analyze_oop_definition(def);
}

void SemanticAnalyzer::visit(Import& imp)
{
}

} // namespace Wasp
