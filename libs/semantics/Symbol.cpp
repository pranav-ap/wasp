#include "Symbol.h"
#include "AST.h"
#include "Doctor.h"
#include "Type.h"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace Wasp
{

// ============================================================================
// Constructor
// ============================================================================

Symbol::Symbol(int id, std::string name, int closure_depth, int lexical_depth, SymbolVariant payload)
    : name(std::move(name)), mangled_name(name), id(id), closure_depth(closure_depth),
      lexical_depth(lexical_depth), payload(std::move(payload))
{
}

// ============================================================================
// Methods
// ============================================================================

std::string Symbol::get_mangled_name() const
{
    if (mangled_name.empty())
    {
        return name;
    }

    return mangled_name;
}

Type_ptr Symbol::get_type() const
{
    return std::visit(
        [&](auto&& arg) -> Type_ptr
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SymbolAliasSymbol>)
            {
                return arg.target->get_type();
            }

            if constexpr (requires { arg.type; })
            {
                return arg.type;
            }

            Doctor::semantics().fatal("Symbol '" + name + "' does not have a type attribute");
        },
        payload
    );
}

void Symbol::set_type(Type_ptr new_type)
{
    std::visit(
        [this, new_type](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SymbolAliasSymbol>)
            {
                arg.target->set_type(new_type);
            }
            else if constexpr (requires { arg.type; })
            {
                arg.type = new_type;
            }
            else
            {
                Doctor::semantics().fatal("Cannot set type for symbol: " + name);
            }
        },
        payload
    );
}

Symbol_ptr Symbol::resolve()
{
    if (is<SymbolAliasSymbol>())
    {
        return as<SymbolAliasSymbol>().target;
    }

    return shared_from_this();
}

bool Symbol::is_global() const
{
    return lexical_depth == 0;
}

bool Symbol::is_exportable() const
{
    return is_global();
}

bool Symbol::should_be_captured(int usage_depth) const
{
    return usage_depth > closure_depth;
}

std::string Symbol::to_string() const
{
    return name + " (id=" + std::to_string(id) + ")";
}

} // namespace Wasp
