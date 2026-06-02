#pragma once

#include "AST.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "TypeAnnotation.h"

#include <utility>

namespace Wasp
{

class ASTCloner
{
public:
    ObjectStringMap substitutions;

    ASTCloner()
    {
    }

    ASTCloner(ObjectStringMap subs) : substitutions(std::move(subs))
    {
    }

    Expression_ptr clone(const Expression_ptr& expr);
    Statement_ptr clone(const Statement_ptr& stmt);
    TypeAnnotation_ptr clone(const TypeAnnotation_ptr& type);

    ExpressionVector clone(const ExpressionVector& exprs);
    StatementVector clone(const StatementVector& stmts);
    TypeAnnotationVector clone(const TypeAnnotationVector& types);

private:
    ExpressionVariant clone_expr_data(const ExpressionVariant& data);
    StatementVariant clone_stmt_data(const StatementVariant& data);
    TypeAnnotationVariant clone_type_data(const TypeAnnotationVariant& data);

    template <typename T> void wipe_resolvable(T& node)
    {
        node.symbol = nullptr;
    }

    template <typename T> void wipe_callable(T& node)
    {
        wipe_resolvable(node);
        node.parameter_symbols.clear();

        node.context_symbol = nullptr;
        node.group_symbol = nullptr;

        if (!substitutions.empty())
        {
            node.template_params.clear();
        }
    }
};

} // namespace Wasp
