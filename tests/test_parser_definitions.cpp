#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseDefinitions, IntDefinition) {
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

TEST(ParseDefinitions, ListDefinition) {
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

TEST(ParseDefinitions, SetDefinition) {
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

TEST(ParseDefinitions, MapDefinition) {
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

TEST(ParseDefinitions, FunTypeDefinition) {
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

TEST(ParseDefinitions, VariantDefinition) {
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

TEST(ParseDefinitions, AliasDefinition) {
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


TEST(ParseDefinitions, EnumSimpleDefinition) {
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


TEST(ParseDefinitions, EnumNestedDefinition) {
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

TEST(ParseDefinitions, FunctionDefinitionSimple) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    x = a + b
    return x
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* funcDef = std::get_if<Wasp::FunctionDefinition>(&mod.statements[0]->data);
    ASSERT_NE(funcDef, nullptr);

    EXPECT_EQ(funcDef->name, "add");

    ASSERT_EQ(funcDef->parameters.size(), 2);
    EXPECT_EQ(funcDef->parameters[0].first, "a");
    ASSERT_NE(funcDef->parameters[0].second, nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcDef->parameters[0].second));

    EXPECT_EQ(funcDef->parameters[1].first, "b");
    ASSERT_NE(funcDef->parameters[1].second, nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcDef->parameters[1].second));

    ASSERT_NE(funcDef->return_type, nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcDef->return_type));

    ASSERT_EQ(funcDef->body.size(), 2);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&funcDef->body[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);
    auto* assignLeft = std::get_if<Wasp::Identifier>(&assign->lhs_expression->data);
    ASSERT_NE(assignLeft, nullptr);
    EXPECT_EQ(assignLeft->name, "x");

    auto* retStmt = std::get_if<Wasp::Return>(&funcDef->body[1]->data);
    ASSERT_NE(retStmt, nullptr);
    ASSERT_TRUE(retStmt->expression.has_value());
    auto* retExpr = std::get_if<Wasp::Identifier>(&retStmt->expression.value()->data);
    ASSERT_NE(retExpr, nullptr);
    EXPECT_EQ(retExpr->name, "x");
}

TEST(ParseDefinitions, FunctionDefinitionWithIf) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    if a > b then
        x = a + b
    return x
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* funcDef = std::get_if<Wasp::FunctionDefinition>(&mod.statements[0]->data);
    ASSERT_NE(funcDef, nullptr);
    EXPECT_EQ(funcDef->name, "add");

    ASSERT_EQ(funcDef->body.size(), 2);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&funcDef->body[0]->data);
    ASSERT_NE(ifBranch, nullptr);
    
    ASSERT_NE(ifBranch->test, nullptr);
    auto* testInfix = std::get_if<Wasp::Infix>(&ifBranch->test->data);
    ASSERT_NE(testInfix, nullptr);
    auto* testLeft = std::get_if<Wasp::Identifier>(&testInfix->left->data);
    ASSERT_NE(testLeft, nullptr);
    EXPECT_EQ(testLeft->name, "a");

    ASSERT_EQ(ifBranch->body.size(), 1);

    auto* retStmt = std::get_if<Wasp::Return>(&funcDef->body[1]->data);
    ASSERT_NE(retStmt, nullptr);
}
 
TEST(ParseDefinitions, FunctionDefinitionWithIfElifElse) {
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

    ASSERT_EQ(mod.statements.size(), 1);

    auto* funcDef = std::get_if<Wasp::FunctionDefinition>(&mod.statements[0]->data);
    ASSERT_NE(funcDef, nullptr);
    
    ASSERT_EQ(funcDef->body.size(), 2);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&funcDef->body[0]->data);
    ASSERT_NE(ifBranch, nullptr);

    ASSERT_TRUE(ifBranch->alternative.has_value());
    auto elifStmt = ifBranch->alternative.value();
    auto* elifBranch = std::get_if<Wasp::IfBranch>(&elifStmt->data);
    ASSERT_NE(elifBranch, nullptr);

    ASSERT_TRUE(elifBranch->alternative.has_value());
    auto elseStmt = elifBranch->alternative.value();
    auto* elseBranch = std::get_if<Wasp::ElseBranch>(&elseStmt->data);
    ASSERT_NE(elseBranch, nullptr);

    auto* retStmt = std::get_if<Wasp::Return>(&funcDef->body[1]->data);
    ASSERT_NE(retStmt, nullptr);
}
 
TEST(ParseDefinitions, FunctionDefinitionWithWhile) {
    auto mod = parse(R"(
fun add(a: int, b: int) => int
    while a < b do
        a = a + 1

    return x
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* funcDef = std::get_if<Wasp::FunctionDefinition>(&mod.statements[0]->data);
    ASSERT_NE(funcDef, nullptr);

    ASSERT_EQ(funcDef->body.size(), 2);

    auto* loop = std::get_if<Wasp::SimpleLoop>(&funcDef->body[0]->data);
    ASSERT_NE(loop, nullptr);

    ASSERT_NE(loop->condition, nullptr);
    auto* condInfix = std::get_if<Wasp::Infix>(&loop->condition->data);
    ASSERT_NE(condInfix, nullptr);

    ASSERT_EQ(loop->body.size(), 1);

    auto* retStmt = std::get_if<Wasp::Return>(&funcDef->body[1]->data);
    ASSERT_NE(retStmt, nullptr);
}

// CLASS 


TEST(ParseDefinitions, ClassDefinitionSimple) {
    auto mod = parse(R"(
class Person
    name: str
    age: int
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);

    EXPECT_EQ(classDef->name, "Person");
    ASSERT_EQ(classDef->members.size(), 2);

    ASSERT_TRUE(classDef->members.count("name"));
    ASSERT_NE(classDef->members.at("name"), nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::StringTypeNode>(*classDef->members.at("name")));

    ASSERT_TRUE(classDef->members.count("age"));
    ASSERT_NE(classDef->members.at("age"), nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*classDef->members.at("age")));
}

TEST(ParseDefinitions, ClassDefinitionWithPrivateVariable) {
    auto mod = parse(R"(
class Person
    name: str
    _age: int
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);

    EXPECT_EQ(classDef->name, "Person");
    ASSERT_EQ(classDef->members.size(), 2);

    ASSERT_TRUE(classDef->members.count("_age"));
    ASSERT_NE(classDef->members.at("_age"), nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*classDef->members.at("_age")));
}

TEST(ParseDefinitions, ClassDefinitionWithSimpleRecord) {
    auto mod = parse(R"(
class Person
    name: str
    address record
        street: str
        city: str
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);
    ASSERT_EQ(classDef->members.size(), 2);

    ASSERT_TRUE(classDef->members.count("address"));
    auto addressTypePtr = classDef->members.at("address");
    ASSERT_NE(addressTypePtr, nullptr);
    
    auto* recordTypeNodePtr = std::get_if<std::shared_ptr<Wasp::RecordTypeNode>>(addressTypePtr.get());
    ASSERT_NE(recordTypeNodePtr, nullptr) << "Expected 'address' to be a RecordTypeNode";
    
    auto recordType = *recordTypeNodePtr;
    ASSERT_EQ(recordType->members.size(), 2);
    
    ASSERT_TRUE(recordType->members.count("street"));
    EXPECT_TRUE(std::holds_alternative<Wasp::StringTypeNode>(*recordType->members.at("street")));
    
    ASSERT_TRUE(recordType->members.count("city"));
    EXPECT_TRUE(std::holds_alternative<Wasp::StringTypeNode>(*recordType->members.at("city")));
}

TEST(ParseDefinitions, ClassDefinitionWithNestedRecord) {
    auto mod = parse(R"(
class Person
    name: str
    address record
        street: str
        city: str

    job record
        title: str
        salary: int

        experience record
            years: int
            field: str
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);

    ASSERT_TRUE(classDef->members.count("job"));
    auto* jobRecordPtr = std::get_if<std::shared_ptr<Wasp::RecordTypeNode>>(classDef->members.at("job").get());
    ASSERT_NE(jobRecordPtr, nullptr);
    
    auto jobRecord = *jobRecordPtr;
    // title, salary, experience
    ASSERT_EQ(jobRecord->members.size(), 3); 

    ASSERT_TRUE(jobRecord->members.count("experience"));
    auto* expRecordPtr = std::get_if<std::shared_ptr<Wasp::RecordTypeNode>>(jobRecord->members.at("experience").get());
    ASSERT_NE(expRecordPtr, nullptr);
    
    auto expRecord = *expRecordPtr;
    ASSERT_EQ(expRecord->members.size(), 2);
    
    ASSERT_TRUE(expRecord->members.count("years"));
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*expRecord->members.at("years")));
    
    ASSERT_TRUE(expRecord->members.count("field"));
    EXPECT_TRUE(std::holds_alternative<Wasp::StringTypeNode>(*expRecord->members.at("field")));
}


TEST(ParseDefinitions, ClassDefinitionWithOneTrait) {
    auto mod = parse(R"(
class Person is Fortifiable
    name: str
    _age: int
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);
    EXPECT_EQ(classDef->name, "Person");
    
    ASSERT_EQ(classDef->traits.size(), 1);
    EXPECT_EQ(classDef->traits[0], "Fortifiable");
}

TEST(ParseDefinitions, ClassDefinitionWithManyTraits) {
    auto mod = parse(R"(
class Person is Fortifiable, Movable, Serializable
    name: str
    _age: int
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* classDef = std::get_if<Wasp::ClassDefinition>(&mod.statements[0]->data);
    ASSERT_NE(classDef, nullptr);
    EXPECT_EQ(classDef->name, "Person");

    ASSERT_EQ(classDef->traits.size(), 3);
}

TEST(ParseDefinitions, ClassImplSingleFunction) {
    auto mod = parse(R"(
impl Person is Fortifiable
    fun fortify()
        if me.age > 30 then
            x = x + 5
        else
            x = x - 5
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* implDef = std::get_if<Wasp::ImplDefinition>(&mod.statements[0]->data);
    ASSERT_NE(implDef, nullptr);

    EXPECT_EQ(implDef->class_name, "Person");
    
    ASSERT_TRUE(implDef->trait_name.has_value());
    EXPECT_EQ(implDef->trait_name.value(), "Fortifiable");

    ASSERT_EQ(implDef->methods.size(), 1);
    
    auto* funcDef = std::get_if<Wasp::FunctionDefinition>(&implDef->methods[0]->data);
    ASSERT_NE(funcDef, nullptr);
    EXPECT_EQ(funcDef->name, "fortify");
}

TEST(ParseDefinitions, ClassImplMultipleFunctions) {
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

    ASSERT_EQ(mod.statements.size(), 1);

    auto* implDef = std::get_if<Wasp::ImplDefinition>(&mod.statements[0]->data);
    ASSERT_NE(implDef, nullptr);

    EXPECT_EQ(implDef->class_name, "Person");
    ASSERT_EQ(implDef->methods.size(), 2);

    auto* funcDef1 = std::get_if<Wasp::FunctionDefinition>(&implDef->methods[0]->data);
    ASSERT_NE(funcDef1, nullptr);
    EXPECT_EQ(funcDef1->name, "fortify");
    ASSERT_EQ(funcDef1->parameters.size(), 0);

    auto* funcDef2 = std::get_if<Wasp::FunctionDefinition>(&implDef->methods[1]->data);
    ASSERT_NE(funcDef2, nullptr);
    EXPECT_EQ(funcDef2->name, "weaken");
    
    ASSERT_EQ(funcDef2->parameters.size(), 1);
    EXPECT_EQ(funcDef2->parameters[0].first, "damage");
    ASSERT_NE(funcDef2->parameters[0].second, nullptr);
    EXPECT_TRUE(std::holds_alternative<Wasp::IntTypeNode>(*funcDef2->parameters[0].second));
}
