#pragma once

#include "Objects.h"
#include "ObjectStore.h"
#include "SymbolScope.h"
#include "Token.h"
#include <vector>
#include <utility>
#include <memory>

namespace Wasp { 
    
struct TypeSystem
{
    ConstantPool_ptr type_pool;

    bool equal(SymbolScope_ptr scope, const Object_ptr type_1, const Object_ptr type_2) const;
    bool equal(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const;
    bool equal_unordered(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const;

    bool assignable(SymbolScope_ptr scope, const Object_ptr lhs_type, const Object_ptr rhs_type) const;
    bool assignable(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const;

    Object_ptr infer(SymbolScope_ptr scope, Object_ptr left_type, TokenType op, Object_ptr right_type);
    Object_ptr infer(SymbolScope_ptr scope, Object_ptr left_type, TokenType op);

    Object_ptr spread_type(Object_ptr type);

    // Is _ type?
    bool is_boolean_type(const Object_ptr type) const;
    bool is_number_type(const Object_ptr type) const;
    bool is_int_type(const Object_ptr type) const;
    bool is_float_type(const Object_ptr type) const;
    bool is_string_type(const Object_ptr type) const;
    bool is_none_type(const Object_ptr type) const;
    bool is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const;
    bool is_spreadable_type(SymbolScope_ptr scope, const Object_ptr condition_type) const;
    bool is_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const;
    bool is_key_type(SymbolScope_ptr scope, const Object_ptr type) const;  

    // assert type
    void expect_boolean_type(const Object_ptr type) const;
    void expect_number_type(const Object_ptr type) const;
    void expect_int_type(const Object_ptr type) const;
    void expect_float_type(const Object_ptr type) const;
    void expect_string_type(const Object_ptr type) const;
    void expect_none_type(const Object_ptr type) const;
    void expect_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const;
    void expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr condition_type) const;
    void expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const;
    void expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const;

    TypeSystem()
        : type_pool(std::make_shared<ConstantPool>()) {};
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
}