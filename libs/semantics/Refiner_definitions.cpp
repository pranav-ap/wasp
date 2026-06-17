#include "AST.h"
#include "Doctor.h"
#include "Refiner.h"
#include "Statement.h"
#include "Type.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Refiner::visit(FunctionDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
}

void Refiner::visit(OperatorDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);
}

void Refiner::visit(ClassDefinition& def)
{
    current_scope->define(def.symbol);

    FunctionDefinitionVector methods = def.methods;
}

void Refiner::visit(TraitDefinition& def)
{
    current_scope->define(def.symbol);
}

void Refiner::visit(PrimitiveDefinition& def)
{
    current_scope->define(def.symbol);
}

void Refiner::visit(EnumDefinition& def)
{
    current_scope->define(def.symbol);
}

void Refiner::visit(TypeAliasDefinition& def)
{
    current_scope->define(def.symbol);
}

} // namespace Wasp
