#pragma once

#include "SymbolScope.h"
#include "Type.h"

#include <memory>

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
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
