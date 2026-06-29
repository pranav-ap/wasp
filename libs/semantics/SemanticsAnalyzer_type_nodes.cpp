#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Type.h"
#include "TypeNode.h"
#include "TypeSystem.h"

#include <cstddef>
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

Type_ptr SemanticsAnalyzer::visit(TypeNode_ptr type_node)
{
    Doctor::semantics().fatal_if_nullptr(type_node);

    return std::visit(
        overloaded{
            [&](std::monostate&) -> Type_ptr
            {
                Doctor::semantics().fatal("Type node is in monostate");
            },
            [&](auto& node) -> Type_ptr
            {
                return visit(node);
            }
        },
        type_node->data
    );
}

TypeVector SemanticsAnalyzer::visit(TypeNodeVector& type_nodes)
{
    TypeVector types;

    for (const TypeNode_ptr& node : type_nodes)
    {
        Type_ptr type = visit(node);
        types.push_back(type);
    }

    return types;
}

Type_ptr SemanticsAnalyzer::visit(NoneTypeNode&)
{
    return make_shared_type<NoneType>();
}

Type_ptr SemanticsAnalyzer::visit(LiteralTypeNode& type_node)
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
                Doctor::semantics().fatal("Invalid literal type");
            }
        },
        type_node.literal->data
    );
}

Type_ptr SemanticsAnalyzer::visit(TypeIdentifierNode& type_node)
{
    if (type_node.name == "int")
    {
        return make_type(std::make_shared<IntType>());
    }
    else if (type_node.name == "float")
    {
        return make_type(std::make_shared<FloatType>());
    }
    else if (type_node.name == "str")
    {
        return make_type(std::make_shared<StringType>());
    }
    else if (type_node.name == "bool")
    {
        return make_type(std::make_shared<BooleanType>());
    }
    else if (type_node.name == "any")
    {
        return make_type(std::make_shared<AnyType>());
    }

    Symbol_ptr symbol = current_scope->lookup(type_node.name);

    Doctor::semantics().fatal_if_nullptr(
        symbol,
        "Undefined type: " + type_node.name
    );

    Type_ptr type = symbol->get_type();

    Doctor::semantics().fatal_if_nullptr(
        type,
        "Symbol is not a type: " + type_node.name
    );

    return type;
}

Type_ptr SemanticsAnalyzer::visit(ListTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<ListType>(element_type);
}

Type_ptr SemanticsAnalyzer::visit(TupleTypeNode& type_node)
{
    TypeVector element_types = visit(type_node.element_types);
    return make_shared_type<TupleType>(element_types);
}

Type_ptr SemanticsAnalyzer::visit(SetTypeNode& type_node)
{
    Type_ptr element_type = visit(type_node.element_type);
    return make_shared_type<SetType>(element_type);
}

Type_ptr SemanticsAnalyzer::visit(MapTypeNode& type_node)
{
    Type_ptr key_type = visit(type_node.key_type);
    Type_ptr value_type = visit(type_node.value_type);
    return make_shared_type<MapType>(key_type, value_type);
}

Type_ptr SemanticsAnalyzer::visit(VariantTypeNode& type_node)
{
    TypeVector options = visit(type_node.options);
    return make_shared_type<VariantType>(options);
}

Type_ptr SemanticsAnalyzer::visit(IntersectionTypeNode& type_node)
{
    TypeVector types = visit(type_node.types);
    return make_shared_type<IntersectionType>(types);
}

Type_ptr SemanticsAnalyzer::visit(FunctionTypeNode& type_node)
{
    TypeVector parameter_types = visit(type_node.parameter_types);
    Type_ptr return_type = visit(type_node.return_type);

    FunctionType_ptr function_type = std::make_shared<FunctionType>("");

    function_type->parameter_types = parameter_types;
    function_type->return_type = return_type;
    function_type->template_type = std::make_shared<TemplateType>();
    function_type->is_pure = false;
    function_type->is_native = false;

    return make_type(function_type);
}

Type_ptr SemanticsAnalyzer::visit(AngularTypeNode& type_node)
{
    Symbol_ptr base_symbol = current_scope->lookup_required_and_resolve(type_node.name);
    Doctor::semantics().fatal_if_nullptr(base_symbol, "Base symbol not found for type: " + type_node.name);

    TypeVector type_arguments = visit(type_node.type_arguments);
    Type_ptr base_type = base_symbol->get_type();

    return std::visit(
        overloaded{
            [&](ClassType_ptr t) -> Type_ptr
            {
                return specialize_oops_type(t, base_symbol, type_arguments, type_node);
            },
            [&](TraitType_ptr t) -> Type_ptr
            {
                return specialize_oops_type(t, base_symbol, type_arguments, type_node);
            },
            [&](PrimitiveType_ptr t) -> Type_ptr
            {
                return specialize_oops_type(t, base_symbol, type_arguments, type_node);
            },
            [&](AngularType_ptr t) -> Type_ptr
            {
                Doctor::semantics().fatal("Nested angular types are not supported: " + base_symbol->name);
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Angular type is not applicable to " + base_symbol->name);
            }
        },
        base_type->data
    );

    return base_type;
}

Type_ptr SemanticsAnalyzer::specialize_oops_type(
    OopsType_ptr oops_type,
    Symbol_ptr base_symbol,
    const TypeVector& type_arguments,
    AngularTypeNode& node
)
{
    Doctor::semantics().check(
        !oops_type->template_type->empty(),
        "Type '" + oops_type->name + "' is not a template, but type arguments were provided."
    );

    const StringVector& param_names = oops_type->template_type->ordered_parameter_names;
    Doctor::semantics().check(
        param_names.size() == type_arguments.size(),
        "Template argument count mismatch for type '" + oops_type->name + "'. Expected " +
            std::to_string(param_names.size()) + ", got " + std::to_string(type_arguments.size()) + "."
    );

    TypeSubstitutionMap substitutions;

    for (size_t i = 0; i < param_names.size(); ++i)
    {
        substitutions[param_names[i]] = type_arguments[i];
    }

    std::string mangled_name = oops_type->name + "_" + TypeSystem::mangle(type_arguments);

    Symbol_ptr specialized_symbol = solidify_template(base_symbol, mangled_name, substitutions);
    node.symbol = specialized_symbol;

    return specialized_symbol->get_type();
}

} // namespace Wasp
