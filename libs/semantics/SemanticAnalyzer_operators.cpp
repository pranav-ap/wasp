#include "AST.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return type_system->infer(current_scope, visit(expr.operand), expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    Object_ptr left_value = visit(expr.left);
    Object_ptr right_value = visit(expr.right);
    return type_system
        ->infer(current_scope, left_value, expr.op.type, right_value);
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    return type_system->infer(current_scope, visit(expr.operand), expr.op.type);
}

} // namespace Wasp
