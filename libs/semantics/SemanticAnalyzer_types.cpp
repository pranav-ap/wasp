#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
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

ObjectVector SemanticAnalyzer::visit(const TypeAnnotationVector& type_nodes)
{
    ObjectVector resolved_types;
    resolved_types.reserve(type_nodes.size());

    for (const auto& node : type_nodes)
    {
        resolved_types.push_back(visit(node));
    }

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

Object_ptr SemanticAnalyzer::visit(LiteralTypeNode& expr)
{
    Doctor::get().fatal_if_nullptr(expr.literal, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](IntegerLiteral& lit) -> Object_ptr
            {
                auto value_obj = make_object(IntObject{lit.value});
                return make_object(LiteralType{value_obj});
            },
            [&](FloatLiteral& lit) -> Object_ptr
            {
                auto value_obj = make_object(FloatObject{lit.value});
                return make_object(LiteralType{value_obj});
            },
            [&](StringLiteral& lit) -> Object_ptr
            {
                auto value_obj = make_object(StringObject{lit.value});
                return make_object(LiteralType{value_obj});
            },
            [&](BooleanLiteral& lit) -> Object_ptr
            {
                auto value_obj = make_object(BooleanObject{lit.value});
                return make_object(LiteralType{value_obj});
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expression inside LiteralTypeNode is not a valid literal."
                );
                return nullptr;
            }
        },
        expr.literal->data
    );
}

Object_ptr SemanticAnalyzer::visit(NativeTypeNode& expr)
{
    Doctor::get().fatal_if_nullptr(expr.underlying_type, WaspStage::Semantics);

    return std::visit(
        overloaded{
            [&](TypeIdentifierNode& id_node) -> Object_ptr
            {
                if (id_node.name == "int")
                {
                    return workspace->pool->get_int_type();
                }
                if (id_node.name == "float")
                {
                    return workspace->pool->get_float_type();
                }
                if (id_node.name == "str")
                {
                    return workspace->pool->get_string_type();
                }
                if (id_node.name == "bool")
                {
                    return workspace->pool->get_boolean_type();
                }
                if (id_node.name == "any")
                {
                    return workspace->pool->get_any_type();
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unknown compiler intrinsic type: '@" + id_node.name + "'"
                );
            },
            [&](std::shared_ptr<ListTypeNode>& list_node) -> Object_ptr
            {
                Object_ptr element_type = this->visit(list_node->element_type);
                return make_object(ListType(element_type));
            },
            [&](std::shared_ptr<TupleTypeNode>& tuple_node) -> Object_ptr
            {
                ObjectVector element_types;

                for (const auto& t : tuple_node->element_types)
                {
                    element_types.push_back(this->visit(t));
                }

                return make_object(TupleType(element_types));
            },
            [&](std::shared_ptr<SetTypeNode>& set_node) -> Object_ptr
            {
                Object_ptr element_type = this->visit(set_node->element_type);
                return make_object(SetType(element_type));
            },
            [&](std::shared_ptr<MapTypeNode>& map_node) -> Object_ptr
            {
                Object_ptr key_type = this->visit(map_node->key_type);
                Object_ptr value_type = this->visit(map_node->value_type);
                return make_object(MapType(key_type, value_type));
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "You can't just slap an '@' on things and hope it works"
                );
            }
        },
        expr.underlying_type->data
    );
}

Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode& expr)
{
    auto symbol = current_scope->lookup(expr.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Unknown type identifier: '" + expr.name + "'"
    );

    Object_ptr resolved_type = unwrap_type_alias(symbol->get_type());
    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(ListTypeNode& expr)
{
    // TODO : need to resolve to template class

    Object_ptr resolved_type = make_object(ListType(visit(expr.element_type)));
    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(TupleTypeNode& expr)
{
    Object_ptr resolved_type = make_object(
        TupleType(visit(expr.element_types))
    );

    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(SetTypeNode& expr)
{
    Object_ptr resolved_type = make_object(SetType(visit(expr.element_type)));
    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(MapTypeNode& expr)
{
    Object_ptr resolved_type = make_object(
        MapType(visit(expr.key_type), visit(expr.value_type))
    );

    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(VariantTypeNode& expr)
{
    Object_ptr resolved_type = make_object(VariantType(visit(expr.types)));
    return resolved_type;
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
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Record types are not yet supported."
    );
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
