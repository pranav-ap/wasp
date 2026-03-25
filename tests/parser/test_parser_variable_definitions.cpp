#include "Expression.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <memory>

TEST(ParseDefinitions, IntDefinition) {
    auto block = parse("let x: int = 5");
    ASSERT_EQ(block.size(), 1);

    auto& var_def = check<Wasp::VariableDefinition>(block[0]);
    EXPECT_TRUE(var_def.is_mutable); 

    auto& assignment = check<Wasp::TypedAssignment>(var_def.expression);

    auto& lhs = check<Wasp::Identifier>(assignment.lhs_expression);
    EXPECT_EQ(lhs.name, "x");

    check<Wasp::IntTypeNode>(assignment.type_node);

    auto& rhs = check<int>(assignment.rhs_expression);
    EXPECT_EQ(rhs, 5);
}

TEST(ParseDefinitions, ListDefinition) {
    auto block = parse("let x : [int] = [1, 2, 3]");
    ASSERT_EQ(block.size(), 1);

    auto& var_def = check<Wasp::VariableDefinition>(block[0]);
    EXPECT_TRUE(var_def.is_mutable); 

    auto& assignment = check<Wasp::TypedAssignment>(var_def.expression);
    
    auto& lhs = check<Wasp::Identifier>(assignment.lhs_expression);
    EXPECT_EQ(lhs.name, "x");

    auto& list_type_ptr = check<std::shared_ptr<Wasp::ListTypeNode>>(assignment.type_node);
    check<Wasp::IntTypeNode>(list_type_ptr->element_type);
    
    auto& list = check<Wasp::ListLiteral>(assignment.rhs_expression);
    ASSERT_EQ(list.expressions.size(), 3);
}

TEST(ParseDefinitions, MapDefinition) {
    auto block = parse("let x : { int => int } = {1 => 1, 2 => 2, 3 => 3}");
    ASSERT_EQ(block.size(), 1);

    auto& var_def = check<Wasp::VariableDefinition>(block[0]);
    EXPECT_TRUE(var_def.is_mutable);

    auto& assignment = check<Wasp::TypedAssignment>(var_def.expression);
    
    auto& lhs = check<Wasp::Identifier>(assignment.lhs_expression);
    EXPECT_EQ(lhs.name, "x");

    auto& map_type_ptr = check<std::shared_ptr<Wasp::MapTypeNode>>(assignment.type_node);
    check<Wasp::IntTypeNode>(map_type_ptr->key_type);
    check<Wasp::IntTypeNode>(map_type_ptr->value_type);

    auto& map = check<Wasp::MapLiteral>(assignment.rhs_expression);
    ASSERT_EQ(map.pairs.size(), 3);
}

TEST(ParseDefinitions, FunTypeDefinition) {
    auto block = parse("let x : (int) => int = function_name");
    ASSERT_EQ(block.size(), 1);

    auto& var_def = check<Wasp::VariableDefinition>(block[0]);
    auto& assignment = check<Wasp::TypedAssignment>(var_def.expression);

    // Check (int) => int 
    auto& func_type_ptr = check<std::shared_ptr<Wasp::FunctionTypeNode>>(assignment.type_node);

    ASSERT_EQ(func_type_ptr->input_types.size(), 1);
    check<Wasp::IntTypeNode>(func_type_ptr->input_types[0]);
    
    check<Wasp::IntTypeNode>(func_type_ptr->return_type);

    // Check RHS assignment
    auto& rhs = check<Wasp::Identifier>(assignment.rhs_expression);
    EXPECT_EQ(rhs.name, "function_name");
}

TEST(ParseDefinitions, VariantDefinition) {
    auto block = parse("let x : int | float = 5");
    ASSERT_EQ(block.size(), 1);

    auto& var_def = check<Wasp::VariableDefinition>(block[0]);
    auto& assignment = check<Wasp::TypedAssignment>(var_def.expression);
    
    // Check int | float 
    auto& variant_type_ptr = check<std::shared_ptr<Wasp::VariantTypeNode>>(assignment.type_node);
    ASSERT_EQ(variant_type_ptr->types.size(), 2);
    check<Wasp::IntTypeNode>(variant_type_ptr->types[0]);
    check<Wasp::FloatTypeNode>(variant_type_ptr->types[1]);

    // Check RHS assignment
    auto& rhs = check<int>(assignment.rhs_expression);
    EXPECT_EQ(rhs, 5);
}
