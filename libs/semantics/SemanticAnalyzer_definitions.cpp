#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(ClassDefinition& class_def)
{
    ObjectStringMap member_types;
    for (const auto& [name, type_ann] : class_def.members)
    {
        member_types[name] = visit(type_ann);
    }

    auto class_type = make_object(
        std::make_shared<ClassType>(
            class_def.name,
            std::move(member_types),
            class_def.members_declaration_order
        )
    );

    if (class_def.symbol)
    {
        // Top Level Class: Hoister already defined it. Just attach the type.
        class_def.symbol->set_type(class_type);
    }
    else
    {
        // Local nested class: Create and define the symbol now.
        class_def.symbol = SymbolFactory::create_class(
            class_def.name,
            class_type,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        Doctor::get().assert(
            !current_scope->contains_in_current_scope(class_def.name),
            WaspStage::Semantics,
            "Redefinition of symbol " + class_def.name
        );

        current_scope->define(class_def.symbol);
    }
}

void SemanticAnalyzer::visit(ImplDefinition& impl_def)
{
    Symbol_ptr class_symbol = current_scope->lookup(impl_def.class_name);

    Doctor::get().assert(
        class_symbol && class_symbol->payload_is<ClassData>(),
        WaspStage::Semantics,
        "Impl block target '" + impl_def.class_name + "' is not a defined class."
    );

    auto class_type_obj = class_symbol->get_type();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    Object_ptr prev_my_type = current_my_instance_type;
    current_my_instance_type = impl_def.is_our ? nullptr : class_type_obj;

    // -------------------------------------------------------------------
    // PASS 1: Hoisting
    // -------------------------------------------------------------------

    for (auto& stmt : impl_def.methods)
    {
        Doctor::get().assert(
            stmt->is<FunctionDefinition>(),
            WaspStage::Semantics,
            "Impl blocks can only contain function definitions."
        );

        auto& method_def = stmt->as<FunctionDefinition>();

        std::string original_name = method_def.name;
        method_def.name = impl_def.class_name + "::" + original_name;

        auto [ret_type, param_types] = evaluate_signature(method_def);
        auto signature = make_object(std::make_shared<FunctionType>(param_types, ret_type));

        auto method_symbol = SymbolFactory::create_function(
            method_def.name,
            signature,
            false,
            current_my_instance_type,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        if (current_scope->contains_in_current_scope(method_def.name))
        {
            type_checker->validate_overload_group(current_scope, method_def.name, method_symbol);
        }

        current_scope->define(method_symbol);
        method_def.symbol = method_symbol;
        method_def.group_symbol = current_scope->lookup(method_def.name);

        if (!class_type->contains_member(original_name))
        {
            class_type->methods_declaration_order.push_back(original_name);
        }

        class_type->members[original_name] = method_def.group_symbol->get_type();
    }

    // -------------------------------------------------------------------
    // PASS 2: Methods Analysis
    // -------------------------------------------------------------------

    for (auto& method_stmt : impl_def.methods)
    {
        visit(method_stmt);
    }

    current_my_instance_type = prev_my_type;
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
