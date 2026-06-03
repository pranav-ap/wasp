#include "ASTCloner.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "TypeAnnotation.h"

#include <map>
#include <memory>
#include <optional>
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

std::string get_raw_type_name(Object_ptr obj)
{
    Doctor::get().fatal_if_nullptr(
        obj,
        WaspStage::Semantics,
        "Cannot get raw type name of nullptr type"
    );

    return std::visit(
        overloaded{
            [](const ClassType_ptr& c)
            {
                return c->name;
            },
            [](const TraitType_ptr& t)
            {
                return t->name;
            },
            [](const TypeAlias_ptr& a)
            {
                return get_raw_type_name(a->underlying_type);
            },
            [](const GenericType_ptr& g)
            {
                return g->name;
            },
            [](const IntType&)
            {
                return std::string("int");
            },
            [](const FloatType&)
            {
                return std::string("float");
            },
            [](const StringType&)
            {
                return std::string("str");
            },
            [](const BooleanType&)
            {
                return std::string("bool");
            },
            [](const AnyType&)
            {
                return std::string("any");
            },

            [](const auto&) -> std::string
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Failed to get raw name of type during AST cloning"
                );
            }
        },
        obj->value
    );
}

Expression_ptr ASTCloner::clone(const Expression_ptr& expr)
{
    if (!expr)
    {
        return nullptr;
    }

    auto cloned = make_expression(clone_expr_data(expr->data), expr->is_desugared);
    return cloned;
}

Statement_ptr ASTCloner::clone(const Statement_ptr& stmt)
{
    if (!stmt)
    {
        return nullptr;
    }

    auto cloned = make_statement(clone_stmt_data(stmt->data));
    return cloned;
}

TypeAnnotation_ptr ASTCloner::clone(const TypeAnnotation_ptr& type)
{
    if (!type)
    {
        return nullptr;
    }

    return make_type_annotation(clone_type_data(type->data));
}

ExpressionVector ASTCloner::clone(const ExpressionVector& exprs)
{
    ExpressionVector cloned;
    cloned.reserve(exprs.size());

    for (const auto& e : exprs)
    {
        cloned.push_back(clone(e));
    }

    return cloned;
}

StatementVector ASTCloner::clone(const StatementVector& stmts)
{
    StatementVector cloned;
    cloned.reserve(stmts.size());

    for (const auto& s : stmts)
    {
        cloned.push_back(clone(s));
    }

    return cloned;
}

TypeAnnotationVector ASTCloner::clone(const TypeAnnotationVector& types)
{
    TypeAnnotationVector cloned;
    cloned.reserve(types.size());

    for (const auto& t : types)
    {
        cloned.push_back(clone(t));
    }

    return cloned;
}

ExpressionVariant ASTCloner::clone_expr_data(const ExpressionVariant& data)
{
    return std::visit(
        overloaded{
            [&](std::monostate) -> ExpressionVariant
            {
                return std::monostate{};
            },
            [&](const IntegerLiteral& n) -> ExpressionVariant
            {
                return IntegerLiteral(n.value);
            },
            [&](const FloatLiteral& n) -> ExpressionVariant
            {
                return FloatLiteral(n.value);
            },
            [&](const StringLiteral& n) -> ExpressionVariant
            {
                return StringLiteral(n.value);
            },
            [&](const BooleanLiteral& n) -> ExpressionVariant
            {
                return BooleanLiteral(n.value);
            },
            [&](const NoneLiteral& n) -> ExpressionVariant
            {
                return NoneLiteral();
            },
            [&](const InterpolatedString& n) -> ExpressionVariant
            {
                return InterpolatedString{clone(n.parts)};
            },

            [&](const Identifier& id) -> ExpressionVariant
            {
                auto c = id;
                wipe_resolvable(c);

                if (auto it = substitutions.find(id.name); it != substitutions.end())
                {
                    Object_ptr concrete_type = it->second;

                    if (concrete_type)
                    {
                        c.name = get_raw_type_name(concrete_type);
                    }
                }

                return c;
            },

            [&](const MemberAccess& ma) -> ExpressionVariant
            {
                MemberAccess c(clone(ma.left), clone(ma.right));
                return c;
            },

            [&](const EnumMember& em) -> ExpressionVariant
            {
                return EnumMember(em.enum_type_id, em.enum_member_value);
            },

            [&](const Box& b) -> ExpressionVariant
            {
                return Box(clone(b.expr), b.trait_type_ids);
            },

            [&](const Call& c) -> ExpressionVariant
            {
                Call new_call(clone(c.callable), clone(c.arguments));
                new_call.is_method_call = c.is_method_call;
                new_call.overload_index = c.overload_index;
                new_call.is_trait_dispatch = c.is_trait_dispatch;
                new_call.trait_type_id = c.trait_type_id;
                return new_call;
            },

            [&](const Constructor& c) -> ExpressionVariant
            {
                auto new_constructor = Constructor(
                    clone(c.construtable),
                    clone(c.values)
                );

                return new_constructor;
            },

            [&](const TemplateAngular& ta) -> ExpressionVariant
            {
                TemplateAngular c(clone(ta.target), clone(ta.angular_nodes));
                wipe_resolvable(c);
                c.group_symbol = nullptr;
                c.overload_index = -1;
                return c;
            },

            [&](const Prefix& n) -> ExpressionVariant
            {
                auto c = Prefix(n.op, clone(n.operand));
                wipe_resolvable(c);

                return c;
            },
            [&](const Infix& n) -> ExpressionVariant
            {
                auto c = Infix(clone(n.left), n.op, clone(n.right));
                wipe_resolvable(c);

                return c;
            },
            [&](const Postfix& n) -> ExpressionVariant
            {
                auto c = Postfix(clone(n.operand), n.op);
                wipe_resolvable(c);

                return c;
            },

            [&](const ListLiteral& n) -> ExpressionVariant
            {
                auto c = ListLiteral(clone(n.expressions));
                return c;
            },
            [&](const TupleLiteral& n) -> ExpressionVariant
            {
                auto c = TupleLiteral(clone(n.expressions));
                return c;
            },
            [&](const SetLiteral& n) -> ExpressionVariant
            {
                auto c = SetLiteral(clone(n.expressions));
                return c;
            },
            [&](const MapLiteral& n) -> ExpressionVariant
            {
                std::map<Expression_ptr, Expression_ptr> new_pairs;
                for (const auto& [k, v] : n.pairs)
                {
                    new_pairs[clone(k)] = clone(v);
                }
                auto c = MapLiteral(std::move(new_pairs));
                return c;
            },

            [&](const Assignment& n) -> ExpressionVariant
            {
                Assignment c(
                    clone(n.lhs),
                    clone(n.rhs),
                    n.is_definition,
                    n.is_mutable,
                    n.declared_type
                        ? std::make_optional(clone(n.declared_type.value()))
                        : std::nullopt
                );
                wipe_resolvable(c);
                return c;
            },

            [&](const IfTernaryBranch& n) -> ExpressionVariant
            {
                return IfTernaryBranch(
                    clone(n.test),
                    clone(n.true_expression),
                    clone(n.alternative)
                );
            },
            [&](const ElseTernaryBranch& n) -> ExpressionVariant
            {
                return ElseTernaryBranch(clone(n.expression));
            }
        },
        data
    );
}

StatementVariant ASTCloner::clone_stmt_data(const StatementVariant& data)
{
    return std::visit(
        overloaded{
            [&](std::monostate) -> StatementVariant
            {
                return std::monostate{};
            },

            [&](const ExpressionStatement& n) -> StatementVariant
            {
                return ExpressionStatement(clone(n.expression));
            },

            [&](const TypeAliasDefinition& n) -> StatementVariant
            {
                TypeAliasDefinition c(n.name, clone(n.ref_type), n.template_params);
                wipe_resolvable(c);
                return c;
            },

            [&](const EnumDefinition& n) -> StatementVariant
            {
                auto c = n;
                wipe_resolvable(c);
                return c;
            },

            [&](const FunctionDefinition& n) -> StatementVariant
            {
                FunctionDefinition c = n;
                c.return_type = clone(n.return_type);
                c.body = clone(n.body);
                for (auto& param : c.parameters)
                {
                    param.type = clone(param.type);
                }

                // wipe_callable(c);
                c.symbol = n.symbol;

                return c;
            },

            [&](const FieldDefinition& n) -> StatementVariant
            {
                FieldDefinition c(n.name, clone(n.type));
                wipe_resolvable(c);
                return c;
            },

            [&](const ClassDefinition& n) -> StatementVariant
            {
                ClassDefinition
                    c(n.name, n.traits, clone(n.members), n.template_params);
                // wipe_resolvable(c);

                c.symbol = n.symbol;
                return c;
            },

            [&](const TraitDefinition& n) -> StatementVariant
            {
                TraitDefinition
                    c(n.name, n.traits, clone(n.members), n.template_params);
                // wipe_resolvable(c);
                c.symbol = n.symbol;
                return c;
            },

            [&](const Import& n) -> StatementVariant
            {
                auto c = n;
                wipe_resolvable(c);
                for (auto& pair : c.exposed_symbols)
                {
                    wipe_resolvable(pair);
                }
                return c;
            },

            [&](const IfBranch& n) -> StatementVariant
            {
                return IfBranch(
                    clone(n.test),
                    clone(n.body),
                    n.alternative ? clone(*n.alternative) : nullptr
                );
            },

            [&](const ElseBranch& n) -> StatementVariant
            {
                return ElseBranch(clone(n.body));
            },

            [&](const SimpleLoop& n) -> StatementVariant
            {
                return SimpleLoop(clone(n.body), clone(n.condition), n.style);
            },

            [&](const ForInLoop& n) -> StatementVariant
            {
                ForInLoop c(
                    clone(n.body),
                    clone(n.lhs),
                    clone(n.iterable),
                    n.lhs_is_mutable
                );
                c.iterator_symbol = nullptr;
                return c;
            },

            [&](const LoopControl& n) -> StatementVariant
            {
                return n;
            },
            [&](const Placeholder& n) -> StatementVariant
            {
                return n;
            },
            [&](const Return& n) -> StatementVariant
            {
                if (n.expression.has_value())
                {
                    auto return_value = clone(n.expression.value());

                    if (return_value)
                    {
                        return Return(return_value);
                    }
                }

                return Return();
            },
            [&](const auto&) -> StatementVariant
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Failed to clone statement during AST cloning"
                );
            }
        },
        data
    );
}

TypeAnnotationVariant ASTCloner::clone_type_data(const TypeAnnotationVariant& data)
{
    return std::visit(
        overloaded{
            [&](std::monostate) -> TypeAnnotationVariant
            {
                return std::monostate{};
            },
            [&](const NoneTypeNode& n) -> TypeAnnotationVariant
            {
                return NoneTypeNode();
            },
            [&](const LiteralTypeNode& n) -> TypeAnnotationVariant
            {
                return LiteralTypeNode{clone(n.literal)};
            },

            [&](const TypeIdentifierNode& n) -> TypeAnnotationVariant
            {
                auto c = n;

                if (auto it = substitutions.find(n.name); it != substitutions.end())
                {
                    Object_ptr concrete_type = it->second;

                    if (concrete_type)
                    {
                        c.name = get_raw_type_name(concrete_type);
                    }
                }

                return c;
            },

            [&](const std::shared_ptr<ListTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<ListTypeNode>(clone(n->element_type));
            },
            [&](const std::shared_ptr<TupleTypeNode>& n)
                -> TypeAnnotationVariant
            {
                return std::make_shared<TupleTypeNode>(clone(n->element_types));
            },
            [&](const std::shared_ptr<SetTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<SetTypeNode>(clone(n->element_type));
            },
            [&](const std::shared_ptr<MapTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<MapTypeNode>(
                    clone(n->key_type),
                    clone(n->value_type)
                );
            },
            [&](const std::shared_ptr<VariantTypeNode>& n)
                -> TypeAnnotationVariant
            {
                return std::make_shared<VariantTypeNode>(clone(n->types));
            },
            [&](const std::shared_ptr<IntersectionTypeNode>& n)
                -> TypeAnnotationVariant
            {
                return std::make_shared<IntersectionTypeNode>(clone(n->types));
            },
            [&](const std::shared_ptr<FunctionTypeNode>& n)
                -> TypeAnnotationVariant
            {
                return std::make_shared<FunctionTypeNode>(
                    clone(n->input_types),
                    clone(n->return_type)
                );
            },
            [&](const std::shared_ptr<TemplateAngularTypeNode>& n)
                -> TypeAnnotationVariant
            {
                return std::make_shared<TemplateAngularTypeNode>(
                    clone(n->base_node),
                    clone(n->angular_nodes)
                );
            }
        },
        data
    );
}

} // namespace Wasp
