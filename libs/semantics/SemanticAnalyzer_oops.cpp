#include <algorithm>
#include <memory>
#include <string>

#include "AST.h"
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

    bool has_generics = prepare_generic_scope(oop_type->generics);

    // --- Pass 0: Structure Filling ---
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

    // --- Pass 1: Hoisting Signatures ---
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

    // --- Pass 2: Body Analysis ---
    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ScopeType st = f->is_pure ? ScopeType::PURE_METHOD
                                      : ScopeType::METHOD;
            analyze_callable(*f, st, type_obj, f->is_static);
        }
    }

    if (has_generics)
    {
        leave_scope();
    }
}

} // namespace Wasp
