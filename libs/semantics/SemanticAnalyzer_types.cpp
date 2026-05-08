#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "TypeAnnotation.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

ObjectVector SemanticAnalyzer::visit(TypeAnnotationVector& type_nodes)
{
    ObjectVector resolved_types;
    resolved_types.reserve(type_nodes.size());

    for (const auto& node : type_nodes)
        resolved_types.push_back(visit(node));

    return resolved_types;
}

Object_ptr SemanticAnalyzer::visit(const TypeAnnotation_ptr type_node)
{
    Doctor::get().fatal_if_nullptr(type_node, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](std::monostate&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled TypeAnnotation node in visitor"
                );
            },
            [&](auto& node) -> Object_ptr
            {
                if constexpr (requires { *node; })
                {
                    return this->visit(*node);
                }
                else
                {
                    return this->visit(node);
                }
            }
        },
        type_node->data
    );
}

Object_ptr SemanticAnalyzer::visit(NoneTypeNode& expr)
{
    return workspace->pool->get_none_type();
}

Object_ptr SemanticAnalyzer::visit(IntLiteralTypeNode& expr)
{
    return make_object(IntLiteralType(expr.value));
}

Object_ptr SemanticAnalyzer::visit(FloatLiteralTypeNode& expr)
{
    return make_object(FloatLiteralType(expr.value));
}

Object_ptr SemanticAnalyzer::visit(StringLiteralTypeNode& expr)
{
    return make_object(StringLiteralType(expr.value));
}

Object_ptr SemanticAnalyzer::visit(BoolLiteralTypeNode& expr)
{
    if (expr.value)
        return workspace->pool->get_true_literal_type();
    else
        return workspace->pool->get_false_literal_type();
}

Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode& expr)
{
    if (expr.is_native)
    {
        if (expr.name == "int")
        {
            return workspace->pool->get_native_int_type();
        }
        if (expr.name == "float")
        {
            return workspace->pool->get_native_float_type();
        }
        if (expr.name == "str")
        {
            return workspace->pool->get_native_string_type();
        }
        if (expr.name == "bool")
        {
            return workspace->pool->get_boolean_type();
        }
        if (expr.name == "any")
        {
            return workspace->pool->get_native_any_type();
        }

        Doctor::get().fatal(
            WaspStage::Semantics,
            "Unknown native type identifier: '" + expr.name + "'"
        );
    }

    auto symbol = current_scope->lookup(expr.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Unknown type identifier: '" + expr.name + "'"
    );

    Object_ptr resolved_type = symbol->get_type();

    while (resolved_type->is<TypeAlias_ptr>())
    {
        resolved_type = resolved_type->as<TypeAlias_ptr>()->underlying_type;
    }

    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(ListTypeNode& expr)
{
    return make_object(ListType(visit(expr.element_type)));
}

Object_ptr SemanticAnalyzer::visit(TupleTypeNode& expr)
{
    return make_object(TupleType(visit(expr.element_types)));
}

Object_ptr SemanticAnalyzer::visit(SetTypeNode& expr)
{
    return make_object(SetType(visit(expr.element_type)));
}

Object_ptr SemanticAnalyzer::visit(MapTypeNode& expr)
{
    return make_object(MapType(visit(expr.key_type), visit(expr.value_type)));
}

Object_ptr SemanticAnalyzer::visit(VariantTypeNode& expr)
{
    return make_object(VariantType(visit(expr.types)));
}

Object_ptr SemanticAnalyzer::visit(FunctionTypeNode& expr)
{
    ObjectVector params = visit(expr.input_types);
    Object_ptr ret_type = expr.return_type ? visit(expr.return_type)
                                           : workspace->pool->get_none_type();

    return make_object(
        std::make_shared<Signature>(
            std::move(params),
            std::move(ret_type),
            ObjectStringMap{},
            StringVector{}
        )
    );
}

Object_ptr SemanticAnalyzer::visit(RecordTypeNode& node)
{
    Doctor::get().fatal(WaspStage::Semantics, "Record types are not yet supported.");
}

Object_ptr SemanticAnalyzer::visit(TemplateAngularTypeNode& node)
{
    // Resolve the base type (e.g., the 'Speaker' in 'Speaker<T>')
    Object_ptr unspecialized_base = visit(node.base_node);
    Doctor::get().fatal_if_nullptr(unspecialized_base, WaspStage::Semantics);

    // Evaluate the generic arguments (e.g., the 'T' in 'Speaker<T>')
    ObjectVector concrete_arguments;
    for (const auto& type_node : node.angular_nodes)
    {
        concrete_arguments.push_back(visit(type_node));
    }

    auto generic_names = type_system->get_generics_declaration_order(
        unspecialized_base
    );

    Doctor::get().assert(
        generic_names.size() == concrete_arguments.size(),
        WaspStage::Semantics,
        "Generic argument count mismatch. Expected " +
            std::to_string(generic_names.size()) + ", but got " +
            std::to_string(concrete_arguments.size()) + "."
    );

    // Map names to types (e.g., { "T": GenericType(T) })
    ObjectStringMap substitutions;
    for (size_t i = 0; i < concrete_arguments.size(); ++i)
    {
        substitutions[generic_names[i]] = concrete_arguments[i];
    }

    auto specialized_type = type_system->substitute_generics(
        unspecialized_base,
        substitutions
    );

    return specialized_type;
}

} // namespace Wasp
