#include "Expression.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <memory>

TEST(ParseDefinitions, AliasDefinition)
{
    auto block = parse("type int_list = [int]");
    ASSERT_EQ(block.size(), 1);

    auto& alias_def = check<Wasp::AliasDefinition>(block[0]);
    EXPECT_EQ(alias_def.name, "int_list");

    auto& list_type_ptr = check<std::shared_ptr<Wasp::ListTypeNode>>(alias_def.ref_type);
    check<Wasp::IntTypeNode>(list_type_ptr->element_type);
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
    EXPECT_EQ(enum_def.members.size(), 3);
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
    ASSERT_EQ(func_def.body.size(), 2);

    // IF ELSE NESTING

    {
        auto& if_branch = check<Wasp::IfBranch>(func_def.body[0]);
        ASSERT_TRUE(if_branch.alternative.has_value());

        auto& elif_branch = check<Wasp::IfBranch>(if_branch.alternative.value());
        ASSERT_TRUE(elif_branch.alternative.has_value());

        check<Wasp::ElseBranch>(elif_branch.alternative.value());
    }

    // RETURN STATEMENT

    {
        check<Wasp::Return>(func_def.body[1]);
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
    ASSERT_EQ(func_def.body.size(), 2);

    auto& loop = check<Wasp::SimpleLoop>(func_def.body[0]);
    check<Wasp::Infix>(loop.condition);
    ASSERT_EQ(loop.body.size(), 1);

    check<Wasp::Return>(func_def.body[1]);
}

// CLASS

TEST(ParseDefinitions, ClassDefinitionWithNestedRecord)
{
    auto block = parse(R"(
class Person
    name: str
    address: record
        street: str
        city: str

    job: record
        title: str
        salary: int

        experience: record
            years: int
            field: str
)");

    ASSERT_EQ(block.size(), 1);

    auto& class_def = check<Wasp::ClassDefinition>(block[0]);

    // 'name', 'address', 'job'
    ASSERT_EQ(class_def.members.size(), 3);

    auto& job_field = check<Wasp::FieldDefinition>(class_def.members[2]);
    EXPECT_EQ(job_field.name, "job");

    auto& job_record = check<std::shared_ptr<Wasp::RecordTypeNode>>(job_field.type);

    // title, salary, experience
    ASSERT_EQ(job_record->fields.size(), 3);

    auto& exp_field = check<Wasp::FieldDefinition>(job_record->fields[2]);
    EXPECT_EQ(exp_field.name, "experience");

    auto& exp_record = check<std::shared_ptr<Wasp::RecordTypeNode>>(exp_field.type);
    ASSERT_EQ(exp_record->fields.size(), 2);

    auto& years_field = check<Wasp::FieldDefinition>(exp_record->fields[0]);
    EXPECT_EQ(years_field.name, "years");
    check<Wasp::IntTypeNode>(years_field.type);

    auto& field_field = check<Wasp::FieldDefinition>(exp_record->fields[1]);
    EXPECT_EQ(field_field.name, "field");
    check<Wasp::StringTypeNode>(field_field.type);
}

TEST(ParseDefinitions, ClassDefinitionWithManyTraits)
{
    auto block = parse(R"(
class Person is Fortifiable & Movable & Serializable
    name: str
    _age: int
)");

    ASSERT_EQ(block.size(), 1);

    auto& class_def = check<Wasp::ClassDefinition>(block[0]);
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

    auto& class_def = check<Wasp::ClassDefinition>(block[0]);
    EXPECT_EQ(class_def.name, "Person");
    ASSERT_EQ(class_def.members.size(), 2);

    auto& func_def1 = check<Wasp::MethodDefinition>(class_def.members[0]);
    EXPECT_EQ(func_def1.name, "fortify");
    ASSERT_EQ(func_def1.parameters.size(), 0);

    auto& func_def2 = check<Wasp::MethodDefinition>(class_def.members[1]);
    EXPECT_EQ(func_def2.name, "weaken");

    ASSERT_EQ(func_def2.parameters.size(), 1);
    EXPECT_EQ(func_def2.parameters[0].first, "damage");

    check<Wasp::IntTypeNode>(func_def2.parameters[0].second);
}
