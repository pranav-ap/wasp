#pragma once

#include "AST.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"

#include <memory>
#include <tuple>

namespace Wasp
{

struct TypeSystem
{
    explicit TypeSystem() {};

    // ============================================================================
    // Equality Checks
    // ============================================================================

    bool equal(
        SymbolScope_ptr scope,
        const Type_ptr type_1,
        const Type_ptr type_2
    ) const;

    bool equal(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    ) const;

    bool equal_unordered(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    ) const;

    // ============================================================================
    // Assignability
    // ============================================================================

    bool assignable(
        SymbolScope_ptr scope,
        const Type_ptr lhs_type,
        const Type_ptr rhs_type
    ) const;

    bool assignable(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    ) const;

    // =========================================================================
    // Type Checks
    // =========================================================================

    bool is_boolean_type(const Type_ptr type) const;
    bool is_number_type(const Type_ptr type) const;
    bool is_int_type(const Type_ptr type) const;
    bool is_float_type(const Type_ptr type) const;
    bool is_string_type(const Type_ptr type) const;
    bool is_none_type(const Type_ptr type) const;
    bool is_primitive_type(const Type_ptr type) const;

    bool is_key_type(const Type_ptr type) const;

    // =========================================================================
    // Calculate
    // =========================================================================

    Type_ptr unify(SymbolScope_ptr scope, const TypeVector& types);

    TypeVector remove_duplicates(
        SymbolScope_ptr scope,
        const TypeVector& types
    ) const;

    // =========================================================================
    // Function Call Resolution
    // =========================================================================

    std::tuple<Symbol_ptr, int> get_best_function_symbol(
        SymbolScope_ptr scope,
        const Symbol_ptr symbol,
        const TypeVector& generic_types,
        const TypeVector& argument_types
    ) const;
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
