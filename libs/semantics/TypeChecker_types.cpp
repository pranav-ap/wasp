#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Type.h"
#include "TypeChecker.h"
#include "TypeNode.h"

#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeChecker::visit(const TypeNode_ptr type_node)
{
    Doctor::get().fatal_if_nullptr(type_node, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](std::monostate&) -> Type_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Type node is in monostate"
                );
            },
            [&](auto& node) -> Type_ptr
            {
                return this->visit(node);
            }
        },
        type_node->data
    );
}

TypeVector TypeChecker::visit(const TypeNodeVector& type_nodes)
{
    TypeVector types;

    for (const auto& node : type_nodes)
    {
        types.push_back(visit(node));
    }

    return types;
}

Type_ptr TypeChecker::visit(NoneTypeNode&)
{
    return make_shared_type<NoneType>();
}

Type_ptr TypeChecker::visit(LiteralTypeNode& type_node)
{
    return std::visit(
        overloaded{
            [&](IntegerLiteral&) -> Type_ptr
            {
                return make_type(std::make_shared<IntType>());
            },
            [&](FloatLiteral&) -> Type_ptr
            {
                return make_type(std::make_shared<FloatType>());
            },
            [&](StringLiteral&) -> Type_ptr
            {
                return make_type(std::make_shared<StringType>());
            },
            [&](BooleanLiteral&) -> Type_ptr
            {
                return make_type(std::make_shared<BooleanType>());
            },
            [&](NoneLiteral&) -> Type_ptr
            {
                return make_type(std::make_shared<NoneType>());
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid literal type"
                );
            }
        },
        type_node.literal->data
    );
}

Type_ptr TypeChecker::visit(TypeIdentifierNode& type_node)
{
    auto symbol = current_scope->lookup(type_node.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined type: " + type_node.name
    );

    auto type = symbol->get_type();

    Doctor::get().fatal_if_nullptr(
        type,
        WaspStage::Semantics,
        "Symbol is not a type: " + type_node.name
    );

    return type;
}

Type_ptr TypeChecker::visit(ListTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<ListType>(element_type);
}

Type_ptr TypeChecker::visit(TupleTypeNode& type_node)
{
    TypeVector element_types = visit(type_node.element_types);
    return make_shared_type<TupleType>(element_types);
}

Type_ptr TypeChecker::visit(SetTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<SetType>(element_type);
}

Type_ptr TypeChecker::visit(MapTypeNode& type_node)
{
    Type_ptr key_type = visit(type_node.key_type);
    Type_ptr value_type = visit(type_node.value_type);
    return make_shared_type<MapType>(key_type, value_type);
}

Type_ptr TypeChecker::visit(VariantTypeNode& type_node)
{
    TypeVector options = visit(type_node.options);
    return make_shared_type<VariantType>(options);
}

Type_ptr TypeChecker::visit(IntersectionTypeNode& type_node)
{
    TypeVector types = visit(type_node.types);
    return make_shared_type<IntersectionType>(types);
}

Type_ptr TypeChecker::visit(FunctionTypeNode& type_node)
{
    TypeVector input_types = visit(type_node.input_types);
    Type_ptr return_type = visit(type_node.return_type);
    return make_shared_type<Signature>(input_types, return_type);
}

Type_ptr TypeChecker::visit(AngularTypeNode& type_node)
{
    TypeVector type_arguments = visit(type_node.type_arguments);

    auto type = make_shared_type<AngularType>(
        type_node.name,
        type_arguments
    );

    type_node.symbol->set_type(type);

    return type;
}

} // namespace Wasp
