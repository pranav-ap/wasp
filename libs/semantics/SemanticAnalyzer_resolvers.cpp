#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Resolvers
// ============================================================================

Symbol_ptr SemanticAnalyzer::get_module_member_symbol(MemberAccess& access)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.member() only."
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& member_identifier = access.right->as<Identifier>();

    Symbol_ptr unresolved_module_symbol = current_scope->lookup_required(
        module_identifier.name
    );

    bind_identifier(module_identifier, unresolved_module_symbol);

    Symbol_ptr resolved_module_symbol = unresolved_module_symbol->resolve();
    auto& module_data = resolved_module_symbol->as<ModuleSymbol>();

    Symbol_ptr member_symbol = module_data.mod->get_member(member_identifier.name);

    Doctor::get().fatal_if_nullptr(
        member_symbol,
        WaspStage::Semantics,
        "Member '" + member_identifier.name + "' not found in module."
    );

    access.member_index = module_data.mod->get_member_index(member_identifier.name);

    return member_symbol;
}

Symbol_ptr SemanticAnalyzer::resolve_target_symbol(Expression_ptr target)
{
    return std::visit(
        overloaded{
            [&](Identifier& id) -> Symbol_ptr
            {
                auto symbol = current_scope->lookup_required_and_resolve(
                    id.name
                );

                bind_identifier(id, symbol);
                return symbol;
            },
            [&](MemberAccess& access) -> Symbol_ptr
            {
                auto member_symbol = get_module_member_symbol(access);
                access.right->as<Identifier>().symbol = member_symbol;
                return member_symbol;
            },
            [&](auto&) -> Symbol_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid target expression."
                );
            }
        },
        target->data
    );
}

// ============================================================================
// Identifiers & Member Access
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr symbol = current_scope->lookup_required(identifier.name);
    bind_identifier(identifier, symbol);
    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr)
{
    Doctor::get().assert(
        expr.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier."
    );

    auto enum_type_object = try_resolve_as_enum(expr);

    if (enum_type_object.has_value())
    {
        return enum_type_object.value();
    }

    Object_ptr left_type = visit(expr.left);

    return resolve_member_access(
        expr,
        left_type,
        expr.right->as<Identifier>().name
    );
}

Object_ptr SemanticAnalyzer::resolve_member_access(
    MemberAccess& ma,
    Object_ptr target_type,
    const std::string& member_name
)
{
    return std::visit(
        overloaded{
            [&](ModuleType_ptr type) -> Object_ptr
            {
                ma.is_module_access = true;
                ma.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },

            [&](ClassType_ptr type) -> Object_ptr
            {
                Doctor::get().assert(
                    type->contains_member(member_name),
                    WaspStage::Semantics,
                    "Class " + type->name + " has no member named " +
                        member_name
                );

                ma.is_field = type->is_field(member_name);

                if (ma.is_field)
                {
                    ma.member_index = type->record_type->get_index(member_name);
                    return type->record_type->get_type(member_name);
                }

                ma.member_index = type->bag_type->get_index(member_name);
                return type->bag_type->get_signatures(member_name);
            },

            [&](TraitType_ptr type) -> Object_ptr
            {
                ma.is_trait_dispatch = true;

                Doctor::get().assert(
                    type->contains_member(member_name),
                    WaspStage::Semantics,
                    "Trait " + type->name + " has no member named " +
                        member_name
                );

                ma.is_field = type->is_field(member_name);

                if (ma.is_field)
                {
                    ma.member_index = type->record_type->get_index(member_name);
                    return type->record_type->get_type(member_name);
                }

                ma.member_index = type->bag_type->get_index(member_name);
                return type->bag_type->get_signatures(member_name);
            },

            [&](GenericType_ptr type) -> Object_ptr
            {
                Doctor::get().fatal_if_nullptr(
                    type->constraint_type,
                    WaspStage::Semantics
                );

                return resolve_member_access(
                    ma,
                    type->constraint_type,
                    member_name
                );
            },

            [&](VariantType_ptr variant) -> Object_ptr
            {
                ObjectVector result_types;

                for (const auto& t : variant->types)
                {
                    result_types.push_back(
                        resolve_member_access(ma, t, member_name)
                    );
                }

                auto resolved_type = type_system->unify(
                    current_scope,
                    result_types
                );

                return resolved_type;
            },

            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid LHS for a member access : " + member_name
                );
            }
        },
        target_type->value
    );
}

Object_ptr SemanticAnalyzer::visit(TemplateAngular& node)
{
    // Resolve angular arguments (e.g., <int>)
    ObjectVector angular_arguments;
    for (const auto& arg_node : node.angular_nodes)
    {
        angular_arguments.push_back(visit(arg_node));
    }

    // Resolve the target symbol (e.g., 'greet' or 'Box')
    Symbol_ptr target_symbol = resolve_target_symbol(node.target);
    node.symbol = target_symbol;

    // Case A: Template Classes
    if (target_symbol->is<OopsSymbol>())
    {
        Object_ptr base = target_symbol->get_type();
        auto names = type_system->get_generics_declaration_order(base);

        Doctor::get().assert(
            names.size() == angular_arguments.size(),
            WaspStage::Semantics,
            "Generic argument count mismatch for class '" + target_symbol->name +
                "'. Expected " + std::to_string(names.size()) + ", got " +
                std::to_string(angular_arguments.size()) + "."
        );

        ObjectStringMap substitutions;
        for (size_t i = 0; i < names.size(); ++i)
        {
            substitutions[names[i]] = angular_arguments[i];
        }

        std::string specialized_name = target_symbol->name + "_" +
                                       Object::mangle_object(angular_arguments);

        if (auto existing_symbol = current_scope->lookup(specialized_name))
        {
            node.symbol = existing_symbol->resolve();
            return node.symbol->get_type();
        }

        Symbol_ptr specialized_class = monomorphize_class_template(
            target_symbol,
            substitutions,
            specialized_name
        );

        node.symbol = specialized_class;
        return specialized_class->get_type();
    }

    // Case B: Template Functions (Overloads)
    if (target_symbol->is<OverloadsSymbol>())
    {
        auto candidates = target_symbol->as<OverloadsSymbol>()
                              .get_overloads_with_indices();

        // specialize_candidates must filter by generic count AND verify
        // that 'int' satisfies the 'int | str' constraint.
        auto result = type_system->specialize_candidates(
            candidates,
            angular_arguments
        );

        Doctor::get().assert(
            !result.signatures.empty(),
            WaspStage::Semantics,
            "No matching template overload found for '" + target_symbol->name +
                "' with the provided type arguments."
        );

        Doctor::get().assert(
            result.signatures.size() == 1,
            WaspStage::Semantics,
            "Ambiguous generic reference for '" + target_symbol->name + "'."
        );

        node.overload_index = result.original_indices[0];

        return result.signatures[0];
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Target '" + target_symbol->name + "' does not support template angulars."
    );
}

Symbol_ptr SemanticAnalyzer::monomorphize_class_template(
    Symbol_ptr blueprint_symbol,
    const ObjectStringMap& substitutions,
    const std::string& specialized_name
)
{
    auto ast = forest[blueprint_symbol];
    Doctor::get().fatal_if_nullptr(
        ast,
        WaspStage::Semantics,
        "AST not found for symbol: " + blueprint_symbol->name
    );

    ASTCloner cloner(substitutions);
    Statement_ptr specialized_stmt = cloner.clone(ast);

    Symbol_ptr specialized_symbol = nullptr;

    SymbolScope_ptr previous_scope = current_scope;

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
                class_type->template_type = std::make_shared<TemplateType>();

                visit(def);
                forest[def.symbol] = specialized_stmt;

                specialized_symbol = def.symbol;
            },
            [&](TraitDefinition& def)
            {
                def.name = specialized_name;
                def.template_params.clear();

                auto type = make_object(std::make_shared<TraitType>(def.name));
                def.symbol = current_scope->define(
                    SymbolFactory::create_oops(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    )
                );

                auto trait_type = type->as<TraitType_ptr>();
                trait_type->template_type = std::make_shared<TemplateType>();

                visit(def);
                forest[def.symbol] = specialized_stmt;

                specialized_symbol = def.symbol;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected a class or trait definition for monomorphization"
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
