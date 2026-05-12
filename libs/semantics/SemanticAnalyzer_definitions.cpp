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

void SemanticAnalyzer::visit(Placeholder& statement)
{
    if (statement.type == TokenType::NATIVE)
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
            "The 'native' keyword is strictly reserved for internal core "
            "libraries."
        );
    }
    else if (statement.type == TokenType::REQUIRED)
    {
        // Add check here if needed to ensure this is inside a TraitDefinition
        // body
    }

    // PASS naturally does nothing.
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
// Callables
// ---------------------------------------------------------------------------

void SemanticAnalyzer::analyze_callable(
    AbstractCallable& def,
    ScopeType scope_type,
    Object_ptr context_type,
    bool is_static
)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();
    bool has_generics = prepare_generic_scope(signature->generics);

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    // 1. Bind Context ('my' or 'our') for Methods
    if (context_type)
    {
        auto context_sym = SymbolFactory::create_variable(
            is_static ? "our" : "my",
            context_type,
            !def.is_pure, // Context is immutable in pure methods
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );
        current_scope->define(context_sym);
    }

    // 2. Bind Parameters
    def.parameter_symbols.clear();
    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto param_symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            signature->parameter_types[i],
            !def.is_pure, // Pure callables keep parameters immutable
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );
        def.parameter_symbols.push_back(current_scope->define(param_symbol));
    }

    // 3. Handle Native Marking & Body Analysis
    if (def.body.size() == 1 && def.body.front()->is<Placeholder>())
    {
        if (def.body.front()->as<Placeholder>().type == TokenType::NATIVE)
        {
            def.symbol->mark_as_native();
        }
    }

    visit(def.body);

    // 4. Cleanup
    return_type_stack.pop_back();
    leave_scope();
    if (has_generics)
    {
        leave_scope();
    }
}

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    analyze_callable(
        def,
        def.is_pure ? ScopeType::PURE_FUNCTION : ScopeType::FUNCTION,
        nullptr,
        false
    );
}

void SemanticAnalyzer::visit(OperatorDefinition& def)
{
    if (def.fixity == TokenType::INFIX)
    {
        Doctor::get().assert(
            def.parameters.size() == 2,
            WaspStage::Semantics,
            "Infix operator '" + def.name + "' must have 2 parameters."
        );
    }
    else
    {
        Doctor::get().assert(
            def.parameters.size() == 1,
            WaspStage::Semantics,
            "Unary operator '" + def.name + "' must have 1 parameter."
        );
    }

    analyze_callable(def, ScopeType::PURE_FUNCTION, nullptr, false);
}

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    analyze_oop_definition(def);
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    analyze_oop_definition(def);
}

void SemanticAnalyzer::analyze_oop_definition(AbstractOOPDefinition& def)
{
    auto type_obj = def.symbol->get_type();
    BaseOOPType_ptr oop_type;

    if (auto class_type = try_unwrap_ptr<ClassType_ptr>(type_obj))
    {
        oop_type = class_type;
    }
    else if (auto trait_type = try_unwrap_ptr<TraitType_ptr>(type_obj))
    {
        oop_type = trait_type;
    }

    bool has_generics = prepare_generic_scope(oop_type->generics);

    // --- Pass 0: Structure Filling ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<FieldDefinition>())
        {
            Doctor::get().assert(
                std::find(
                    oop_type->fields.begin(),
                    oop_type->fields.end(),
                    f->name
                ) == oop_type->fields.end(),
                WaspStage::Semantics,
                "Duplicate field '" + f->name + "'."
            );
            oop_type->fields.push_back(f->name);
            oop_type->member_types[f->name] = visit(f->type);
        }
        else if (auto* m = stmt->try_as<MethodDefinition>())
        {
            auto push_unique = [](StringVector& vec, const std::string& name)
            {
                if (std::find(vec.begin(), vec.end(), name) == vec.end())
                {
                    vec.push_back(name);
                }
            };
            push_unique(oop_type->methods, m->name);
            if (m->is_pure)
            {
                push_unique(oop_type->pures, m->name);
            }
            if (m->is_static)
            {
                push_unique(oop_type->statics, m->name);
            }
        }
        else
        {
            Doctor::get().fatal(WaspStage::Semantics, "Invalid OOP body stmt.");
        }
    }

    // --- Pass 1: Hoisting Signatures ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ObjectVector param_types;
            for (const auto& [name, type_node] : f->parameters)
            {
                param_types.push_back(visit(type_node));
            }

            auto signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    f->return_type ? visit(f->return_type)
                                   : workspace->pool->get_none_type(),
                    oop_type->generics,
                    oop_type->expected_generic_names_order
                )
            );

            f->symbol = SymbolFactory::create_method(
                f->name,
                signature,
                false,
                current_scope->get_closure_depth(),
                current_scope->get_lexical_depth()
            );
            oop_type->add_overload(f->name, signature);
        }
    }

    // --- Pass 2: Body Analysis (using unified helper) ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ScopeType st = f->is_pure ? ScopeType::PURE_METHOD
                                      : ScopeType::METHOD;
            analyze_callable(*f, st, type_obj, f->is_static);
        }
    }

    if (has_generics)
    {
        leave_scope();
    }
}

} // namespace Wasp
