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

    // Handle template parameters (template_type is now TemplateType_ptr, not
    // optional)
    if (oop_type->template_type)
    {
        for (const auto& name :
             oop_type->template_type->ordered_parameter_names)
        {
            auto generic_type_obj = oop_type->template_type->template_parameters
                                        .at(name);

            Doctor::get().assert(
                generic_type_obj->is<GenericType_ptr>(),
                WaspStage::Semantics,
                "Expected GenericType for template parameter: " + name
            );

            auto symbol = SymbolFactory::create_template_parameter(
                name,
                generic_type_obj
            );

            current_scope->define(symbol);
        }
    }

    resolve_traits(def, oop_type);
    fill_oops_member_names(def, oop_type);
    hoist_methods(def, oop_type);
    inherit_default_methods(def, oop_type);
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
                        !oop_type->record_type.field_types.contains(f.name),
                        WaspStage::Semantics,
                        "Duplicate field '" + f.name + "'."
                    );

                    auto field_type = visit(f.type);
                    oop_type->record_type.field_types[f.name] = field_type;
                    oop_type->record_type.ordered_keys.push_back(f.name);
                },
                [&](const MethodDefinition& m)
                {
                    push_unique(oop_type->bag_type.ordered_keys, m.name);

                    // Store method signature placeholder (will be filled in
                    // hoist_methods) For now, just mark that it exists
                    if (!oop_type->bag_type.overload_types.contains(m.name))
                    {
                        oop_type->bag_type
                            .overload_types[m.name] = workspace->pool
                                                          ->get_none_type();
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

void SemanticAnalyzer::hoist_methods(
    AbstractOopsDefinition& def,
    OopsType_ptr oop_type
)
{
    for (auto& stmt : def.members)
    {
        if (auto* method = stmt->try_as<MethodDefinition>())
        {
            ObjectVector param_types;
            for (const auto& param : method->parameters)
            {
                param_types.push_back(visit(param.type));
            }

            auto return_type = method->return_type
                                   ? visit(method->return_type)
                                   : workspace->pool->get_none_type();

            auto signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    return_type,
                    oop_type->template_type
                )
            );

            method->symbol = SymbolFactory::create_function(
                method->name,
                signature,
                current_scope->get_closure_depth(),
                current_scope->get_lexical_depth()
            );

            // Store in bag_type
            oop_type->bag_type.overload_types[method->name] = signature;
        }
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

        auto& trait_data = trait_symbol->as<OopsSymbol>();
        auto trait_ast = trait_data.definition->try_as<TraitDefinition>();
        Doctor::get().fatal_if_nullptr(trait_ast, WaspStage::Semantics);

        for (const auto& method_name : trait->bag_type.ordered_keys)
        {
            // Get trait method signatures
            auto trait_sig_obj = trait->bag_type.overload_types.at(method_name);
            Doctor::get().assert(
                trait_sig_obj->is<Signature_ptr>(),
                WaspStage::Semantics,
                "Expected signature for trait method"
            );

            // Check if class already implements this method
            bool is_implemented = oop_type->bag_type.overload_types.contains(
                method_name
            );

            if (!is_implemented)
            {
                // Find the method AST in the trait definition
                Statement_ptr source_stmt_ptr = nullptr;
                MethodDefinition* source_method_ast = nullptr;

                for (auto& stmt : trait_ast->members)
                {
                    if (auto* m = stmt->try_as<MethodDefinition>())
                    {
                        if (m->name == method_name)
                        {
                            source_stmt_ptr = stmt;
                            source_method_ast = m;
                            break;
                        }
                    }
                }

                Doctor::get().fatal_if_nullptr(
                    source_method_ast,
                    WaspStage::Semantics
                );

                bool is_required = source_method_ast->symbol
                                       ->as<FunctionSymbol>()
                                       .required_in_class;

                Doctor::get().assert(
                    !is_required,
                    WaspStage::Semantics,
                    "Class '" + oop_type->name +
                        "' fails to implement required method '" + method_name +
                        "' from trait '" + trait->name + "'."
                );

                // Clone and add the default implementation
                ASTCloner cloner;
                Statement_ptr cloned_stmt = cloner.clone(source_stmt_ptr);
                auto* cloned_method = cloned_stmt->try_as<MethodDefinition>();

                def.members.push_back(cloned_stmt);

                if (!oop_type->bag_type.overload_types.contains(method_name))
                {
                    oop_type->bag_type.ordered_keys.push_back(method_name);
                    oop_type->bag_type
                        .overload_types[method_name] = trait_sig_obj;
                }

                cloned_method->symbol = SymbolFactory::create_function(
                    cloned_method->name,
                    trait_sig_obj,
                    current_scope->get_closure_depth(),
                    current_scope->get_lexical_depth()
                );

                auto& function_data = cloned_method->symbol
                                          ->as<FunctionSymbol>();
                function_data.definition = cloned_stmt;
                function_data.declaration_scope = current_scope;
            }
        }
    }
}

void SemanticAnalyzer::analyze_methods(AbstractOopsDefinition& def)
{
    auto type_obj = def.symbol->get_type();

    for (auto& stmt : def.members)
    {
        if (auto* method = stmt->try_as<MethodDefinition>())
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

        for (size_t m_idx = 0; m_idx < trait->bag_type.ordered_keys.size();
             ++m_idx)
        {
            const auto& method_name = trait->bag_type.ordered_keys[m_idx];

            Doctor::get().assert(
                oop_type->bag_type.overload_types.contains(method_name),
                WaspStage::Semantics,
                "Class '" + oop_type->name +
                    "' fails to implement required method '" + method_name +
                    "' from trait '" + trait->name + "'."
            );

            auto trait_sig_obj = trait->bag_type.overload_types.at(method_name);
            auto class_sig_obj = oop_type->bag_type.overload_types.at(
                method_name
            );

            Doctor::get().assert(
                trait_sig_obj->is<Signature_ptr>() &&
                    class_sig_obj->is<Signature_ptr>(),
                WaspStage::Semantics,
                "Expected signatures for method: " + method_name
            );

            // Find class method index
            int class_member_idx = -1;
            for (size_t c_idx = 0;
                 c_idx < oop_type->bag_type.ordered_keys.size();
                 ++c_idx)
            {
                if (oop_type->bag_type.ordered_keys[c_idx] == method_name)
                {
                    class_member_idx = static_cast<int>(c_idx);
                    break;
                }
            }

            Doctor::get().assert(
                class_member_idx != -1,
                WaspStage::Semantics,
                "Method '" + method_name + "' not found in class '" +
                    oop_type->name + "'."
            );

            OverloadCoordinate trait_coord{static_cast<int>(m_idx), 0};
            OverloadCoordinate class_coord{class_member_idx, 0};

            oop_type->bag_type.itables[trait_id][trait_coord] = class_coord;
        }
    }
}

Symbol_ptr SemanticAnalyzer::monomorphize_class_template(
    Symbol_ptr blueprint_symbol,
    const ObjectStringMap& substitutions,
    const std::string& specialized_name
)
{
    auto& class_data = blueprint_symbol->as<OopsSymbol>();

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
                class_type->template_type = nullptr; // Clear template params

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
