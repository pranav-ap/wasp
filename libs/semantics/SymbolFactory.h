#pragma once

#include "Symbol.h"
#include "Type.h"

#include <string>

namespace Wasp
{

class SymbolFactory
{
private:
    static int symbol_id_counter;

    static Symbol_ptr create_symbol(
        const std::string& name,
        SymbolVariant&& payload,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_symbol(SymbolVariant&& payload);

public:
    static void reset_counter();
    static int get_current_id();

    static Symbol_ptr create_variable(
        const std::string& name,
        Type_ptr type,
        bool is_mutable = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_function(
        const std::string& name,
        Type_ptr type,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_type(
        const std::string& name,
        Type_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_type_alias(
        const std::string& name,
        Type_ptr type,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_symbol_alias(
        const std::string& name,
        Symbol_ptr target,
        int closure_depth = 0,
        int lexical_depth = 0
    );
};

} // namespace Wasp
