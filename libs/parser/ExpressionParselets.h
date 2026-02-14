#pragma once

#include "Expression.h"
#include <memory>


namespace Wasp {
    class Parser;

    class IPrefixParselet {
    public:
        virtual ~IPrefixParselet() = default;

        virtual Expression_ptr parse(Parser &parser, const Token &token) = 0;
    };

    using IPrefixParselet_ptr = std::shared_ptr<IPrefixParselet>;

    class IInfixParselet {
    public:
        virtual ~IInfixParselet() = default;

        virtual Expression_ptr parse(Parser &parser, Expression_ptr left, const Token &token) = 0;

        [[nodiscard]] virtual int get_precedence() const = 0;
    };

    using IInfixParselet_ptr = std::shared_ptr<IInfixParselet>;

    class IdentifierParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser &parser, const Token &token) override;
    };

    class LiteralParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser &parser, const Token &token) override;
    };

    class PrefixOperatorParselet : public IPrefixParselet {
        int precedence;

    public:
        explicit PrefixOperatorParselet(const int precedence)
            : precedence(precedence) {
        }

        Expression_ptr parse(Parser &parser, const Token &token) override;

        [[nodiscard]] int get_precedence() const;
    };

    class InfixOperatorParselet : public IInfixParselet {
        int precedence;
        bool is_right_associative;

    public:
        InfixOperatorParselet(const int precedence, const bool is_right_associative)
            : precedence(precedence), is_right_associative(is_right_associative) {
        };

        Expression_ptr parse(Parser &parser, Expression_ptr left, const Token &token) override;

        [[nodiscard]] int get_precedence() const override;
    };

    class ListParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser& parser, const Token& token);
    };

    class ParenthesisParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser& parser, const Token& token);
    };

    class CurlyBraceParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser& parser, const Token& token);
    }; 

    class TypePatternParselet : public IInfixParselet {
    public:
        Expression_ptr parse(Parser& parser, Expression_ptr left, const Token& token) override;
        [[nodiscard]] int get_precedence() const override;
    };
    
    class AssignmentParselet : public IInfixParselet
    {
    public:
        Expression_ptr parse(Parser &parser, Expression_ptr left, const Token &token) override;
        [[nodiscard]] int get_precedence() const override;
    };

    class TernaryConditionParselet : public IPrefixParselet {
    public:
        Expression_ptr parse(Parser& parser, const Token& token);
        [[nodiscard]] int get_precedence() const;
    };
}
