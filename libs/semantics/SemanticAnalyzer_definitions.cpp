#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Workspace.h"

#include <string>
#include <utility>

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

    auto class_type = make_object(ClassType(
        class_definition.name,
        std::move(member_types),
        std::move(class_definition.members_declaration_order)
    ));

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

void SemanticAnalyzer::visit(ImplDefinition& statement)
{
    Symbol_ptr class_symbol = current_scope->lookup(statement.class_name);

    Doctor::get().assert(
        class_symbol != nullptr && class_symbol->payload_is<ClassData>(),
        WaspStage::Semantics,
        "Impl block target '" + statement.class_name + "' is not a defined class."
    );

    auto class_type_obj = class_symbol->get_type();
    auto& class_type = class_type_obj->as<ClassType>();

    Object_ptr previous_bound_type = current_bound_instance_type;
    current_bound_instance_type = class_type_obj;

    for (auto& method_stmt : statement.methods)
    {
        Doctor::get().assert(
            method_stmt->is<FunctionDefinition>(),
            WaspStage::Semantics,
            "Impl blocks can only contain function definitions."
        );

        auto& method_def = method_stmt->as<FunctionDefinition>();

        std::string original_name = method_def.name;
        method_def.name = statement.class_name + "::" + original_name;

        visit(method_def);

        Object_ptr method_type = method_def.group_symbol->get_type();
        class_type.members[original_name] = method_type;
        class_type.methods_declaration_order.push_back(original_name);
    }

    current_bound_instance_type = previous_bound_type;
}

void SemanticAnalyzer::visit(TraitDefinition& statement)
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
