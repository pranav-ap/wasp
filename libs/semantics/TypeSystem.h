#pragma once

#include "SymbolScope.h"
#include "Token.h"
#include "Type.h"

#include <memory>
#include <string>

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

    bool signatures_match(
        SymbolScope_ptr scope,
        const FunctionType_ptr a,
        const FunctionType_ptr b
    ) const;

    bool signatures_match(
        SymbolScope_ptr scope,
        const MethodType_ptr a,
        const MethodType_ptr b
    ) const;

    // =========================================================================
    // Operator Resolution
    // =========================================================================

    Type_ptr infer(
        SymbolScope_ptr scope,
        const Type_ptr left_type,
        const TokenType op,
        const Type_ptr right_type
    ) const;

    Type_ptr infer(
        SymbolScope_ptr scope,
        const TokenType op,
        const Type_ptr operand_type
    ) const;

    // =======================================================================
    // Traits
    // =======================================================================

    bool implements_trait(Type_ptr patient, const std::string& trait_name) const;
    bool implements_trait(OopsType_ptr patient, const std::string& trait_name) const;

    // =======================================================================
    // Utils
    // =======================================================================

    Type_ptr unpack_primitive(Type_ptr type) const;

    std::string mangle_name(const Type_ptr& type) const;
    std::string mangle_name(const TypeVector& generic_types) const;
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
