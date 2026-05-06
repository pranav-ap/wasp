#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

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

bool SemanticAnalyzer::prepare_generic_scope(const ObjectStringMap& generics)
{
    if (generics.empty())
    {
        return false;
    }

    enter_scope(ScopeType::TEMPLATE);

    for (const auto& [name, generic_type] : generics)
    {
        auto symbol = SymbolFactory::create_generic(name, generic_type);
        current_scope->define(symbol);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Functions & Methods
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();
    bool has_generics = prepare_generic_scope(signature->generics);

    bool parameters_are_mutable = !def.is_pure;
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    Doctor::get().assert(
        def.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty"
    );

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            signature->parameter_types[i],
            parameters_are_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        def.parameter_symbols.push_back(symbol);
    }

    if (def.body.size() == 1 && def.body.front()->is<Native>())
    {
        def.symbol->mark_as_native();
    }

    visit(def.body);
    return_type_stack.pop_back();

    leave_scope(); // Function Scope
    if (has_generics)
    {
        leave_scope(); // Template Scope
    }
}

// ---------------------------------------------------------------------------
// Classes & Traits
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = def.symbol->get_type()->as<ClassType_ptr>();
    bool has_generics = prepare_generic_scope(class_type->generics);

    // --- Pass 0: Fill up class type ---
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& f)
                {
                    Doctor::get().assert(
                        std::find(
                            class_type->fields.begin(),
                            class_type->fields.end(),
                            f.name
                        ) == class_type->fields.end(),
                        WaspStage::Semantics,
                        "Duplicate field name '" + f.name + "' in class."
                    );

                    auto field_type = visit(f.type);
                    class_type->fields.push_back(f.name);
                    class_type->member_types[f.name] = field_type;
                },
                [&](MethodDefinition& m)
                {
                    auto push_unique = [](StringVector& vec, const std::string& name)
                    {
                        if (std::find(vec.begin(), vec.end(), name) == vec.end())
                        {
                            vec.push_back(name);
                        }
                    };

                    push_unique(class_type->methods, m.name);

                    if (m.is_pure)
                    {
                        push_unique(class_type->pures, m.name);
                    }

                    if (m.is_static)
                    {
                        push_unique(class_type->statics, m.name);
                    }
                },
                [](auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "Invalid statement in class body."
                    );
                }
            },
            stmt->data
        );
    }

    auto class_type_obj = make_object(class_type);

    // --- Pass 1: Hoisting ---
    for (auto& stmt : def.members)
    {
        if (auto* method_def = stmt->try_as<MethodDefinition>())
        {
            Object_ptr return_type = method_def->return_type
                                         ? visit(method_def->return_type)
                                         : workspace->pool->get_none_type();

            ObjectVector param_types;

            for (const auto& [name, type_node] : method_def->parameters)
            {
                param_types.push_back(visit(type_node));
            }

            auto signature = make_object(
                std::make_shared<Signature>(
                    param_types,
                    return_type,
                    class_type->generics,
                    class_type->expected_generic_names_order
                )
            );

            method_def->symbol = SymbolFactory::create_method(
                method_def->name,
                signature,
                false, // can become true once we analyse body
                current_scope->get_closure_depth(),
                current_scope->get_lexical_depth()
            );

            class_type->add_overload(method_def->name, signature);
        }
    }

    // --- Pass 2: Analysis ---
    for (auto& stmt : def.members)
    {
        if (auto* method_def = std::get_if<MethodDefinition>(&stmt->data))
        {
            auto signature = method_def->symbol->get_type()->as<Signature_ptr>();
            bool has_method_generics = prepare_generic_scope(signature->generics);

            enter_scope(
                method_def->is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD
            );

            return_type_stack.push_back(signature->return_type);

            auto define_param = [&](const std::string& name, Object_ptr type)
            {
                auto symbol = SymbolFactory::create_variable(
                    name,
                    type,
                    !method_def->is_pure,
                    current_scope->get_closure_depth(),
                    current_scope->get_lexical_depth()
                );

                current_scope->define(symbol);
                method_def->parameter_symbols.push_back(symbol);
            };

            if (method_def->is_static)
            {
                define_param("our", class_type_obj);
            }
            else
            {
                define_param("my", class_type_obj);
            }

            for (size_t i = 0; i < method_def->parameters.size(); ++i)
            {
                define_param(
                    method_def->parameters[i].first,
                    signature->parameter_types[i]
                );
            }

            visit(method_def->body);
            return_type_stack.pop_back();

            leave_scope();

            if (has_method_generics)
            {
                leave_scope();
            }
        }
    }

    if (has_generics)
    {
        leave_scope();
    }
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Trait definitions are not supported for now"
    );
}

} // namespace Wasp
