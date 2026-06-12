#include "Expression.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include "test_utils.h"

#include <gtest/gtest.h>

TEST(ParseDefinitions, IntDefinition)
{
    auto block = parse("let x: int = 5");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block.get(0));
    auto& binding = check<Wasp::Binding>(stmt.expression);

    EXPECT_TRUE(binding.is_mutable);
    ASSERT_TRUE(binding.declared_type);

    auto& lhs = check<Wasp::Identifier>(binding.lhs);
    EXPECT_EQ(lhs.name, "x");

    auto& type_node = check<Wasp::TypeIdentifierNode>(binding.declared_type);
    EXPECT_EQ(type_node.name, "int");

    auto& rhs = check<Wasp::IntegerLiteral>(binding.rhs);
    EXPECT_EQ(rhs.value, 5);
}

TEST(ParseDefinitions, ListDefinition)
{
    auto block = parse("let x : [int] = [1, 2, 3]");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block.get(0));
    auto& binding = check<Wasp::Binding>(stmt.expression);

    EXPECT_TRUE(binding.is_mutable);
    ASSERT_TRUE(binding.declared_type);

    auto& lhs = check<Wasp::Identifier>(binding.lhs);
    EXPECT_EQ(lhs.name, "x");

    auto& list_type_ptr = check<Wasp::ListTypeNode>(binding.declared_type);
    auto& element_type = check<Wasp::TypeIdentifierNode>(
        list_type_ptr.element_type
    );
    EXPECT_EQ(element_type.name, "int");

    auto& list = check<Wasp::ListLiteral>(binding.rhs);
    ASSERT_EQ(list.expressions.size(), 3);
}

TEST(ParseDefinitions, MapDefinition)
{
    auto block = parse("let x : { int => int } = {1 => 1, 2 => 2, 3 => 3}");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block.get(0));
    auto& binding = check<Wasp::Binding>(stmt.expression);

    EXPECT_TRUE(binding.is_mutable);
    ASSERT_TRUE(binding.declared_type);

    auto& lhs = check<Wasp::Identifier>(binding.lhs);
    EXPECT_EQ(lhs.name, "x");

    auto& map_type_ptr = check<Wasp::MapTypeNode>(binding.declared_type);
    auto& key_type = check<Wasp::TypeIdentifierNode>(map_type_ptr.key_type);
    EXPECT_EQ(key_type.name, "int");

    auto& value_type = check<Wasp::TypeIdentifierNode>(map_type_ptr.value_type);
    EXPECT_EQ(value_type.name, "int");

    auto& map = check<Wasp::MapLiteral>(binding.rhs);
    ASSERT_EQ(map.pairs.size(), 3);
}

TEST(ParseDefinitions, FunTypeDefinition)
{
    auto block = parse("let x : (int) => int = function_name");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block.get(0));
    auto& binding = check<Wasp::Binding>(stmt.expression);

    EXPECT_TRUE(binding.is_mutable);
    ASSERT_TRUE(binding.declared_type);

    // Check (int) => int
    auto& func_type_ptr = check<Wasp::FunctionTypeNode>(binding.declared_type);

    ASSERT_EQ(func_type_ptr.input_types.size(), 1);
    auto& input_type = check<Wasp::TypeIdentifierNode>(
        func_type_ptr.input_types[0]
    );
    EXPECT_EQ(input_type.name, "int");

    auto& return_type = check<Wasp::TypeIdentifierNode>(
        func_type_ptr.return_type
    );
    EXPECT_EQ(return_type.name, "int");

    // Check RHS assignment
    auto& rhs = check<Wasp::Identifier>(binding.rhs);
    EXPECT_EQ(rhs.name, "function_name");
}

TEST(ParseDefinitions, VariantDefinition)
{
    auto block = parse("let x : int | float = 5");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block.get(0));
    auto& binding = check<Wasp::Binding>(stmt.expression);

    EXPECT_TRUE(binding.is_mutable);
    ASSERT_TRUE(binding.declared_type);

    auto& lhs = check<Wasp::Identifier>(binding.lhs);
    EXPECT_EQ(lhs.name, "x");

    // Check int | float
    auto& variant_type_ptr = check<Wasp::VariantTypeNode>(
        binding.declared_type
    );

    ASSERT_EQ(variant_type_ptr.types.size(), 2);

    auto& type_1 = check<Wasp::TypeIdentifierNode>(variant_type_ptr.types[0]);
    EXPECT_EQ(type_1.name, "int");

    auto& type_2 = check<Wasp::TypeIdentifierNode>(variant_type_ptr.types[1]);
    EXPECT_EQ(type_2.name, "float");

    // Check RHS assignment
    auto& rhs = check<Wasp::IntegerLiteral>(binding.rhs);
    EXPECT_EQ(rhs.value, 5);
}
