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

void SemanticAnalyzer::run(struct Module& ast)
{
    enter_scope(ScopeType::MODULE);
    visit(ast.statements);
    leave_scope();
}
}
