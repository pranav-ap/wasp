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
    // Do nothing
}

void SemanticAnalyzer::leave_scope()
{
    // Do nothing
}

std::tuple<std::string, Object_ptr> SemanticAnalyzer::deconstruct_type_pattern(Expression_ptr expression)
{
    return {"", nullptr};
}

bool SemanticAnalyzer::any_eq(ObjectVector vec, Object_ptr x)
{
    return false;
}

ObjectVector SemanticAnalyzer::remove_duplicates(ObjectVector vec)
{
    return {};
}

}