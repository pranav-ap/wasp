#pragma once

#include <string>
#include <gtest/gtest.h>
#include "Parser.h" 


Wasp::Module parse(const std::string& code);

template<typename T>
const T& check(const Wasp::Expression_ptr& ptr) {
    auto* val = ptr->try_as<T>();
    
    if (val) {
        return *val;  
    }

    ADD_FAILURE() << "AST Node Type Mismatch"; 
    
    // We must return a T&. 
    // This lives forever, so the reference remains "valid" 
    // long enough for the test runner to report the failure.
    static T dummy{}; 
    return dummy;
}

// Use PtrType as a template parameter to accept Statement_ptr or Expression_ptr
template<typename T, typename PtrType>
const T& check(const PtrType& ptr) {
    if (!ptr) {
        ADD_FAILURE() << "Target pointer is null";
        static T dummy{};
        return dummy;
    }

    // Use 'template' keyword here because try_as is a dependent template member
    auto* val = ptr->template try_as<T>();
    
    if (val) {
        return *val;  
    }

    ADD_FAILURE() << "AST Node Type Mismatch"; 
    
    static T dummy{}; 
    return dummy;
}

// Overload specifically for empty sequences
template<typename T>
void check_sequence(const Wasp::Expression_ptr& expr) {
    const auto& seq = check<T>(expr);
    ASSERT_EQ(seq.expressions.size(), 0) << "Sequence size mismatch: expected empty sequence";
}

template<typename T, typename U>
void check_sequence(const Wasp::Expression_ptr& expr, const std::vector<U>& expected_values) {
    // Check the container type (ListLiteral, TupleLiteral, etc.)
    const auto& seq = check<T>(expr);
    
    // Check Length
    ASSERT_EQ(seq.expressions.size(), expected_values.size()) << "Sequence size mismatch";

    // Check each element against the expected type U
    for (size_t i = 0; i < expected_values.size(); ++i) {
        EXPECT_EQ(check<U>(seq.expressions[i]), expected_values[i]) 
            << "Value mismatch at index " << i;
    }
}

template<typename T, typename U>
void check_sequence(const Wasp::Expression_ptr& expr, std::initializer_list<U> expected) {
    check_sequence<T, U>(expr, std::vector<U>(expected));
}

template<typename T>
void check_sequence_custom(const Wasp::Expression_ptr& expr, 
                           size_t expected_size, 
                           std::function<void(const Wasp::Expression_ptr&, size_t)> element_checker) {
    const auto& seq = check<T>(expr);
    ASSERT_EQ(seq.expressions.size(), expected_size);

    for (size_t i = 0; i < seq.expressions.size(); ++i) {
        element_checker(seq.expressions[i], i);
    }
}

