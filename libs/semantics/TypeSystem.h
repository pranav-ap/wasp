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
    // ============================================================================
    // Equality Checks
    // ============================================================================

    static bool equal(SymbolScope_ptr scope, const Type_ptr type_1, const Type_ptr type_2);

    static bool equal(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    );

    static bool equal_unordered(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    );

    // ============================================================================
    // Assignability
    // ============================================================================

    static bool assignable(SymbolScope_ptr scope, const Type_ptr lhs_type, const Type_ptr rhs_type);

    static bool assignable(
        SymbolScope_ptr scope,
        const TypeVector& type_vector_1,
        const TypeVector& type_vector_2
    );

    // =========================================================================
    // Type Checks
    // =========================================================================

    static bool is_boolean_type(const Type_ptr type);
    static bool is_number_type(const Type_ptr type);
    static bool is_int_type(const Type_ptr type);
    static bool is_float_type(const Type_ptr type);
    static bool is_string_type(const Type_ptr type);
    static bool is_none_type(const Type_ptr type);
    static bool is_primitive_type(const Type_ptr type);

    static bool is_key_type(const Type_ptr type);

    // =========================================================================
    // Calculate
    // =========================================================================

    static Type_ptr unify(SymbolScope_ptr scope, const TypeVector& types);

    static TypeVector remove_duplicates(SymbolScope_ptr scope, const TypeVector& types);

    // =========================================================================
    // Function Call Resolution
    // =========================================================================

    static bool signatures_match(SymbolScope_ptr scope, const FunctionType_ptr a, const FunctionType_ptr b);

    static bool signatures_match(SymbolScope_ptr scope, const MethodType_ptr a, const MethodType_ptr b);

    // =========================================================================
    // Operator Resolution
    // =========================================================================

    static Type_ptr infer(
        SymbolScope_ptr scope,
        const Type_ptr left_type,
        const TokenType op,
        const Type_ptr right_type
    );

    static Type_ptr infer(SymbolScope_ptr scope, const TokenType op, const Type_ptr operand_type);

    // =======================================================================
    // Traits
    // =======================================================================

    static bool implements_trait(Type_ptr patient, const std::string& trait_name);
    static bool implements_trait(OopsType_ptr patient, const std::string& trait_name);

    // =======================================================================
    // Utils
    // =======================================================================

    static Type_ptr unpack_primitive(Type_ptr type);
    static std::string mangle(const Type_ptr& type);
    static std::string mangle(const TypeVector& generic_types);
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
