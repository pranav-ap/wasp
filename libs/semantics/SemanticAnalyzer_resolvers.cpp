#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>

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
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);

    bind_identifier(identifier, symbol);
    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr)
{
    Object_ptr left_type = visit(expr.left);

    Doctor::get().assert(
        expr.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier."
    );

    std::string member_name = expr.right->as<Identifier>().name;

    // 1. Modules
    if (auto type = try_unwrap_ptr<ModuleType_ptr>(left_type))
    {
        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }

    // 2. Classes or Traits (Inline check for OOP types)
    BaseOOPType_ptr oop_type = nullptr;
    if (auto t = try_unwrap_ptr<ClassType_ptr>(left_type))
    {
        oop_type = t;
    }
    else if (auto t = try_unwrap_ptr<TraitType_ptr>(left_type))
    {
        oop_type = t;
    }

    if (oop_type)
    {
        expr.member_index = oop_type->get_member_index(member_name);
        return oop_type->get_member(member_name);
    }

    // 3. Enums
    if (auto type = try_unwrap_ptr<EnumType_ptr>(left_type))
    {
        std::string full_name = type->name + "." + member_name;
        if (type->nested_enums.contains(full_name))
        {
            return make_object(type->nested_enums.at(full_name));
        }

        Doctor::get().assert(
            type->members.contains(full_name),
            WaspStage::Semantics,
            "Enum '" + type->name + "' does not contain member '" + member_name +
                "'."
        );

        expr.member_index = type->members.at(full_name);
        expr.is_enum_value = true;
        return left_type;
    }

    // 4. Template Parameters (Generics)
    if (auto type = try_unwrap_ptr<TemplateParameterType_ptr>(left_type))
    {
        Object_ptr constraint = type->constraint_type;

        // Check if the constraint is a Union/Variant (e.g., Int | String)
        if (auto* variant_constraint = constraint->try_as<VariantType>())
        {
            Object_ptr resulting_member_type = nullptr;

            for (const auto& variant : variant_constraint->types)
            {
                // Manual check for OOP types within the variant loop
                BaseOOPType_ptr oop_variant = nullptr;
                if (variant->is<ClassType_ptr>())
                {
                    oop_variant = variant->as<ClassType_ptr>();
                }
                else if (variant->is<TraitType_ptr>())
                {
                    oop_variant = variant->as<TraitType_ptr>();
                }

                Doctor::get().assert(
                    oop_variant && oop_variant->contains_member(member_name),
                    WaspStage::Semantics,
                    "Member '" + member_name +
                        "' not found in variant of template parameter."
                );

                if (!resulting_member_type)
                {
                    resulting_member_type = oop_variant->get_member(member_name);
                }
            }
            return resulting_member_type;
        }

        // Single OOP constraint (e.g., T: Int)
        BaseOOPType_ptr single_oop = nullptr;
        if (auto t = try_unwrap_ptr<ClassType_ptr>(constraint))
        {
            single_oop = t;
        }
        else if (auto t = try_unwrap_ptr<TraitType_ptr>(constraint))
        {
            single_oop = t;
        }

        if (single_oop)
        {
            return single_oop->get_member(member_name);
        }
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + member_name +
            "'. LHS is not a module, class, trait, enum, or template parameter."
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
    if (target_symbol->payload_is<ClassData>())
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

        return type_system->substitute_generics(base, substitutions);
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
