#include "Statement.h"
#include "Token.h"

#include "test_utils.h"
#include <gtest/gtest.h>

// -----------------------------------------------------------------------------
// Standard Imports
// -----------------------------------------------------------------------------

TEST(ParseImports, SimpleImportLibrary)
{
    auto block = parse("import math3d");

    auto& stmt = check<Wasp::Import>(block[0]);

    EXPECT_FALSE(stmt.access_modifier.has_value());
    EXPECT_EQ(stmt.access_argument, 1);
    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");

    EXPECT_FALSE(stmt.module_alias.has_value());
    EXPECT_FALSE(stmt.expose_all);
    EXPECT_TRUE(stmt.exposed_symbols.empty());
}

TEST(ParseImports, SimpleMyImportLibrary)
{
    auto block = parse("import my.math3d");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_TRUE(stmt.access_modifier.has_value());
    EXPECT_EQ(stmt.access_modifier.value(), Wasp::TokenType::MY);
    EXPECT_EQ(stmt.access_argument, 1);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");
    EXPECT_FALSE(stmt.module_alias.has_value());
}

TEST(ParseImports, SimpleImportWithAliasAndDepth)
{
    auto block = parse("import up(2).navigation as nav");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_TRUE(stmt.access_modifier.has_value());
    EXPECT_EQ(stmt.access_modifier.value(), Wasp::TokenType::UP);

    // Depth is now stored cleanly in access_argument!
    EXPECT_EQ(stmt.access_argument, 2);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "navigation");

    ASSERT_TRUE(stmt.module_alias.has_value());
    EXPECT_EQ(stmt.module_alias.value(), "nav");
}

// -----------------------------------------------------------------------------
// Expose Imports
// -----------------------------------------------------------------------------

TEST(ParseImports, ExposeSingleSymbol)
{
    auto block = parse("import my.fuel expose Tank");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_TRUE(stmt.access_modifier.has_value());
    EXPECT_EQ(stmt.access_modifier.value(), Wasp::TokenType::MY);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "fuel");

    ASSERT_EQ(stmt.exposed_symbols.size(), 1);
    EXPECT_EQ(stmt.exposed_symbols[0].name, "Tank");
    EXPECT_FALSE(stmt.exposed_symbols[0].alias.has_value());
}

TEST(ParseImports, ExposeGroupedSymbols)
{
    auto block = parse("import up(3).engine expose Tank as FuelTank, Pump");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_TRUE(stmt.access_modifier.has_value());
    EXPECT_EQ(stmt.access_modifier.value(), Wasp::TokenType::UP);
    EXPECT_EQ(stmt.access_argument, 3);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "engine");

    ASSERT_EQ(stmt.exposed_symbols.size(), 2);

    // First symbol
    EXPECT_EQ(stmt.exposed_symbols[0].name, "Tank");
    ASSERT_TRUE(stmt.exposed_symbols[0].alias.has_value());
    EXPECT_EQ(stmt.exposed_symbols[0].alias.value(), "FuelTank");

    // Second symbol
    EXPECT_EQ(stmt.exposed_symbols[1].name, "Pump");
    EXPECT_FALSE(stmt.exposed_symbols[1].alias.has_value());
}

// -----------------------------------------------------------------------------
// Wildcard Imports
// -----------------------------------------------------------------------------

TEST(ParseImports, ExposeWildcard)
{
    auto block = parse("import math3d expose *");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");

    EXPECT_TRUE(stmt.expose_all);
    EXPECT_TRUE(stmt.excluded_symbols.empty());
    EXPECT_TRUE(stmt.exposed_symbols.empty());
}

TEST(ParseImports, ExposeWildcardWithExceptions)
{
    auto block = parse("import math3d expose * except tan, cos");

    auto& stmt = check<Wasp::Import>(block[0]);

    ASSERT_EQ(stmt.path.size(), 1);
    EXPECT_EQ(stmt.path[0], "math3d");

    EXPECT_TRUE(stmt.expose_all);
    EXPECT_TRUE(stmt.exposed_symbols.empty());

    ASSERT_EQ(stmt.excluded_symbols.size(), 2);
    EXPECT_EQ(stmt.excluded_symbols[0], "tan");
    EXPECT_EQ(stmt.excluded_symbols[1], "cos");
}
