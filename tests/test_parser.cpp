#include "lexer.h"
#include "parser.h"
#include <gtest/gtest.h>


Wasp::Module parse(const std::string& code) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    auto tokens = lexer.run(code);
    auto mod = parser.run(tokens);

    return mod;
}

TEST(ParserTestSuite, Number) {
    auto mod = parse("2");
    EXPECT_TRUE(true); 
}

TEST(ParserTestSuite, Addition) {
    auto mod = parse("1 + 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, List) {
    auto mod = parse("[1, 2, 3]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Tuple) {
    auto mod = parse("(1, 2, 3)");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EmptySet) {
    auto mod = parse("{}");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, Set) {
    auto mod = parse("{1, 2, 3}");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EmptyMap) {
    auto mod = parse("{->}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Map) {
    auto mod = parse("{1 -> 1, 2 -> '2', 3 -> 3.0}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, IntDefinition) {
    auto mod = parse("let x : int = 5");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ListDefinition) {
    auto mod = parse("let x : [int] = [1, 2, 3]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, SetDefinition) {
    auto mod = parse("let x : { int } = {1, 2, 3}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MapDefinition) {
    auto mod = parse("let x : { int -> int } = {1 -> 1, 2 -> 2, 3 -> 3}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunTypeDefinition) {
    auto mod = parse("let x : (int) -> int = function_name");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, VariantDefinition) {
    auto mod = parse("let x : (int | float) = 5");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, AliasDefinition) {
    auto mod = parse("alias int_list = [int]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, TernaryExpression) {
    auto mod = parse("if true then 1 else 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, TernaryLetExpression) {
    auto mod = parse("if let x = 1 then 1 else 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, IfBlock) {
    auto mod = parse(R"(
if true then
    1 
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, IfElseBlock) {
    auto mod = parse(R"(
if true then
    1
else
    2
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, IfElifElseBlock) {
    auto mod = parse(R"(
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, WhileSingle) {
    auto mod = parse(R"(while x < 10 do x = x + 1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, WhileBlock) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    y = 1
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Continue) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    continue
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ForSingle) {
    auto mod = parse(R"(for x in [1, 2, 3] do x = x + 1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ForBlock) {
    auto mod = parse(R"(
for x in [1, 2, 3] do 
    x = x + 1
    y = 1
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EnumSimpleDefinition) {
    auto mod = parse(R"(
enum Animal
	Dog
    Cat
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EnumNestedDefinition) {
    auto mod = parse(R"(
enum Animal
	Dog

	enum Bird
		Crow
		Pigeon
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MemberAccessSimple) {
    auto mod = parse(R"(Animal.Dog)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MemberAccessNested) {
    auto mod = parse(R"(Animal.Dog.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MemberAccessWithString) {
    auto mod = parse(R"(Animal.'Dog'.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ReturnStatementEmpty) {
    auto mod = parse(R"(return)");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ReturnStatementSimple) {
    auto mod = parse(R"(return 5)");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ReturnStatementExpression) {
    auto mod = parse(R"(return 5 + 23)");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunctionDefinitionSimple) {
    auto mod = parse(R"(
fun add(a: int, b: int) -> int
    x = a + b
    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}

TEST(ParserTestSuite, FunctionDefinitionWithIf) {
    auto mod = parse(R"(
fun add(a: int, b: int) -> int
    if a > b then
        x = a + b
    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}
 
TEST(ParserTestSuite, FunctionDefinitionWithIfElifElse) {
    auto mod = parse(R"(
fun add(a: int, b: int) -> int
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
fun add(a: int, b: int) -> int
    while a < b do
        a = a + 1

    return x
)");
 
    EXPECT_TRUE(mod.statements.size() == 1);
}

TEST(ParserTestSuite, FunctionCallWithoutArguments) {
    auto mod = parse("get_worker()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunctionCallWithOneArgument) {
    auto mod = parse("get_worker(1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunctionCallWithMultipleArguments) {
    auto mod = parse("get_worker(1, 'John', true)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMemberAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).name");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).get_name()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMethodAccessWithStringAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).'police'.get_name()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeSimpleExclusive) {
    auto mod = parse("1..10");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeSimpleInclusive) {
    auto mod = parse("1...10");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithStep) {
    auto mod = parse("1..10:2");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutEnd) {
    auto mod = parse("1..");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutEndWithStep) {
    auto mod = parse("1..:2");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutStartOrEndOrStep) {
    // No assigned meaning yet, but should still parse without error
    auto mod = parse("...");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, Dot) {
    auto mod = parse(".");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, DotDot) {
    auto mod = parse("..");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, PipeOutput) {
    auto mod = parse("foo() ~ bar(., 35) ~ boom(...)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Star) {
    auto mod = parse("*b");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarGather) {
    auto mod = parse("[a, *b, c] = [1, 2, 3, 4, 5]");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarSpread) {
    auto mod = parse("[a, b, c] = *three_nums");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarGatherAndSpread) {
    auto mod = parse("[a, *b, c] = *five_nums");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, AnnotationDefinitionSimple) {
    auto mod = parse("@tag('smoke', 'unit')");

    EXPECT_TRUE(true);
}


