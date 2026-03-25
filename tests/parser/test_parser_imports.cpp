#include "Statement.h"
#include "Token.h"

#include "test_utils.h"
#include <gtest/gtest.h>

// -----------------------------------------------------------------------------
// Simple Imports
// -----------------------------------------------------------------------------

TEST(ParseImports, SimpleImportLibrary) {
    auto block = parse("import math3d");

    auto& stmt = check<Wasp::SimpleImport>(block[0]);

    EXPECT_FALSE(stmt.access_token_type.has_value());
    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");
    EXPECT_FALSE(stmt.alias.has_value());
}

TEST(ParseImports, SimpleMyImportLibrary) {
    auto block = parse("import my.math3d");

    auto& stmt = check<Wasp::SimpleImport>(block[0]);

    EXPECT_TRUE(stmt.access_token_type.has_value());
    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");
    EXPECT_FALSE(stmt.alias.has_value());
}

TEST(ParseImports, SimpleImportWithAliasAndDepth) {
    auto block = parse("import up(2).navigation as nav");

    auto& stmt = check<Wasp::SimpleImport>(block[0]);

    ASSERT_TRUE(stmt.access_token_type.has_value());
    EXPECT_EQ(stmt.access_token_type.value(), Wasp::TokenType::UP);

    ASSERT_EQ(stmt.path.size(), 2);
    // Depth is stored as the first element in the path vector
    EXPECT_EQ(stmt.path[0], "2");
    EXPECT_EQ(stmt.path[1], "navigation");
}

// -----------------------------------------------------------------------------
// From Imports
// -----------------------------------------------------------------------------

TEST(ParseImports, FromImportSingleSymbol) {
    auto block = parse("from my.fuel import Tank");

    auto& stmt = check<Wasp::FromImport>(block[0]);

    ASSERT_TRUE(stmt.access_token_type.has_value());
    EXPECT_EQ(stmt.access_token_type.value(), Wasp::TokenType::MY);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "fuel");

    ASSERT_EQ(stmt.symbols.size(), 1);
    EXPECT_EQ(stmt.symbols[0].name, "Tank");
    EXPECT_FALSE(stmt.symbols[0].alias.has_value());
}

TEST(ParseImports, FromImportGroupedSymbols) {
    auto block = parse("from up(3).engine import (Tank as FuelTank, Pump)");

    auto& stmt = check<Wasp::FromImport>(block[0]);

    ASSERT_TRUE(stmt.access_token_type.has_value());
    EXPECT_EQ(stmt.access_token_type.value(), Wasp::TokenType::UP);

    ASSERT_EQ(stmt.path.size(), 2);
    EXPECT_EQ(stmt.path[0], "3");
    EXPECT_EQ(stmt.path[1], "engine");

    ASSERT_EQ(stmt.symbols.size(), 2);

    // First symbol
    EXPECT_EQ(stmt.symbols[0].name, "Tank");
    ASSERT_TRUE(stmt.symbols[0].alias.has_value());
    EXPECT_EQ(stmt.symbols[0].alias.value(), "FuelTank");

    // Second symbol
    EXPECT_EQ(stmt.symbols[1].name, "Pump");
    EXPECT_FALSE(stmt.symbols[1].alias.has_value());
}