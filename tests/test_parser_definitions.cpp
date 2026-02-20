#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParserTestSuite, IntDefinition) {
    auto mod = parse("let x: int = 5");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);
    
    EXPECT_TRUE(varDef->is_mutable); 

    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();

    ASSERT_TRUE(assignment.lhs_expression->is<Wasp::Identifier>());
    EXPECT_EQ(assignment.lhs_expression->as<Wasp::Identifier>().name, "x");

    auto* typeNode = std::get_if<Wasp::IntTypeNode>(assignment.type_node.get());
    ASSERT_NE(typeNode, nullptr) << "Expected IntTypeNode annotation";

    ASSERT_TRUE(assignment.rhs_expression->is<int>());
    EXPECT_EQ(assignment.rhs_expression->as<int>(), 5);
}

TEST(ParserTestSuite, ListDefinition) {
    auto mod = parse("let x : [int] = [1, 2, 3]");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);

    EXPECT_TRUE(varDef->is_mutable); 

    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();
    
    ASSERT_TRUE(assignment.lhs_expression->is<Wasp::Identifier>());
    EXPECT_EQ(assignment.lhs_expression->as<Wasp::Identifier>().name, "x");

    auto* listTypePtr = std::get_if<std::shared_ptr<Wasp::ListTypeNode>>(assignment.type_node.get());
    ASSERT_NE(listTypePtr, nullptr) << "Expected ListTypeNode annotation";
    
    auto listTypeNode = *listTypePtr;
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*listTypeNode->element_type));
    
    ASSERT_TRUE(assignment.rhs_expression->is<Wasp::ListLiteral>());
    const auto& list = assignment.rhs_expression->as<Wasp::ListLiteral>();
    
    ASSERT_EQ(list.expressions.size(), 3);
    ASSERT_TRUE(list.expressions[0]->is<int>());
    EXPECT_EQ(list.expressions[0]->as<int>(), 1);
}

TEST(ParserTestSuite, SetDefinition) {
    auto mod = parse("let x : { int } = {1, 2, 3}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);

    EXPECT_TRUE(varDef->is_mutable);
    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();
    ASSERT_TRUE(assignment.lhs_expression->is<Wasp::Identifier>());
    EXPECT_EQ(assignment.lhs_expression->as<Wasp::Identifier>().name, "x");
    
    auto* setTypePtr = std::get_if<std::shared_ptr<Wasp::SetTypeNode>>(assignment.type_node.get());
    ASSERT_NE(setTypePtr, nullptr) << "Expected SetTypeNode annotation";
    
    auto setTypeNode = *setTypePtr;
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*setTypeNode->element_type));
    ASSERT_TRUE(assignment.rhs_expression->is<Wasp::SetLiteral>());
    
    const auto& set = assignment.rhs_expression->as<Wasp::SetLiteral>();
    ASSERT_EQ(set.expressions.size(), 3);
    ASSERT_TRUE(set.expressions[0]->is<int>());
    EXPECT_EQ(set.expressions[0]->as<int>(), 1);
}

TEST(ParserTestSuite, MapDefinition) {
    auto mod = parse("let x : { int => int } = {1 => 1, 2 => 2, 3 => 3}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);

    EXPECT_TRUE(varDef->is_mutable);

    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();
    ASSERT_TRUE(assignment.lhs_expression->is<Wasp::Identifier>());
    EXPECT_EQ(assignment.lhs_expression->as<Wasp::Identifier>().name, "x");

    auto* mapTypePtr = std::get_if<std::shared_ptr<Wasp::MapTypeNode>>(assignment.type_node.get());
    ASSERT_NE(mapTypePtr, nullptr) << "Expected MapTypeNode annotation";
    auto mapTypeNode = *mapTypePtr;
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*mapTypeNode->key_type));

    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*mapTypeNode->value_type));
    ASSERT_TRUE(assignment.rhs_expression->is<Wasp::MapLiteral>());

    const auto& map = assignment.rhs_expression->as<Wasp::MapLiteral>();
    ASSERT_EQ(map.pairs.size(), 3);
}

TEST(ParserTestSuite, FunTypeDefinition) {
    auto mod = parse("let x : (int) => int = function_name");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);

    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();

    // Verify Type Annotation is a FunctionTypeNode
    auto* funcTypeStrPtr = std::get_if<std::shared_ptr<Wasp::FunctionTypeNode>>(assignment.type_node.get());
    ASSERT_NE(funcTypeStrPtr, nullptr);
    auto funcType = *funcTypeStrPtr;

    ASSERT_EQ(funcType->input_types.size(), 1);
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcType->input_types[0]));
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcType->return_type));
}

TEST(ParserTestSuite, VariantDefinition) {
    auto mod = parse("let x : int | float = 5");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* varDef = std::get_if<Wasp::VariableDefinition>(&mod.statements[0]->data);
    ASSERT_NE(varDef, nullptr);

    ASSERT_TRUE(varDef->expression->is<Wasp::TypedAssignment>());
    const auto& assignment = varDef->expression->as<Wasp::TypedAssignment>();
    auto* variantTypeStrPtr = std::get_if<std::shared_ptr<Wasp::VariantTypeNode>>(assignment.type_node.get());
    ASSERT_NE(variantTypeStrPtr, nullptr);

    auto variantType = *variantTypeStrPtr;
    ASSERT_EQ(variantType->types.size(), 2);
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*variantType->types[0]));
    ASSERT_TRUE(std::holds_alternative<Wasp::FloatTypeNode>(*variantType->types[1]));

    ASSERT_TRUE(assignment.rhs_expression->is<int>());
    EXPECT_EQ(assignment.rhs_expression->as<int>(), 5);
}

TEST(ParserTestSuite, AliasDefinition) {
    auto mod = parse("type int_list = [int]");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* aliasDef = std::get_if<Wasp::AliasDefinition>(&mod.statements[0]->data);
    ASSERT_NE(aliasDef, nullptr);

    EXPECT_EQ(aliasDef->name, "int_list");

    auto* listTypePtr = std::get_if<std::shared_ptr<Wasp::ListTypeNode>>(aliasDef->ref_type.get());
    ASSERT_NE(listTypePtr, nullptr) << "Expected ListTypeNode annotation";

    auto listTypeNode = *listTypePtr;
    ASSERT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*listTypeNode->element_type));
}


TEST(ParserTestSuite, EnumSimpleDefinition) {
    auto mod = parse(R"(
enum Animal
	Dog
    Cat
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* enumDef = std::get_if<Wasp::EnumDefinition>(&mod.statements[0]->data);
    ASSERT_NE(enumDef, nullptr);

    EXPECT_EQ(enumDef->name, "Animal");
    EXPECT_EQ(enumDef->members.size(), 2);
}


TEST(ParserTestSuite, EnumNestedDefinition) {
    auto mod = parse(R"(
enum Animal
	Dog

	enum Bird
		Crow
		Pigeon
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* enumDef = std::get_if<Wasp::EnumDefinition>(&mod.statements[0]->data);
    ASSERT_NE(enumDef, nullptr);

    EXPECT_EQ(enumDef->name, "Animal");
    EXPECT_EQ(enumDef->members.size(), 3);
}

TEST(ParserTestSuite, FunctionDefinitionSimple) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    x = a + b
    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}

TEST(ParserTestSuite, FunctionDefinitionWithIf) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    if a > b then
        x = a + b
    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}
 
TEST(ParserTestSuite, FunctionDefinitionWithIfElifElse) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    if a > b then
        x = a + b
    elif a < b then
        x = a - b
    else
        x = a * b
    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}
 
TEST(ParserTestSuite, FunctionDefinitionWithWhile) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    while a < b do
        a = a + 1

    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}



TEST(ParserTestSuite, ClassDefinitionSimple) {
    auto mod = parse(R"(
class Person
    name: string
    age: int
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ClassDefinitionWithPrivateVariable) {
    auto mod = parse(R"(
class Person
    name: string
    _age: int
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, ClassDefinitionWithSimpleRecord) {
    auto mod = parse(R"(
class Person
    name: string
    address record
        street: string
        city: string
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, ClassDefinitionWithNestedRecord) {
    auto mod = parse(R"(
class Person
    name: string
    address record
        street: string
        city: string

    job record
        title: string
        salary: int

        experience record
            years: int
            field: string

)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ClassDefinitionWithOneTrait) {
    auto mod = parse(R"(
class Person is Fortifiable
    name: string
    _age: int
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ClassDefinitionWithManyTraits) {
    auto mod = parse(R"(
class Person is Fortifiable, Movable, Serializable
    name: string
    _age: int
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, ClassImplSingleFunction) {
    auto mod = parse(R"(
impl Person is Fortifiable
    fun fortify()
        if me.age > 30 then
            x = x + 5
        else
            x = x - 5
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ClassImplMultipleFunctions) {
    auto mod = parse(R"(
impl Person is Fortifiable
    fun fortify()
        if me.age > 30 then
            me.defense = me.defense + 15
        else
            me.defense = me.defense + 5
    
    fun weaken(damage: int)
        me.defense = me.defense - damage
        
        if me.defense < 0 then
            me.defense = 0
)");

    EXPECT_TRUE(true);
}



