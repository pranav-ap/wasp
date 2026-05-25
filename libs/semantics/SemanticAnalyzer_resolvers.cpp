#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <algorithm>
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

void SemanticAnalyzer::bind_identifier(Identifier& id, Symbol_ptr symbol)
{
    id.symbol = symbol;

    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        id.must_be_captured = true;
    }
}

Symbol_ptr SemanticAnalyzer::get_module_member_symbol(MemberAccess& access)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.member() only."
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& member_identifier = access.right->as<Identifier>();

    Symbol_ptr unresolved_module_symbol = current_scope->lookup(
        module_identifier.name
    );
    Doctor::get().fatal_if_nullptr(unresolved_module_symbol, WaspStage::Semantics);

    bind_identifier(module_identifier, unresolved_module_symbol);

    Symbol_ptr resolved_module_symbol = unresolved_module_symbol->resolve();
    auto& module_data = resolved_module_symbol->get_payload_as<ModuleData>();

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
    Symbol_ptr target_symbol = nullptr;

    if (auto* id = target->try_as<Identifier>())
    {
        Symbol_ptr unresolved = current_scope->lookup(id->name);
        Doctor::get().fatal_if_nullptr(
            unresolved,
            WaspStage::Semantics,
            "Undefined identifier: '" + id->name + "'"
        );

        target_symbol = unresolved->resolve();
        bind_identifier(*id, target_symbol);
    }
    else if (auto* access = target->try_as<MemberAccess>())
    {
        auto member_symbol = get_module_member_symbol(*access);
        target_symbol = member_symbol;
        access->right->as<Identifier>().symbol = target_symbol;
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "Invalid target expression.");
    }

    return target_symbol;
}

// ============================================================================
// Identifiers & Member Access
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr symbol = current_scope->lookup(identifier.name);
    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined identifier: " + identifier.name
    );

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
    return resolve_member_access(expr, left_type, expr.right->as<Identifier>().name);
}

StringVector SemanticAnalyzer::unfurl_member_access(const MemberAccess& expr)
{
    StringVector path = {expr.right->as<Identifier>().name};
    Expression_ptr current = expr.left;

    while (current && current->is<MemberAccess>())
    {
        auto& nested_ma = current->as<MemberAccess>();
        if (!nested_ma.right->is<Identifier>())
        {
            break;
        }

        path.push_back(nested_ma.right->as<Identifier>().name);
        current = nested_ma.left;
    }

    if (current && current->is<Identifier>())
    {
        path.push_back(current->as<Identifier>().name);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::optional<Object_ptr> SemanticAnalyzer::try_resolve_as_enum(MemberAccess& ma)
{
    StringVector path = unfurl_member_access(ma);

    if (path.size() < 2)
    {
        return std::nullopt;
    }

    Symbol_ptr base_sym = current_scope->lookup(path.front());
    if (!base_sym || !base_sym->get_type())
    {
        return std::nullopt;
    }

    Object_ptr base_type = base_sym->get_type();
    if (base_type->is<TypeAlias_ptr>())
    {
        base_type = unwrap_type_alias(base_type);
    }

    if (!base_type->is<EnumType_ptr>())
    {
        return std::nullopt;
    }

    auto enum_type = base_type->as<EnumType_ptr>();
    int value = enum_type->get_value(path);

    Doctor::get().assert(value != -1, WaspStage::Semantics, "Enum member not found.");

    ma.is_enum_value = true;
    ma.enum_member_value = value;
    ma.enum_type_id = enum_type->type_id;

    return make_object(EnumMemberType(enum_type, value));
}

Object_ptr SemanticAnalyzer::resolve_member_access(
    MemberAccess& expr,
    Object_ptr target_type,
    const std::string& member_name
)
{
    return std::visit(
        overloaded{
            [&](ModuleType_ptr type) -> Object_ptr
            {
                expr.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },

            [&](ClassType_ptr type) -> Object_ptr
            {
                expr.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },

            [&](TraitType_ptr type) -> Object_ptr
            {
                expr.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },

            [&](TemplateParameterType_ptr type) -> Object_ptr
            {
                Doctor::get().fatal_if_nullptr(
                    type->constraint_type,
                    WaspStage::Semantics
                );

                return resolve_member_access(
                    expr,
                    type->constraint_type,
                    member_name
                );
            },

            [&](VariantType& variant_type) -> Object_ptr
            {
                ObjectVector resulting_member_types;

                for (const auto& variant_obj : variant_type.types)
                {
                    std::visit(
                        overloaded{
                            [&](ClassType_ptr oop_variant)
                            {
                                Doctor::get().assert(
                                    oop_variant->contains_member(member_name),
                                    WaspStage::Semantics,
                                    "Member '" + member_name +
                                        "' not found in class variant."
                                );

                                resulting_member_types.push_back(
                                    oop_variant->get_member(member_name)
                                );
                            },
                            [&](TraitType_ptr oop_variant)
                            {
                                Doctor::get().assert(
                                    oop_variant->contains_member(member_name),
                                    WaspStage::Semantics,
                                    "Member '" + member_name +
                                        "' not found in trait variant."
                                );

                                resulting_member_types.push_back(
                                    oop_variant->get_member(member_name)
                                );
                            },
                            [&](auto&)
                            {
                                Doctor::get().fatal(
                                    WaspStage::Semantics,
                                    "Variant contains a non-OOP type, cannot access "
                                    "member '" +
                                        member_name + "'."
                                );
                            }
                        },
                        variant_obj->value
                    );
                }

                ObjectVector unique_types = type_system->remove_duplicates(
                    current_scope,
                    resulting_member_types
                );

                if (unique_types.size() == 1)
                {
                    return unique_types.front();
                }

                return make_object(VariantType{unique_types});
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
    // 1. Resolve angular arguments (e.g., <int>)
    ObjectVector angular_arguments;
    for (const auto& arg_node : node.angular_nodes)
    {
        angular_arguments.push_back(visit(arg_node));
    }

    // 2. Resolve the target symbol (e.g., 'greet' or 'Box')
    Symbol_ptr target_symbol = resolve_target_symbol(node.target);
    node.symbol = target_symbol;

    // Case A: Template Classes
    if (target_symbol->payload_is<OopsData>())
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
                                       mangle_object(angular_arguments);

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
    if (target_symbol->payload_is<OverloadsData>())
    {
        auto candidates = target_symbol->get_payload_as<OverloadsData>()
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

        // Store the specific overload index so the Backend knows which
        // specialized version of the function to call.
        node.overload_index = result.original_indices[0];

        return result.signatures[0];
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Target '" + target_symbol->name + "' does not support template angulars."
    );
}

} // namespace Wasp
