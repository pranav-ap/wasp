#include "Doctor.h"
#include "SemanticAnalyzer.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"


#include <ctime>
#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// SCOPE MANAGEMENT
// ============================================================================

void SemanticAnalyzer::enter_scope(ScopeType scope_type)
{
    current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
}

void SemanticAnalyzer::leave_scope()
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();
    }
}

void SemanticAnalyzer::leave_scope_keep_symbol(Symbol_ptr symbol_to_keep)
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();

        if (current_scope)
        {
            current_scope->define(symbol_to_keep);
        }
    }
}

std::string SemanticAnalyzer::get_operator_symbol_name(
    TokenType fixity,
    TokenType op_type
)
{
    std::string prefix;
    switch (fixity)
    {
    case TokenType::INFIX:
        prefix = "infix_";
        break;
    case TokenType::PREFIX:
        prefix = "prefix_";
        break;
    case TokenType::POSTFIX:
        prefix = "postfix_";
        break;
    default:
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Invalid operator fixity for operator symbol name generation."
        );
    }

    return prefix + to_string(op_type);
}

} // namespace Wasp
