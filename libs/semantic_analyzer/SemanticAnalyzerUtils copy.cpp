#include "SemanticAnalyzer.h"
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

namespace Wasp {

void SemanticAnalyzer::enter_scope(ScopeType scope_type)
{
    current_scope = std::make_shared<SymbolScope>(current_scope, scope_type);
}

void SemanticAnalyzer::leave_scope()
{
    if (current_scope->enclosing_scope.has_value()) {
        current_scope = current_scope->enclosing_scope.value();
    } else {
        FATAL("Attempted to leave the global scope.");
    }
}

std::tuple<std::string, Object_ptr> SemanticAnalyzer::deconstruct_type_pattern(Expression_ptr expression)
{
    FATAL("Deconstruct type pattern not implemented");
    return {"", nullptr};
}

bool SemanticAnalyzer::any_eq(ObjectVector vec, Object_ptr x)
{
    return std::any_of(vec.begin(), vec.end(), [&](Object_ptr item) {
        return type_system->equal(current_scope, item, x);
    });
}

ObjectVector SemanticAnalyzer::remove_duplicates(ObjectVector vec)
{
    ObjectVector unique_vec;
    for (auto& item : vec) {
        if (!any_eq(unique_vec, item)) {
            unique_vec.push_back(item);
        }
    }
    return unique_vec;
}

}
