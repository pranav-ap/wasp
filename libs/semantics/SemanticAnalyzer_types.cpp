#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "TypeAnnotation.h"
#include "Workspace.h"

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

Object_ptr SemanticAnalyzer::visit(AnyTypeNode& expr)
{
    return workspace->pool->get_any_type();
}

Object_ptr SemanticAnalyzer::visit(NoneTypeNode& expr)
{
    return workspace->pool->get_none_type();
}

Object_ptr SemanticAnalyzer::visit(IntTypeNode& expr)
{
    return workspace->pool->get_int_type();
}

Object_ptr SemanticAnalyzer::visit(FloatTypeNode& expr)
{
    return workspace->pool->get_float_type();
}

Object_ptr SemanticAnalyzer::visit(StringTypeNode& expr)
{
    return workspace->pool->get_string_type();
}

Object_ptr SemanticAnalyzer::visit(BoolTypeNode& expr)
{
    return workspace->pool->get_boolean_type();
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
    auto symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);
    return symbol->get_type();
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

    if (expr.return_type)
        return make_object(std::make_shared<Signature>(Signature{params, visit(expr.return_type)}));

    return make_object(std::make_shared<Signature>(Signature{params, make_object(NoneType())}));
}

Object_ptr SemanticAnalyzer::visit(RecordTypeNode& node)
{
    Doctor::get().fatal(WaspStage::Semantics, "Record types are not yet supported.");
}

Object_ptr SemanticAnalyzer::visit(GenericTemplateTypeNode& node)
{
    Object_ptr base = visit(node.base_type);

    ObjectVector generic_arguments;

    for (auto& arg_node : node.generic_nodes)
    {
        generic_arguments.push_back(visit(arg_node));
    }

    auto [generics_map, base_name] = type_system->extract_generics_and_name(base);

    Doctor::get().assert(
        !generics_map.empty(),
        WaspStage::Semantics,
        "Expected to extract generics from this type, but couldn't find any"
    );

    Doctor::get().assert(
        base_name != "",
        WaspStage::Semantics,
        "Expected to extract a name from this type, but got an empty string"
    );

    Symbol_ptr base_symbol = current_scope->lookup(base_name);

    std::string mangled_suffix = "<";

    for (const auto& t : generic_arguments)
    {
        mangled_suffix += mangle_object(t);
    }

    mangled_suffix += ">";
    std::string full_mangled_name = base_name + mangled_suffix;

    Doctor::get().fatal(WaspStage::Semantics, "Boom!");
}

} // namespace Wasp
