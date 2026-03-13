#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "TypeAnnotation.h"

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
ObjectVector SemanticAnalyzer::visit(std::vector<TypeAnnotation_ptr>& type_nodes) {
    ObjectVector resolved_types;
    resolved_types.reserve(type_nodes.size());

    for (const auto& node : type_nodes)
        resolved_types.push_back(visit(node));

    return resolved_types;
}

Object_ptr SemanticAnalyzer::visit(const TypeAnnotation_ptr type_node) {
    Doctor::get().fatal_if_nullptr(type_node, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](AnyTypeNode& node) -> Object_ptr { return visit(node); },
            [&](NoneTypeNode& node) -> Object_ptr { return visit(node); },
            [&](IntTypeNode& node) -> Object_ptr { return visit(node); },
            [&](FloatTypeNode& node) -> Object_ptr { return visit(node); },
            [&](StringTypeNode& node) -> Object_ptr { return visit(node); },
            [&](BoolTypeNode& node) -> Object_ptr { return visit(node); },
            [&](IntLiteralTypeNode& node) -> Object_ptr { return visit(node); },
            [&](FloatLiteralTypeNode& node) -> Object_ptr { return visit(node); },
            [&](StringLiteralTypeNode& node) -> Object_ptr { return visit(node); },
            [&](BoolLiteralTypeNode& node) -> Object_ptr { return visit(node); },
            [&](TypeIdentifierNode& node) -> Object_ptr { return visit(node); },

            [&](std::shared_ptr<ListTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<TupleTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<SetTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<MapTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<VariantTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<FunctionTypeNode>& node) -> Object_ptr { return visit(*node); },
            [&](std::shared_ptr<RecordTypeNode>& node) -> Object_ptr { return visit(*node); },

            [](auto&) -> Object_ptr {
                Doctor::get().fatal(
                    WaspStage::Semantics, "Unhandled TypeAnnotation node in visitor"
                );
            }
        },
        type_node->data
    );
}

    Object_ptr SemanticAnalyzer::visit(AnyTypeNode &expr) { return MAKE_OBJECT_VARIANT(AnyType()); }

    Object_ptr SemanticAnalyzer::visit(NoneTypeNode &expr) { return MAKE_OBJECT_VARIANT(NoneType()); }

    Object_ptr SemanticAnalyzer::visit(IntTypeNode &expr) { return MAKE_OBJECT_VARIANT(IntType()); }

    Object_ptr SemanticAnalyzer::visit(FloatTypeNode &expr) { return MAKE_OBJECT_VARIANT(FloatType()); }

    Object_ptr SemanticAnalyzer::visit(StringTypeNode &expr) { return MAKE_OBJECT_VARIANT(StringType()); }

    Object_ptr SemanticAnalyzer::visit(BoolTypeNode &expr) { return MAKE_OBJECT_VARIANT(BooleanType()); }

    Object_ptr SemanticAnalyzer::visit(IntLiteralTypeNode &expr) { return MAKE_OBJECT_VARIANT(IntLiteralType(expr.value)); }

    Object_ptr SemanticAnalyzer::visit(FloatLiteralTypeNode &expr) { return MAKE_OBJECT_VARIANT(FloatLiteralType(expr.value)); }

    Object_ptr SemanticAnalyzer::visit(StringLiteralTypeNode &expr) { return MAKE_OBJECT_VARIANT(StringLiteralType(expr.value)); }

    Object_ptr SemanticAnalyzer::visit(BoolLiteralTypeNode &expr) { return MAKE_OBJECT_VARIANT(BooleanLiteralType(expr.value)); }

    Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode &expr)
    {
        auto symbol = current_scope->lookup(expr.name);
        Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);
        return symbol->get_type();
    }

    Object_ptr SemanticAnalyzer::visit(ListTypeNode &expr) { return MAKE_OBJECT_VARIANT(ListType(visit(expr.element_type))); }

    Object_ptr SemanticAnalyzer::visit(TupleTypeNode &expr) { return MAKE_OBJECT_VARIANT(TupleType(visit(expr.element_types))); }

    Object_ptr SemanticAnalyzer::visit(SetTypeNode &expr) { return MAKE_OBJECT_VARIANT(SetType(visit(expr.element_type))); }

    Object_ptr SemanticAnalyzer::visit(MapTypeNode &expr) { return MAKE_OBJECT_VARIANT(MapType(visit(expr.key_type), visit(expr.value_type))); }

    Object_ptr SemanticAnalyzer::visit(VariantTypeNode &expr) { return MAKE_OBJECT_VARIANT(VariantType(visit(expr.types))); }

    Object_ptr SemanticAnalyzer::visit(FunctionTypeNode &expr)
    {
        ObjectVector params = visit(expr.input_types);
        if (expr.return_type)
            return MAKE_OBJECT_VARIANT(FunctionType(params, visit(expr.return_type)));
        return MAKE_OBJECT_VARIANT(FunctionType(params, MAKE_OBJECT_VARIANT(NoneType())));
    }

    Object_ptr SemanticAnalyzer::visit(RecordTypeNode &expr)
    {
        std::map<std::string, Object_ptr> resolved_members;
        for (const auto &[name, type_ann] : expr.members)
        {
            resolved_members[name] = visit(type_ann);
        }
        return MAKE_OBJECT_VARIANT(RecordType(resolved_members));
    }
}