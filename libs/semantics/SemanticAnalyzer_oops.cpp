#include <algorithm>
#include <memory>
#include <string>
#include <variant>

#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    analyze_oops_definition(def);
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    analyze_oops_definition(def);
}

void SemanticAnalyzer::analyze_oops_definition(AbstractOopsDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    auto type_obj = def.symbol->get_type();
    BaseOOPType_ptr oop_type;

    if (auto class_type = try_unwrap_ptr<ClassType_ptr>(type_obj))
    {
        oop_type = class_type;
    }
    else if (auto trait_type = try_unwrap_ptr<TraitType_ptr>(type_obj))
    {
        oop_type = trait_type;
    }

    auto [generics, ordered_names] = evaluate_template_params(def.template_params);

    for (const auto& [name, generic_type] : generics)
    {
        auto symbol = SymbolFactory::create_template_parameter(name, generic_type);
        current_scope->define(symbol);
    }

    // ========================================================================
    // --- Pass 0: Trait Resolution ---
    // ========================================================================
    for (const auto& trait_annotation : def.traits)
    {
        Object_ptr resolved_type = visit(trait_annotation);
        resolved_type = unwrap_completely(resolved_type);

        Doctor::get().assert(
            resolved_type->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "A class or trait can only implement/inherit from a Trait."
        );

        auto trait_type = resolved_type->as<TraitType_ptr>();
        oop_type->traits.push_back(make_object(trait_type));
    }

    // --- Pass 1: Structure Filling ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<FieldDefinition>())
        {
            Doctor::get().assert(
                std::find(
                    oop_type->fields.begin(),
                    oop_type->fields.end(),
                    f->name
                ) == oop_type->fields.end(),
                WaspStage::Semantics,
                "Duplicate field '" + f->name + "'."
            );
            oop_type->fields.push_back(f->name);
            oop_type->member_types[f->name] = visit(f->type);
        }
        else if (auto* m = stmt->try_as<MethodDefinition>())
        {
            auto push_unique = [](StringVector& vec, const std::string& name)
            {
                if (std::find(vec.begin(), vec.end(), name) == vec.end())
                {
                    vec.push_back(name);
                }
            };
            push_unique(oop_type->methods, m->name);
            if (m->is_pure)
            {
                push_unique(oop_type->pures, m->name);
            }
            if (m->is_static)
            {
                push_unique(oop_type->statics, m->name);
            }
        }
        else
        {
            Doctor::get().fatal(WaspStage::Semantics, "Invalid OOP body stmt.");
        }
    }

    // --- Pass 2: Hoisting Signatures ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ObjectVector param_types;
            for (const auto& [name, type_node] : f->parameters)
            {
                param_types.push_back(visit(type_node));
            }

            auto signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    f->return_type ? visit(f->return_type)
                                   : workspace->pool->get_none_type(),
                    oop_type->generics,
                    oop_type->expected_generic_names_order
                )
            );

            f->symbol = SymbolFactory::create_method(
                f->name,
                signature,
                false,
                current_scope->get_closure_depth(),
                current_scope->get_lexical_depth()
            );
            oop_type->add_overload(f->name, signature);
        }
    }

    // --- Pass 3: Body Analysis ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ScopeType st = f->is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;
            analyze_callable(*f, st, type_obj, f->is_static);
        }
    }

    leave_scope();
}

Symbol_ptr SemanticAnalyzer::monomorphize_class_template(
    Symbol_ptr blueprint_symbol,
    const ObjectStringMap& substitutions,
    const std::string& specialized_name
)
{
    auto& class_data = blueprint_symbol->get_payload_as<ClassData>();

    ASTCloner cloner(substitutions);
    Statement_ptr specialized_stmt = cloner.clone(class_data.definition);

    Symbol_ptr specialized_symbol = nullptr;

    SymbolScope_ptr previous_scope = current_scope;
    current_scope = class_data.definition_scope;

    std::visit(
        overloaded{
            [&](ClassDefinition& def)
            {
                def.name = specialized_name;
                def.template_params.clear();

                auto type = make_object(std::make_shared<ClassType>(def.name));
                def.symbol = current_scope->define(
                    SymbolFactory::create_class(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    )
                );

                auto class_type = type->as<ClassType_ptr>();
                class_type->generics.clear();
                class_type->expected_generic_names_order.clear();

                visit(def);

                specialized_symbol = def.symbol;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected a class definition"
                );
            }
        },
        specialized_stmt->data
    );

    Doctor::get().fatal_if_nullptr(specialized_symbol, WaspStage::Semantics);

    current_scope = previous_scope;
    pending_templates.push_back(specialized_stmt);

    return specialized_symbol;
}

} // namespace Wasp
