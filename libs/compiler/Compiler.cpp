#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Symbol.h"
#include "Type.h"
#include "Workspace.h"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Compiler::Compiler(Workspace_ptr workspace) : workspace(workspace)
{
}

// ============================================================================
// Entry Point
// ============================================================================

void Compiler::run(std::vector<Module_ptr>& build_order)
{
    Doctor::compiler().start();

    for (const Module_ptr& mod : build_order)
    {
        emit(mod);
        mod->cpp_code = output;
        mod->save_cpp_code("cpp");
    }

    Doctor::compiler().stop();
}

void Compiler::emit(const Module_ptr& mod)
{
    Doctor::compiler().fatal_if_nullptr(mod, "Module is null");

    output.clear();
    emit_common_includes();

    for (const Statement_ptr stmt : mod->block.statements)
    {
        emit(stmt);
    }
}

void Compiler::emit_common_includes()
{
    emit("#pragma once");
    emit("#include <iostream>");
    emit("#include <string>");
    emit("");
}

void Compiler::emit(const std::string& line)
{
    output += line + "\n";
}

// ============================================================================
// Statements
// ============================================================================

void Compiler::emit(const Statement_ptr stmt)
{
    Doctor::compiler().fatal_if_nullptr(stmt, "Null statement in code generation");

    std::visit(
        overloaded{[&](const auto& s)
                   {
                       if constexpr (requires { emit(s); })
                       {
                           emit(s);
                       }
                   }},
        stmt->data
    );
}

void Compiler::emit(const ExpressionStatement& stmt)
{
    std::string exp = emit(stmt.expression);
    emit(exp + ";");
}

// ============================================================================
// Expressions
// ============================================================================

std::string Compiler::emit(const Expression_ptr expr)
{
    Doctor::compiler().fatal_if_nullptr(expr, "Null expression in code generation");

    return std::visit(
        overloaded{
            [&](const auto& e) -> std::string
            {
                if constexpr (requires { emit(e); })
                {
                    return emit(e);
                }

                Doctor::compiler().fatal("Unsupported expression type in code generation");
            }
        },
        expr->data
    );
}

std::string Compiler::emit(const IntegerLiteral& lit)
{
    return std::to_string(lit.value);
}

std::string Compiler::emit(const FloatLiteral& lit)
{
    return std::to_string(lit.value);
}

std::string Compiler::emit(const StringLiteral& lit)
{
    return lit.value;
}

std::string Compiler::emit(const BooleanLiteral& lit)
{
    return lit.value ? "true" : "false";
}

std::string Compiler::emit(const NoneLiteral& lit)
{
    return "nullptr";
}

std::string Compiler::emit(const Binding& binding)
{
    Doctor::compiler().check(binding.lhs->is<Identifier>(), "Binding LHS must be an identifier");

    Identifier& id = binding.lhs->as<Identifier>();
    Type_ptr type = id.symbol->get_type();
    std::string type_str = emit(type);
    std::string rhs = emit(binding.rhs);

    std::string decl = type_str + " " + id.name + " = " + rhs;

    if (!binding.is_mutable)
    {
        decl = "const " + decl;
    }

    return decl;
}

std::string Compiler::emit(const Assignment& assignment)
{
    std::string lhs = emit(assignment.lhs);
    std::string rhs = emit(assignment.rhs);
    return lhs + " = " + rhs;
}

std::string Compiler::emit(const Type_ptr& type)
{
    if (!type)
        return "void";

    return std::visit(
        overloaded{
            [](AnyType_ptr) -> std::string
            {
                return "void";
            },
            [](NoneType_ptr) -> std::string
            {
                return "void";
            },

            [](IntType_ptr) -> std::string
            {
                return "int";
            },
            [](FloatType_ptr) -> std::string
            {
                return "double";
            },
            [](StringType_ptr) -> std::string
            {
                return "std::string";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "bool";
            },

            [&](LiteralType_ptr lit) -> std::string
            {
                if (lit->value->is<IntegerLiteral>())
                    return "int";
                if (lit->value->is<FloatLiteral>())
                    return "double";
                if (lit->value->is<StringLiteral>())
                    return "std::string";
                if (lit->value->is<BooleanLiteral>())
                    return "bool";
                if (lit->value->is<NoneLiteral>())
                    return "std::nullptr_t";

                return "void";
            },

            [&](ListType_ptr list) -> std::string
            {
                std::string elem = emit(list->element_type);
                return "std::vector<" + elem + ">";
            },
            [&](SetType_ptr set) -> std::string
            {
                std::string elem = emit(set->element_type);
                return "std::set<" + elem + ">";
            },
            [&](TupleType_ptr tuple) -> std::string
            {
                std::string elements;

                for (size_t i = 0; i < tuple->element_types.size(); ++i)
                {
                    if (i > 0)
                        elements += ", ";
                    elements += emit(tuple->element_types[i]);
                }

                return "std::tuple<" + elements + ">";
            },
            [&](MapType_ptr map) -> std::string
            {
                std::string key = emit(map->key_type);
                std::string val = emit(map->value_type);
                return "std::map<" + key + ", " + val + ">";
            },

            [&](VariantType_ptr variant) -> std::string
            {
                std::string types;

                for (size_t i = 0; i < variant->types.size(); ++i)
                {
                    if (i > 0)
                    {
                        types += ", ";
                    }

                    types += emit(variant->types[i]);
                }

                return "std::variant<" + types + ">";
            },
            [&](IntersectionType_ptr inter) -> std::string
            {
                std::string combined;

                for (size_t i = 0; i < inter->types.size(); ++i)
                {
                    if (i > 0)
                    {
                        combined += "_";
                    }

                    combined += emit(inter->types[i]);
                }

                return "Intersection_" + combined;
            },

            [&](ClassType_ptr cls) -> std::string
            {
                return cls->name;
            },
            [&](TraitType_ptr trait) -> std::string
            {
                return trait->name;
            },
            [&](PrimitiveType_ptr prim) -> std::string
            {
                return prim->name;
            },

            [&](FunctionType_ptr func) -> std::string
            {
                std::string params;

                for (size_t i = 0; i < func->parameter_types.size(); ++i)
                {
                    if (i > 0)
                    {
                        params += ", ";
                    }

                    params += emit(func->parameter_types[i]);
                }

                std::string ret = emit(func->return_type);
                return "std::function<" + ret + "(" + params + ")>";
            },

            [&](EnumType_ptr enum_type) -> std::string
            {
                return enum_type->name;
            },

            [&](TypeAlias_ptr alias) -> std::string
            {
                return emit(alias->underlying_type);
            },

            [](const auto&) -> std::string
            {
                Doctor::compiler().fatal("Unsupported type for C++ conversion");
            }
        },
        type->data
    );
}

} // namespace Wasp
