#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Doctor.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(ClassDefinition& class_definition)
{
    ObjectStringMap member_types;

    for (const auto& [member_name, type_annotation] : class_definition.members)
    {
        Object_ptr member_type = visit(type_annotation);
        member_types[member_name] = member_type;
    }

    auto class_type = make_object(
        ClassType(class_definition.name, std::move(member_types))
    );

    Symbol_ptr actual_class_symbol;

    if (class_definition.symbol)
    {
        // Top Level Class: The Hoister already found it and put it in the scope.
        // We just need to attach the fully resolved type to it.
        actual_class_symbol = class_definition.symbol;
        actual_class_symbol->set_type(class_type);
    }
    else
    {
        // Local nested class: We need to create the symbol right now.
        actual_class_symbol = SymbolFactory::create_class(
            class_definition.name,
            class_type,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        Doctor::get().assert(
            !current_scope->contains_in_current_scope(class_definition.name),
            WaspStage::Semantics,
            "Redefinition of symbol " + class_definition.name
        );

        current_scope->define(actual_class_symbol);
        class_definition.symbol = actual_class_symbol;
    }
}

void SemanticAnalyzer::visit(TraitDefinition& statement)
{
}

void SemanticAnalyzer::visit(ImplDefinition& statement)
{
}

void SemanticAnalyzer::visit(AliasDefinition& statement)
{
}

void SemanticAnalyzer::visit(EnumDefinition& statement)
{
}

void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

} // namespace Wasp
