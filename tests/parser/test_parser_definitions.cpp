#include "Expression.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseDefinitions, AliasDefinition)
{
    auto block = parse("type int_list = [int]");
    ASSERT_EQ(block.size(), 1);

    auto& alias_def = check<Wasp::TypeAliasDefinition>(block[0]);
    EXPECT_EQ(alias_def.name, "int_list");

    auto& list_type_ptr = check<Wasp::ListTypeNode>(alias_def.ref_type);

    auto& element_type = check<Wasp::TypeIdentifierNode>(
        list_type_ptr.element_type
    );

    EXPECT_EQ(element_type.name, "int");
}

TEST(ParseDefinitions, EnumNestedDefinition)
{
    auto block = parse(R"(
enum Animal
    Dog

    enum Bird
        Crow
        Pigeon
)");

    ASSERT_EQ(block.size(), 1);

    auto& enum_def = check<Wasp::EnumDefinition>(block[0]);
    EXPECT_EQ(enum_def.name, "Animal");

    // Check top-level members
    ASSERT_EQ(enum_def.members.size(), 1);
    EXPECT_EQ(enum_def.members[0], "Dog");

    // Check nested enums
    ASSERT_EQ(enum_def.nested_enums.size(), 1);

    auto& bird_def = enum_def.nested_enums[0];
    EXPECT_EQ(bird_def.name, "Bird");

    ASSERT_EQ(bird_def.members.size(), 2);
    EXPECT_EQ(bird_def.members[0], "Crow");
    EXPECT_EQ(bird_def.members[1], "Pigeon");
}

TEST(ParseDefinitions, FunctionDefinitionWithIfElifElse)
{
    auto block = parse(R"(
fun add(a: int, b: int) => int
    if a > b then
        x = a + b
    elif a < b then
        x = a - b
    else
        x = a * b
    return x
)");

    ASSERT_EQ(block.size(), 1);

    auto& func_def = check<Wasp::FunctionDefinition>(block[0]);
    ASSERT_EQ(func_def.block.statements.size(), 2);

    // IF ELSE NESTING

    {
        auto& if_branch = check<Wasp::Branch>(func_def.block.statements[0]);
        ASSERT_TRUE(if_branch.alternative);

        auto& elif_branch = check<Wasp::Branch>(if_branch.alternative);
        ASSERT_TRUE(elif_branch.alternative);

        check<Wasp::Branch>(elif_branch.alternative);
    }

    // RETURN STATEMENT

    {
        check<Wasp::Return>(func_def.block.statements[1]);
    }
}

TEST(ParseDefinitions, FunctionDefinitionWithWhile)
{
    auto block = parse(R"(
fun add(a: int, b: int) => int
    while a < b do
        a = a + 1

    return x
)");

    ASSERT_EQ(block.size(), 1);

    auto& func_def = check<Wasp::FunctionDefinition>(block[0]);
    ASSERT_EQ(func_def.block.statements.size(), 2);

    auto& loop = check<Wasp::SimpleLoop>(func_def.block.statements[0]);
    check<Wasp::Infix>(loop.condition);
    ASSERT_EQ(loop.block.statements.size(), 1);

    check<Wasp::Return>(func_def.block.statements[1]);
}

// CLASS

TEST(ParseDefinitions, ClassDefinitionWithManyTraits)
{
    auto block = parse(R"(
class Person is Fortifiable & Movable & Serializable
    name: str
    _age: int
)");

    ASSERT_EQ(block.size(), 1);

    auto& class_def = check<Wasp::TypeDefinition>(block[0]);
    EXPECT_EQ(class_def.name, "Person");
    ASSERT_EQ(class_def.traits.size(), 3);
}

TEST(ParseDefinitions, ClassMultipleFunctions)
{
    auto block = parse(R"(
class Person
    fun fortify()
        if my.age > 30 then
            my.defense = my.defense + 15
        else
            my.defense = my.defense + 5

    fun weaken(damage: int)
        my.defense = my.defense - damage

        if my.defense < 0 then
            my.defense = 0
)");

    ASSERT_EQ(block.size(), 1);

    auto& class_def = check<Wasp::TypeDefinition>(block[0]);
    EXPECT_EQ(class_def.name, "Person");
    ASSERT_EQ(class_def.methods.size(), 2);

    auto& func_def1 = class_def.methods[0];
    EXPECT_EQ(func_def1.name, "fortify");
    EXPECT_FALSE(func_def1.is_shared);
    EXPECT_TRUE(func_def1.parameters.empty());

    auto& func_def2 = class_def.methods[1];
    EXPECT_EQ(func_def2.name, "weaken");
    EXPECT_FALSE(func_def2.is_shared);

    ASSERT_EQ(func_def2.parameters.size(), 1);
    EXPECT_EQ(func_def2.parameters[0].name, "damage");

    auto& param_type = check<Wasp::TypeIdentifierNode>(
        func_def2.parameters[0].type
    );
    EXPECT_EQ(param_type.name, "int");
}
