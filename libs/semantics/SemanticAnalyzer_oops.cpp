#include <algorithm>
#include <cstddef>
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
    auto type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(type_obj, WaspStage::Semantics);

    Doctor::get().assert(
        type_obj->is<ClassType_ptr>(),
        WaspStage::Semantics,
        "Expected a class type in the class definition"
    );

    auto& class_type = type_obj->as<ClassType_ptr>();

    analyze_oops_definition(def, class_type);
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    auto type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(type_obj, WaspStage::Semantics);

    Doctor::get().assert(
        type_obj->is<TraitType_ptr>(),
        WaspStage::Semantics,
        "Expected a trait type in the trait definition"
    );

    auto& trait_type = type_obj->as<TraitType_ptr>();

    analyze_oops_definition(def, trait_type);
}

void SemanticAnalyzer::analyze_oops_definition(AbstractOopsDefinition& def, OopsType_ptr oop_type)
{
    enter_scope(ScopeType::CLASS);

    auto [generics, ordered_names] = evaluate_template_params(def.template_params);

    for (const auto& [name, generic_type] : generics)
    {
        auto symbol = SymbolFactory::create_template_parameter(name, generic_type);
        current_scope->define(symbol);
    }

    resolve_traits(def, oop_type);
    fill_oops_member_names(def, oop_type);
    hoist_methods(def, oop_type);
    // inherit_default_methods(def, oop_type);
    analyze_methods(def);
    check_trait_conformance(oop_type);

    leave_scope();
}

void SemanticAnalyzer::resolve_traits(AbstractOopsDefinition& def, OopsType_ptr oop_type)
{
    for (const auto& trait_annotation : def.traits)
    {
        Object_ptr resolved_type = visit(trait_annotation);
        resolved_type = unwrap_completely(resolved_type);

        Doctor::get().assert(
            resolved_type->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "A Class or Trait can only implement a Trait."
        );

        auto trait_type = resolved_type->as<TraitType_ptr>();
        oop_type->traits.push_back(make_object(trait_type));
    }
}

void SemanticAnalyzer::fill_oops_member_names(AbstractOopsDefinition& def, OopsType_ptr oop_type)
{
    auto push_unique = [](StringVector& vec, const std::string& name)
    {
        if (std::find(vec.begin(), vec.end(), name) == vec.end())
        {
            vec.push_back(name);
        }
    };

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](const FieldDefinition& f)
                {
                    Doctor::get().assert(
                        std::find(
                            oop_type->fields.begin(),
                            oop_type->fields.end(),
                            f.name
                        ) == oop_type->fields.end(),
                        WaspStage::Semantics,
                        "Duplicate field '" + f.name + "'."
                    );

                    oop_type->fields.push_back(f.name);
                    oop_type->member_types[f.name] = visit(f.type);
                },
                [&](const MethodDefinition& m)
                {
                    push_unique(oop_type->methods, m.name);

                    if (m.is_pure)
                    {
                        push_unique(oop_type->pures, m.name);
                    }

                    if (m.is_static)
                    {
                        push_unique(oop_type->statics, m.name);
                    }
                },
                [&](const auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "Invalid OOP body stmt."
                    );
                }
            },
            stmt->data
        );
    }
}

void SemanticAnalyzer::hoist_methods(AbstractOopsDefinition& def, OopsType_ptr oop_type)
{
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
                    oop_type->template_parameter_types,
                    oop_type->ordered_template_parameter_names
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
}

void SemanticAnalyzer::inherit_default_methods(AbstractOopsDefinition& def, OopsType_ptr oop_type)
{
    for (const auto& trait_obj : oop_type->traits)
    {
        auto trait = trait_obj->as<TraitType_ptr>();

        Symbol_ptr trait_symbol = current_scope->lookup(trait->name);
        Doctor::get().fatal_if_nullptr(trait_symbol, WaspStage::Semantics);

        auto& trait_data = trait_symbol->get_payload_as<OopsData>();
        auto trait_ast = trait_data.definition->try_as<TraitDefinition>();
        Doctor::get().fatal_if_nullptr(trait_ast, WaspStage::Semantics);

        for (const std::string& method_name : trait->methods)
        {
            auto trait_overloads = trait->get_overloads(method_name);
            auto class_overloads = oop_type->get_overloads(method_name);

            for (size_t t_idx = 0; t_idx < trait_overloads.size(); ++t_idx)
            {
                const auto& trait_sig = trait_overloads[t_idx];
                bool is_implemented = false;

                for (const auto& class_sig : class_overloads)
                {
                    if (type_system->assignable(current_scope, trait_sig, class_sig))
                    {
                        is_implemented = true;
                        break;
                    }
                }

                if (!is_implemented)
                {
                    Statement_ptr source_stmt_ptr = nullptr;
                    MethodDefinition* source_method_ast = nullptr;

                    for (auto& stmt : trait_ast->members)
                    {
                        if (auto* m = stmt->try_as<MethodDefinition>())
                        {
                            if (m->name == method_name &&
                                type_system->equal(current_scope, m->symbol->get_type(), trait_sig))
                            {
                                source_stmt_ptr = stmt;
                                source_method_ast = m;
                                break;
                            }
                        }
                    }

                    Doctor::get().fatal_if_nullptr(source_method_ast, WaspStage::Semantics);

                    bool is_required = source_method_ast->symbol->get_payload_as<CallableData>()
                                           .required_in_class;

                    Doctor::get().assert(
                        !is_required,
                        WaspStage::Semantics,
                        "Class '" + oop_type->name + "' fails to implement required method '" +
                            method_name + "' from trait '" + trait->name + "'."
                    );

                    ASTCloner cloner;
                    Statement_ptr cloned_stmt = cloner.clone(source_stmt_ptr);
                    auto* cloned_method = cloned_stmt->try_as<MethodDefinition>();

                    def.members.push_back(cloned_stmt);

                    if (std::find(
                            oop_type->methods.begin(),
                            oop_type->methods.end(),
                            method_name
                        ) == oop_type->methods.end())
                    {
                        oop_type->methods.push_back(method_name);
                        if (cloned_method->is_pure)
                        {
                            oop_type->pures.push_back(method_name);
                        }
                        if (cloned_method->is_static)
                        {
                            oop_type->statics.push_back(method_name);
                        }
                    }

                    cloned_method->symbol = SymbolFactory::create_method(
                        cloned_method->name,
                        trait_sig,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    auto& function_data = cloned_method->symbol->get_payload_as<CallableData>();
                    function_data.definition = cloned_stmt;
                    function_data.declaration_scope = current_scope;

                    oop_type->add_overload(method_name, trait_sig);
                }
            }
        }
    }
}

void SemanticAnalyzer::analyze_methods(AbstractOopsDefinition& def)
{
    auto type_obj = def.symbol->get_type();

    for (auto& stmt : def.members)
    {
        if (auto* f = stmt->try_as<MethodDefinition>())
        {
            ScopeType scope_type = f->is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;
            analyze_callable(*f, scope_type, type_obj, f->is_static);
        }
    }
}

void SemanticAnalyzer::check_trait_conformance(OopsType_ptr oop_type)
{
    for (const auto& trait_obj : oop_type->traits)
    {
        auto trait = trait_obj->as<TraitType_ptr>();
        int trait_id = trait->type_id;

        for (const std::string& required_method_name : trait->methods)
        {
            Doctor::get().assert(
                oop_type->contains_member(required_method_name),
                WaspStage::Semantics,
                "Class '" + oop_type->name +
                    "' fails to implement required method '" + required_method_name +
                    "' from trait '" + trait->name + "'."
            );

            auto trait_overloads = trait->get_overloads(required_method_name);
            auto class_overloads = oop_type->get_overloads(required_method_name);

            int trait_member_idx = trait->get_member_index(required_method_name);
            int class_member_idx = oop_type->get_member_index(required_method_name);

            for (size_t t_idx = 0; t_idx < trait_overloads.size(); ++t_idx)
            {
                const auto& trait_sig = trait_overloads[t_idx];
                bool signature_matched = false;
                int matched_c_idx = -1;

                for (size_t c_idx = 0; c_idx < class_overloads.size(); ++c_idx)
                {
                    const auto& class_sig = class_overloads[c_idx];

                    if (type_system->assignable(current_scope, trait_sig, class_sig))
                    {
                        signature_matched = true;
                        matched_c_idx = static_cast<int>(c_idx);
                        break;
                    }
                }

                Doctor::get().assert(
                    signature_matched,
                    WaspStage::Semantics,
                    "Signature mismatch in class '" + oop_type->name +
                        "' for method '" + required_method_name +
                        "' required by trait '" + trait->name + "'."
                );

                OverloadCoordinate trait_coord{trait_member_idx, static_cast<int>(t_idx)};
                OverloadCoordinate class_coord{class_member_idx, matched_c_idx};

                oop_type->itables[trait_id][trait_coord] = class_coord;
            }
        }
    }
}

Symbol_ptr SemanticAnalyzer::monomorphize_class_template(
    Symbol_ptr blueprint_symbol,
    const ObjectStringMap& substitutions,
    const std::string& specialized_name
)
{
    auto& class_data = blueprint_symbol->get_payload_as<OopsData>();

    ASTCloner cloner(substitutions);
    Statement_ptr specialized_stmt = cloner.clone(class_data.definition);

    Symbol_ptr specialized_symbol = nullptr;

    SymbolScope_ptr previous_scope = current_scope;
    current_scope = class_data.declaration_scope;

    std::visit(
        overloaded{
            [&](ClassDefinition& def)
            {
                def.name = specialized_name;
                def.template_params.clear();

                auto type = make_object(std::make_shared<ClassType>(def.name));
                def.symbol = current_scope->define(
                    SymbolFactory::create_oops(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    )
                );

                auto class_type = type->as<ClassType_ptr>();
                class_type->template_parameter_types.clear();
                class_type->ordered_template_parameter_names.clear();

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
