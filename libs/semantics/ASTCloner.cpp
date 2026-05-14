#include "ASTCloner.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include "Workspace.h"

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
    if (auto cls = try_unwrap_ptr<ClassType_ptr>(obj))
    {
        return cls->name;
    }
    if (auto trt = try_unwrap_ptr<TraitType_ptr>(obj))
    {
        return trt->name;
    }
    if (auto alias = try_unwrap_ptr<TypeAlias_ptr>(obj))
    {
        return alias->name;
    }
    if (auto gen = try_unwrap_ptr<TemplateParameterType_ptr>(obj))
    {
        return gen->name;
    }

    Doctor::get().fatal(
        WaspStage::Compiler,
        "Attempted to substitute an unnamed/invalid type in AST Cloner."
    );
}

Expression_ptr ASTCloner::clone(const Expression_ptr& expr)
{
    if (!expr)
    {
        return nullptr;
    }
    auto cloned = make_expression(
        clone_expr_data(expr->data),
        expr->start_token,
        expr->end_token,
        expr->is_desugared
    );
    return cloned;
}

Statement_ptr ASTCloner::clone(const Statement_ptr& stmt)
{
    if (!stmt)
    {
        return nullptr;
    }
    auto cloned = make_statement(clone_stmt_data(stmt->data));
    cloned->start_token = stmt->start_token;
    cloned->end_token = stmt->end_token;
    return cloned;
}

TypeAnnotation_ptr ASTCloner::clone(const TypeAnnotation_ptr& type)
{
    if (!type)
    {
        return nullptr;
    }
    return make_type_annotation(
        clone_type_data(type->data),
        type->start_token,
        type->end_token
    );
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
                auto c = n;
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const FloatLiteral& n) -> ExpressionVariant
            {
                auto c = n;
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const StringLiteral& n) -> ExpressionVariant
            {
                auto c = n;
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const BooleanLiteral& n) -> ExpressionVariant
            {
                auto c = n;
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const NoneLiteral& n) -> ExpressionVariant
            {
                return n;
            },
            [&](const DotLiteral& n) -> ExpressionVariant
            {
                auto c = n;
                wipe_resolvable(c);
                return c;
            },

            [&](const InterpolatedString& n) -> ExpressionVariant
            {
                return InterpolatedString{clone(n.parts)};
            },

            // [&](const Identifier& id) -> ExpressionVariant
            // {
            //     auto c = id;
            //     wipe_resolvable(c);
            //     return c;
            // },

            [&](const Identifier& id) -> ExpressionVariant
            {
                auto c = id;
                wipe_resolvable(c);

                if (auto it = substitutions.find(id.name); it != substitutions.end())
                {
                    Object_ptr concrete_type = it->second;

                    int c_depth = id.symbol ? id.symbol->closure_depth : 0;
                    int l_depth = id.symbol ? id.symbol->lexical_depth : 0;

                    // "T" acts as a type. By turning it into a TypeAlias,
                    // the Semantic Analyzer will safely resolve it to the concrete
                    // type!
                    c.symbol = SymbolFactory::create_type_alias(
                        id.name,
                        concrete_type,
                        c_depth,
                        l_depth
                    );
                }

                return c;
            },

            [&](const MemberAccess& ma) -> ExpressionVariant
            {
                MemberAccess c(clone(ma.left), clone(ma.right));
                c.member_index = -1;
                c.is_enum_value = false;
                return c;
            },

            [&](const Call& c) -> ExpressionVariant
            {
                Call new_call(clone(c.callable), clone(c.arguments));
                new_call.is_method_call = false;
                new_call.overload_index = -1;
                return new_call;
            },

            [&](const Constructor& c) -> ExpressionVariant
            {
                return Constructor(clone(c.construtable), clone(c.values));
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
                c.overload_index = -1;
                wipe_resolvable(c);
                return c;
            },
            [&](const Infix& n) -> ExpressionVariant
            {
                auto c = Infix(clone(n.left), n.op, clone(n.right));
                c.overload_index = -1;
                wipe_resolvable(c);
                return c;
            },
            [&](const Postfix& n) -> ExpressionVariant
            {
                auto c = Postfix(clone(n.operand), n.op);
                c.overload_index = -1;
                wipe_resolvable(c);
                return c;
            },

            [&](const ListLiteral& n) -> ExpressionVariant
            {
                auto c = ListLiteral(clone(n.expressions));
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const TupleLiteral& n) -> ExpressionVariant
            {
                auto c = TupleLiteral(clone(n.expressions));
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },
            [&](const SetLiteral& n) -> ExpressionVariant
            {
                auto c = SetLiteral(clone(n.expressions));
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
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
                wipe_resolvable(c);
                c.constructible = clone(n.constructible);
                return c;
            },

            [&](const RangeLiteral& n) -> ExpressionVariant
            {
                return RangeLiteral(
                    clone(n.start),
                    clone(n.end),
                    clone(n.step),
                    n.is_inclusive
                );
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
                    param.second = clone(param.second);
                }
                wipe_callable(c);
                return c;
            },

            [&](const MethodDefinition& n) -> StatementVariant
            {
                MethodDefinition c = n;
                c.return_type = clone(n.return_type);
                c.body = clone(n.body);
                for (auto& param : c.parameters)
                {
                    param.second = clone(param.second);
                }
                wipe_callable(c);
                return c;
            },

            [&](const OperatorDefinition& n) -> StatementVariant
            {
                OperatorDefinition c = n;
                c.return_type = clone(n.return_type);
                c.body = clone(n.body);
                for (auto& param : c.parameters)
                {
                    param.second = clone(param.second);
                }
                wipe_callable(c);
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
                wipe_resolvable(c);
                return c;
            },

            [&](const TraitDefinition& n) -> StatementVariant
            {
                TraitDefinition
                    c(n.name, n.traits, clone(n.members), n.template_params);
                wipe_resolvable(c);
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
                return Return(n.expression ? clone(*n.expression) : nullptr);
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
                return n;
            },
            [&](const LiteralTypeNode& n) -> TypeAnnotationVariant
            {
                return LiteralTypeNode{clone(n.literal)};
            },

            // [&](const TypeIdentifierNode& n) -> TypeAnnotationVariant
            // {
            //     return n;
            // },

            [&](const TypeIdentifierNode& n) -> TypeAnnotationVariant
            {
                auto c = n;

                if (auto it = substitutions.find(n.name); it != substitutions.end())
                {
                    Object_ptr concrete_type = it->second;

                    if (concrete_type)
                    {
                        // Safely extract "int" instead of "class type: Int"
                        c.name = get_raw_type_name(concrete_type);
                    }
                }

                return c;
            },

            [&](const NativeTypeNode& n) -> TypeAnnotationVariant
            {
                return NativeTypeNode(clone(n.underlying_type));
            },

            [&](const std::shared_ptr<ListTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<ListTypeNode>(clone(n->element_type));
            },
            [&](const std::shared_ptr<TupleTypeNode>& n) -> TypeAnnotationVariant
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
            [&](const std::shared_ptr<VariantTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<VariantTypeNode>(clone(n->types));
            },
            [&](const std::shared_ptr<FunctionTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<FunctionTypeNode>(
                    clone(n->input_types),
                    clone(n->return_type)
                );
            },
            [&](const std::shared_ptr<RecordTypeNode>& n) -> TypeAnnotationVariant
            {
                return std::make_shared<RecordTypeNode>(clone(n->fields));
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
