#pragma once

#include "Token.h"
#include "TypeNode.h"
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <map>

namespace Wasp {

struct Identifier { std::string name; };
struct LetExpression { std::unique_ptr<struct Expression> assignment; };
struct ConstExpression { std::unique_ptr<struct Expression> assignment; };
struct UntypedAssignment { std::unique_ptr<struct Expression> lhs; std::unique_ptr<struct Expression> rhs; };
struct TypedAssignment { std::unique_ptr<struct Expression> lhs; std::unique_ptr<struct Expression> rhs; TypeNodePtr type; };

struct ListLiteral { std::vector<std::unique_ptr<struct Expression>> elements; };
struct TupleLiteral { std::vector<std::unique_ptr<struct Expression>> elements; };
struct SetLiteral { std::vector<std::unique_ptr<struct Expression>> elements; };
struct MapLiteral { std::vector<std::pair<std::unique_ptr<struct Expression>, std::unique_ptr<struct Expression>>> pairs; };

struct Prefix { TokenPtr op; std::unique_ptr<struct Expression> operand; };
struct Infix { std::unique_ptr<struct Expression> left; TokenPtr op; std::unique_ptr<struct Expression> right; };
struct Postfix { std::unique_ptr<struct Expression> operand; TokenPtr op; };

struct Call { std::string name; std::vector<std::unique_ptr<struct Expression>> arguments; bool is_builtin; };


using Expression = std::variant<
    std::monostate,
    int, double, std::string, bool,
    Identifier,
    LetExpression, ConstExpression,
    UntypedAssignment, TypedAssignment,
    ListLiteral, TupleLiteral, SetLiteral, MapLiteral,
    Prefix, Infix, Postfix,
    Call,
    struct IfTernaryBranch,
    struct ElseTernaryBranch,
    struct TypePattern,
    struct EnumMember,
    struct Spread,
    struct Is,
    struct As
>;

using ExpressionPtr = std::unique_ptr<Expression>;


struct IfTernaryBranch {
    ExpressionPtr test;
    ExpressionPtr true_expr;
    // Points to another IfTernaryBranch or ElseTernaryBranch
    ExpressionPtr alternative; 
};

struct ElseTernaryBranch {
    ExpressionPtr expression;
};

struct TypePattern {
    ExpressionPtr expression;
    TypeNodePtr type_node;
};

struct EnumMember {
    std::vector<std::string> chain;
};

struct Spread {
    ExpressionPtr expression;
    bool is_rvalue;
};

struct Is {
    ExpressionPtr left;
    TypeNodePtr right;
};

struct As {
    ExpressionPtr left;
    TypeNodePtr right;
};

}