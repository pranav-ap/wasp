#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "TypeAnnotation.h"

#include <map>
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
                return nullptr;
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

Object_ptr SemanticAnalyzer::evaluate_type_template_instantiation(
    Object_ptr base_type_obj,
    const ObjectVector& resolved_args
)
{
    if (base_type_obj->is<TemplateType_ptr>())
    {
        auto tmpl = base_type_obj->as<TemplateType_ptr>();
        auto underlying = tmpl->underlying_type;

        if (underlying->is<ClassType_ptr>())
        {
            auto base_class = underlying->as<ClassType_ptr>();

            ObjectStringMap concrete_members;
            for (const auto& [name, type] : base_class->members)
            {
                concrete_members[name] = type_checker
                                             ->substitute_generics(type, tmpl, resolved_args);
            }

            return make_object(
                std::make_shared<ClassType>(
                    base_class->name,
                    std::move(concrete_members),
                    base_class->fields,
                    base_class->methods,
                    base_class->pures,
                    base_class->statics
                )
            );
        }
        else if (underlying->is<TraitType_ptr>())
        {
            auto base_trait = underlying->as<TraitType_ptr>();

            ObjectStringMap concrete_members;
            for (const auto& [name, type] : base_trait->members)
            {
                concrete_members[name] = type_checker
                                             ->substitute_generics(type, tmpl, resolved_args);
            }

            return make_object(
                std::make_shared<TraitType>(
                    base_trait->name,
                    std::move(concrete_members),
                    base_trait->methods,
                    base_trait->pures,
                    base_trait->statics
                )
            );
        }
        else
        {
            return type_checker->substitute_generics(underlying, tmpl, resolved_args);
        }
    }

    Doctor::get().fatal(WaspStage::Semantics, "Failed to instantiate template type.");
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(TemplateTypeNode& node)
{
    Object_ptr base_type_obj = visit(node.base_type);

    ObjectVector resolved_args;
    resolved_args.reserve(node.generic_args.size());

    for (const auto& arg : node.generic_args)
    {
        resolved_args.push_back(visit(arg));
    }

    if (base_type_obj->is<TemplateType_ptr>())
    {
        auto tmpl = base_type_obj->as<TemplateType_ptr>();
        Doctor::get().assert(
            tmpl->generics.size() == resolved_args.size(),
            WaspStage::Semantics,
            "Argument count mismatch for template."
        );
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "Target type is not a generic template.");
    }

    // Call your internal instantiation helper to bind the resolved_args
    // to the template's generic parameters and return the concrete type.
    return evaluate_type_template_instantiation(base_type_obj, resolved_args);
}

} // namespace Wasp
