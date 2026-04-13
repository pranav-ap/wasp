#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "TypeAnnotation.h"

#include <map>
#include <memory>
#include <string>
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

ObjectVector SemanticAnalyzer::visit(std::vector<TypeAnnotation_ptr>& type_nodes)
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
            [&](AnyTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](NoneTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](IntTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](FloatTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](StringTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](BoolTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](IntLiteralTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](FloatLiteralTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](StringLiteralTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](BoolLiteralTypeNode& node) -> Object_ptr
            {
                return visit(node);
            },
            [&](TypeIdentifierNode& node) -> Object_ptr
            {
                return visit(node);
            },

            [&](std::shared_ptr<ListTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<TupleTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<SetTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<MapTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<VariantTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<FunctionTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },
            [&](std::shared_ptr<RecordTypeNode>& node) -> Object_ptr
            {
                return visit(*node);
            },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled TypeAnnotation node in visitor"
                );
            }
        },
        type_node->data
    );
}

Object_ptr SemanticAnalyzer::visit(AnyTypeNode& expr)
{
    return make_object(AnyType());
}

Object_ptr SemanticAnalyzer::visit(NoneTypeNode& expr)
{
    return make_object(NoneType());
}

Object_ptr SemanticAnalyzer::visit(IntTypeNode& expr)
{
    return make_object(IntType());
}

Object_ptr SemanticAnalyzer::visit(FloatTypeNode& expr)
{
    return make_object(FloatType());
}

Object_ptr SemanticAnalyzer::visit(StringTypeNode& expr)
{
    return make_object(StringType());
}

Object_ptr SemanticAnalyzer::visit(BoolTypeNode& expr)
{
    return make_object(BooleanType());
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
    return make_object(BooleanLiteralType(expr.value));
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
        return make_object(std::make_shared<LocalFunctionType>(params, visit(expr.return_type)));

    return make_object(std::make_shared<LocalFunctionType>(params, make_object(NoneType())));
}

Object_ptr SemanticAnalyzer::visit(RecordTypeNode& node)
{
    ObjectStringMap record_members;

    for (const auto& stmt_ptr : node.fields)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& field)
                {
                    // record does not support 'our'
                    record_members[field.name] = visit(field.type);
                },
                [](auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "RecordTypeNode contains a non-field statement."
                    );
                }
            },
            stmt_ptr->data
        );
    }

    return make_object(std::make_shared<RecordType>(std::move(record_members)));
}
} // namespace Wasp
