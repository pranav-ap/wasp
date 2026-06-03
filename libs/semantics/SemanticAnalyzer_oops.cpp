#include <cstddef>
#include <memory>
#include <string>
#include <variant>

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
    auto type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(type_obj, WaspStage::Semantics);

    Doctor::get().assert(
        type_obj->is<ClassType_ptr>(),
        WaspStage::Semantics,
        "Expected a class type in the class definition"
    );

    auto class_type = type_obj->as<ClassType_ptr>();

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

    auto trait_type = type_obj->as<TraitType_ptr>();

    analyze_oops_definition(def, trait_type);
}

void SemanticAnalyzer::analyze_oops_definition(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    enter_scope(ScopeType::CLASS);

    define_template_parameters(oop_type->template_type);

    resolve_traits(def, oop_type);
    inherit_default_methods(def, oop_type);
    fill_oops_member_names(def, oop_type);
    hoist_methods(def, oop_type);
    analyze_methods(def);
    check_trait_conformance(oop_type);

    leave_scope();
}

void SemanticAnalyzer::resolve_traits(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    for (const auto& trait_annotation : def.traits)
    {
        Object_ptr resolved_type = visit(trait_annotation);
        resolved_type = resolved_type->unwrap_completely();

        Doctor::get().assert(
            resolved_type->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "A Class or Trait can only implement a Trait."
        );

        auto trait_type = resolved_type->as<TraitType_ptr>();
        oop_type->traits.push_back(make_object(trait_type));
    }
}

void SemanticAnalyzer::fill_oops_member_names(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](const FieldDefinition& f)
                {
                    Doctor::get().assert(
                        !oop_type->record_type->types.contains(f.name),
                        WaspStage::Semantics,
                        "Duplicate field '" + f.name + "'."
                    );

                    auto field_type = visit(f.type);

                    Doctor::get().fatal_if_nullptr(
                        field_type,
                        WaspStage::Semantics,
                        "Field '" + f.name + "' has invalid type"
                    );

                    oop_type->record_type->types[f.name] = field_type;
                    oop_type->record_type->ordered_keys.push_back(f.name);
                },
                [&](const FunctionDefinition& m)
                {
                    if (!oop_type->bag_type->types.contains(m.name))
                    {
                        oop_type->bag_type->ordered_keys.push_back(m.name);

                        auto empty_signatures_set = make_object(
                            std::make_shared<SignaturesSet>()
                        );

                        oop_type->bag_type
                            ->types[m.name] = empty_signatures_set;
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

void SemanticAnalyzer::inherit_default_methods(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    for (const auto& trait_obj : oop_type->traits)
    {
        auto trait = trait_obj->as<TraitType_ptr>();

        Symbol_ptr trait_symbol = current_scope->lookup(trait->name);
        Doctor::get().fatal_if_nullptr(trait_symbol, WaspStage::Semantics);

        auto ast = forest[trait_symbol];
        Doctor::get().fatal_if_nullptr(ast, WaspStage::Semantics);

        auto trait_ast = ast->try_as<TraitDefinition>();
        Doctor::get().fatal_if_nullptr(trait_ast, WaspStage::Semantics);

        def.members.insert(
            def.members.end(),
            trait_ast->members.begin(),
            trait_ast->members.end()
        );
    }
}

void SemanticAnalyzer::hoist_methods(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    for (auto& stmt : def.members)
    {
        if (auto* method = stmt->try_as<FunctionDefinition>())
        {
            ObjectVector param_types;
            for (const auto& param : method->parameters)
            {
                param_types.push_back(visit(param.type));
            }

            auto return_type = method->return_type
                                   ? visit(method->return_type)
                                   : workspace->pool->get_none_type();

            auto new_signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    return_type,
                    oop_type->template_type
                )
            );

            if (method->symbol)
            {
                method->symbol->set_type(new_signature);
            }
            else
            {
                method->symbol = SymbolFactory::create_function(
                    method->name,
                    new_signature,
                    current_scope->get_closure_depth(),
                    current_scope->get_lexical_depth()
                );
            }

            Doctor::get().assert(
                oop_type->bag_type->types.contains(method->name),
                WaspStage::Semantics,
                "Expected method '" + method->name + "' to be in bag_type"
            );

            auto signatures_obj = oop_type->bag_type->get_signatures(
                method->name
            );

            Doctor::get().assert(
                signatures_obj->is<SignaturesSet_ptr>(),
                WaspStage::Semantics,
                "Expected signatures set for method " + method->name
            );

            auto signatures = signatures_obj->as<SignaturesSet_ptr>();

            validate_method_signature(
                signatures,
                new_signature->as<Signature_ptr>(),
                method->name,
                def.name
            );

            signatures->add_signature(new_signature);
        }
    }
}

void SemanticAnalyzer::validate_method_signature(
    const SignaturesSet_ptr& existing_signatures,
    const Signature_ptr& new_signature,
    const std::string& method_name,
    const std::string& class_name
)
{
    for (const auto& existing_sig_obj : existing_signatures->types)
    {
        auto existing_sig = existing_sig_obj->as<Signature_ptr>();

        if (existing_sig->parameter_types.size() !=
            new_signature->parameter_types.size())
        {
            continue;
        }

        bool is_equal = type_system->equal(
            current_scope,
            existing_sig->parameter_types,
            new_signature->parameter_types
        );

        Doctor::get().assert(
            !is_equal,
            WaspStage::Semantics,
            "Duplicate method " + method_name + " in class " + class_name
        );
    }
}

void SemanticAnalyzer::analyze_methods(AbstractOopsDefinition& def)
{
    for (auto& stmt : def.members)
    {
        if (auto* method = stmt->try_as<FunctionDefinition>())
        {
            ScopeType scope_type = method->is_pure ? ScopeType::PURE_METHOD
                                                   : ScopeType::METHOD;

            analyze_callable(*method, scope_type);
        }
    }
}

void SemanticAnalyzer::check_trait_conformance(OopsType_ptr oop_type)
{
    for (const auto& trait_obj : oop_type->traits)
    {
        auto trait = trait_obj->as<TraitType_ptr>();
        int trait_id = trait->type_id;

        for (size_t m_idx = 0; m_idx < trait->bag_type->ordered_keys.size();
             ++m_idx)
        {
            const auto& method_name = trait->bag_type->ordered_keys[m_idx];

            Doctor::get().assert(
                oop_type->bag_type->types.contains(method_name),
                WaspStage::Semantics,
                "Class '" + oop_type->name + "' fails to implement method '" +
                    method_name + "' from trait '" + trait->name + "'."
            );

            auto trait_entry = trait->bag_type->types.at(method_name);
            Doctor::get().assert(
                trait_entry->is<SignaturesSet_ptr>(),
                WaspStage::Semantics
            );

            auto class_entry = oop_type->bag_type->types.at(method_name);
            Doctor::get().assert(
                class_entry->is<SignaturesSet_ptr>(),
                WaspStage::Semantics
            );

            auto trait_set = trait_entry->as<SignaturesSet_ptr>();
            auto class_set = class_entry->as<SignaturesSet_ptr>();

            int class_member_idx = oop_type->bag_type->get_index(method_name);

            Doctor::get().assert(
                class_member_idx != -1,
                WaspStage::Semantics,
                "Method '" + method_name + "' not found in class '" +
                    oop_type->name + "'."
            );

            // For each signature in trait, find matching overload in class
            for (size_t t_idx = 0; t_idx < trait_set->types.size(); ++t_idx)
            {
                auto trait_sig = trait_set->types[t_idx]->as<Signature_ptr>();
                bool found_match = false;

                for (size_t c_idx = 0; c_idx < class_set->types.size(); ++c_idx)
                {
                    auto class_sig = class_set->types[c_idx]
                                         ->as<Signature_ptr>();

                    // Skip the 'self' parameter (first parameter) for both
                    size_t trait_param_start = trait_sig->parameter_types
                                                       .empty()
                                                   ? 0
                                                   : 1;
                    size_t class_param_start = class_sig->parameter_types
                                                       .empty()
                                                   ? 0
                                                   : 1;

                    size_t trait_param_count = trait_sig->parameter_types
                                                   .size() -
                                               trait_param_start;
                    size_t class_param_count = class_sig->parameter_types
                                                   .size() -
                                               class_param_start;

                    if (trait_param_count != class_param_count)
                    {
                        continue;
                    }

                    // Compare parameter types (excluding self)
                    bool params_match = true;
                    for (size_t p_idx = 0; p_idx < trait_param_count; ++p_idx)
                    {
                        if (!type_system->equal(
                                current_scope,
                                trait_sig->parameter_types
                                    [trait_param_start + p_idx],
                                class_sig
                                    ->parameter_types[class_param_start + p_idx]
                            ))
                        {
                            params_match = false;
                            break;
                        }
                    }

                    // Compare return types
                    if (params_match && type_system->equal(
                                            current_scope,
                                            trait_sig->return_type,
                                            class_sig->return_type
                                        ))
                    {
                        OverloadCoordinate trait_coord{
                            static_cast<int>(m_idx),
                            static_cast<int>(t_idx)
                        };
                        OverloadCoordinate class_coord{
                            class_member_idx,
                            static_cast<int>(c_idx)
                        };
                        oop_type->bag_type
                            ->itables[trait_id][trait_coord] = class_coord;
                        found_match = true;
                        break;
                    }
                }

                Doctor::get().assert(
                    found_match,
                    WaspStage::Semantics,
                    "Trait '" + trait->name + "' method '" + method_name +
                        "' overload " + std::to_string(t_idx) +
                        " not implemented in class '" + oop_type->name + "'"
                );
            }
        }
    }
}

} // namespace Wasp
