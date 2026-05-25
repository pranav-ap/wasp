#pragma once

#include "ConstantPool.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Wasp
{

struct TypeSystem
{
    ConstantPool_ptr pool;

    TypeSystem(ConstantPool_ptr pool) : pool(pool) {};

    // =========================================================================
    // Type Inference
    // =========================================================================

    Object_ptr get_least_upper_bound(
        SymbolScope_ptr scope,
        ObjectVector types
    ) const;

    Object_ptr get_least_upper_bound(
        SymbolScope_ptr scope,
        Object_ptr a,
        Object_ptr b
    ) const;

    Object_ptr unify(SymbolScope_ptr scope, const ObjectVector& types);

    bool equal(
        SymbolScope_ptr scope,
        const Object_ptr type_1,
        const Object_ptr type_2
    ) const;

    bool equal(
        SymbolScope_ptr scope,
        const ObjectVector& type_vector_1,
        const ObjectVector& type_vector_2
    ) const;

    bool equal_unordered(
        SymbolScope_ptr scope,
        const ObjectVector& type_vector_1,
        const ObjectVector& type_vector_2
    ) const;

    bool assignable(
        SymbolScope_ptr scope,
        const Object_ptr lhs_type,
        const Object_ptr rhs_type
    ) const;

    bool assignable(
        SymbolScope_ptr scope,
        const ObjectVector& type_vector_1,
        const ObjectVector& type_vector_2
    ) const;

    ObjectStringMap infer_template_arguments(
        Signature_ptr signature,
        const ObjectVector& argument_types
    );

    Object_ptr infer(
        SymbolScope_ptr scope,
        Object_ptr left_type,
        TokenType op,
        Object_ptr right_type
    );

    Object_ptr infer(SymbolScope_ptr scope, Object_ptr left_type, TokenType op);

    // =========================================================================
    // Overloads
    // =========================================================================

    SymbolVector::iterator find_matching_signature(
        SymbolScope_ptr scope,
        SymbolVector& target_vector,
        const ObjectVector& parameter_types
    );

    std::tuple<Symbol_ptr, int> get_best_function_symbol(
        SymbolScope_ptr scope,
        const SymbolVector& candidates,
        const ObjectVector& argument_types
    ) const;

    std::tuple<Object_ptr, int> get_best_function_object(
        SymbolScope_ptr scope,
        const ObjectVector& candidates,
        const ObjectVector& argument_types
    ) const;

    void validate_new_function_overload(
        SymbolScope_ptr scope,
        std::string& function_name,
        const Symbol_ptr new_func_symbol
    );

    void validate_new_method_overload(
        SymbolScope_ptr scope,
        ObjectVector existing_overloads,
        const Symbol_ptr new_method_symbol
    );

    // =========================================================================
    // Traits
    // =========================================================================

    bool implements_trait(
        SymbolScope_ptr scope,
        const Object_ptr patient,
        const std::string& trait_name
    ) const;

    bool implements_trait(
        SymbolScope_ptr scope,
        const Object_ptr patient,
        const int trait_type_id
    ) const;

    bool implements_trait(
        SymbolScope_ptr scope,
        const Object_ptr patient,
        const Object_ptr trait
    ) const;

    // =========================================================================
    // Templates
    // =========================================================================

    struct SpecializationResult
    {
        ObjectVector signatures;
        std::vector<int> original_indices;
    };

    StringVector get_generics_declaration_order(const Object_ptr& base) const;

    std::vector<std::pair<Symbol_ptr, int>> filter_by_generic_arity(
        const SymbolVector& overloads,
        size_t expected_generic_count
    ) const;

    Object_ptr substitute_generics(
        Object_ptr type,
        const ObjectStringMap& substitutions
    ) const;

    SpecializationResult specialize_candidates(
        const std::vector<std::pair<Symbol_ptr, int>>& candidates,
        const ObjectVector& generic_args
    ) const;

    // =========================================================================
    // Utilities
    // =========================================================================

    ObjectVector remove_duplicates(
        SymbolScope_ptr scope,
        const ObjectVector& vec
    ) const;

    Object_ptr resolve_type(
        Object_ptr type,
        bool resolve_generics = false
    ) const;

    Object_ptr spread_type(Object_ptr type);
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
