#include "Compiler.h"
#include "AST.h"
#include "CFGraph.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Token.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Compiler::Compiler(Workspace_ptr workspace)
    : workspace(std::move(workspace)), current_block_id(InvalidBlockId), parent(nullptr),
      compiler_depth(0), current_lexical_scope_depth(0)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

Compiler::Compiler(Compiler* parent)
    : parent(parent), workspace(parent->workspace), compiler_depth(parent->compiler_depth + 1),
      current_lexical_scope_depth(parent->current_lexical_scope_depth + 1),
      module_path(parent->module_path)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

FunctionBlueprintObject_ptr Compiler::run(
    const StatementVector& block,
    std::string filepath,
    bool is_main
)
{
    this->module_path = std::move(filepath);

    if (is_main)
        emit(OpCode::ENTER_WORKSPACE);
    emit(OpCode::ENTER_MODULE);

    for (const auto& s : block)
        visit(s);

    BlockId exit = graph.create_block();
    graph.add_edge(current_block_id, exit);
    emit(OpCode::JUMP, static_cast<int>(exit));
    set_current_block(exit);

    emit_exports();

    if (is_main)
        emit(OpCode::EXIT_WORKSPACE);

    return std::make_shared<FunctionBlueprintObject>(flatten(), module_path);
}

void Compiler::emit_exports()
{
    auto mod = workspace->get_module(module_path);

    if (mod == nullptr)
    {
        emit(OpCode::EXIT_MODULE, 0);
        return;
    }

    Doctor::get().fatal_if_nullptr(mod, WaspStage::Compiler);

    int export_count = 0;

    // Iterate over the exports approved by the Semantic Analyzer
    for (const auto& exported_symbol : mod->exports)
    {
        // Find where this export lives on the VM's local stack
        int stack_index = resolve_local(exported_symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Failed to find exported symbol on stack: " + exported_symbol->name
        );

        // Push it to the top of the stack for EXIT_MODULE to consume
        emit(OpCode::GET_LOCAL, stack_index, exported_symbol->name);
        export_count++;
    }

    // Exit the module with the correct, synchronized count
    emit(OpCode::EXIT_MODULE, export_count);
}

// ========================================================================
// Visitors
// ========================================================================

void Compiler::visit(std::vector<Statement_ptr>& statements)
{
    auto is_func = [](const Statement_ptr& s)
    {
        return s->is<FunctionDefinition>();
    };

    for (auto& stmt : statements)
        if (is_func(stmt))
            visit(stmt);

    for (auto& stmt : statements)
        if (!is_func(stmt))
            visit(stmt);
}

void Compiler::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](std::monostate&)
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unhandled Statement (monostate) in Compiler!"
                );
            },
            [&](auto& stat)
            {
                this->visit(stat);
            }
        },
        statement->data
    );
}

void Compiler::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
    emit(OpCode::POP);
}

// ============================================================================
// Variables & Assignments
// ============================================================================

void Compiler::visit(Assignment& expr)
{
    if (expr.lhs->is<Identifier>())
    {
        // 1. Evaluate the right-hand side (leaves value on the VM stack)
        visit(expr.rhs);

        auto& id = expr.lhs->as<Identifier>();
        auto symbol = id.symbol;
        Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

        // 2. Handle captured closure variables (Upvalues) vs pure locals
        if (id.must_be_captured)
        {
            int upval_index = resolve_upvalue(this, symbol);
            emit(OpCode::SET_UPVALUE, upval_index, symbol->name);
            emit(
                OpCode::GET_UPVALUE,
                upval_index,
                symbol->name
            ); // Leave value on stack
        }
        else
        {
            int physical_index = get_or_add_local_index(symbol);
            emit(OpCode::SET_LOCAL, physical_index, symbol->name);
            emit(
                OpCode::GET_LOCAL,
                physical_index,
                symbol->name
            ); // Leave value on stack
        }
    }
    else if (expr.lhs->is<MemberAccess>())
    {
        auto& mac = expr.lhs->as<MemberAccess>();
        compile_member_assignment(mac, expr.rhs);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Left-hand side of assignment must be an Identifier or MemberAccess"
        );
    }
}

void Compiler::compile_member_assignment(
    MemberAccess& access,
    const Expression_ptr& value
)
{
    visit(access.left);
    visit(value);

    Doctor::get().assert(
        access.right->is<Identifier>(),
        WaspStage::Compiler,
        "Right side of member assignment must be an Identifier"
    );

    auto target_name = access.right->as<Identifier>().name;
    emit(OpCode::SET_MEMBER, access.member_index, target_name);
}

// ============================================================================
// Core Statements
// ============================================================================

void Compiler::visit(Return& statement)
{
    if (statement.expression.has_value())
    {
        visit(statement.expression.value());
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::RETURN);
}

// ============================================================================
// Definitions
// ============================================================================

void Compiler::visit(FunctionDefinition& def)
{
    bool is_new_declaration = (resolve_local(def.group_symbol->id) == -1);
    int physical_index = get_or_add_local_index(def.group_symbol);

    if (is_new_declaration)
    {
        emit(OpCode::PUSH_EMPTY_OVERLOAD_GROUP);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "reserve slot for fun " + def.name
        );
    }

    if (def.symbol->is_native())
    {
        std::string mangled = mangle_name(
            def.name,
            "",
            def.symbol->module_path
        );

        int registry_id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, registry_id, mangled);
    }
    else
    {
        compile_function_closure(def.name, def.parameter_symbols, def.body);
    }

    std::string debug_prefix = def.is_pure ? "pure fun " : "fun ";
    emit(
        OpCode::STORE_FUNCTION_OVERLOAD,
        physical_index,
        debug_prefix + def.name
    );
}

void Compiler::visit(OperatorDefinition& def)
{
    bool is_new_declaration = (resolve_local(def.group_symbol->id) == -1);

    int physical_index = get_or_add_local_index(def.group_symbol);

    if (is_new_declaration)
    {
        emit(OpCode::PUSH_EMPTY_OVERLOAD_GROUP);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "reserve slot for operator " + def.name
        );
    }

    if (def.symbol->is_native())
    {
        std::string mangled = mangle_name(
            def.name,
            "",
            def.symbol->module_path
        );

        int registry_id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, registry_id, mangled);
    }
    else
    {
        compile_function_closure(def.name, def.parameter_symbols, def.body);
    }

    std::string debug_prefix = "operator " + def.name;
    emit(
        OpCode::STORE_FUNCTION_OVERLOAD,
        physical_index,
        debug_prefix + def.name
    );
}

void Compiler::visit(ClassDefinition& class_definition)
{
    int class_blueprint_physical_index = get_or_add_local_index(
        class_definition.symbol
    );

    emit(
        OpCode::PUSH_EMPTY_CLASS_BLUEPRINT,
        "push empty class " + class_definition.name
    );
    emit(
        OpCode::SET_LOCAL,
        class_blueprint_physical_index,
        "init class in slot"
    );

    int unique_method_count = 0;

    auto type_obj = class_definition.symbol->get_type();
    std::shared_ptr<ClassType> class_type = type_obj->as<ClassType_ptr>();

    StringVector all_methods = class_type->methods;
    all_methods.insert(
        all_methods.end(),
        class_type->pures.begin(),
        class_type->pures.end()
    );
    all_methods.insert(
        all_methods.end(),
        class_type->statics.begin(),
        class_type->statics.end()
    );

    auto compile_if_match =
        [&](auto& method, const std::string& target_name, int& count)
    {
        if (method.name == target_name)
        {
            if (method.symbol->is_native())
            {
                std::string mangled = mangle_name(
                    method.name,
                    class_definition.name,
                    method.symbol->module_path
                );

                int registry_id = workspace->native_registry->get_native_index(
                    mangled
                );
                emit(
                    OpCode::GET_NATIVE,
                    registry_id,
                    "load native method " + mangled
                );
            }
            else
            {
                compile_function_closure(
                    method.name,
                    method.parameter_symbols,
                    method.body
                );
            }
            count++;
        }
    };

    for (const std::string& method_name : all_methods)
    {
        int overload_count = 0;

        for (auto& stmt : class_definition.members)
        {
            std::visit(
                overloaded{
                    [&](MethodDefinition& m)
                    {
                        compile_if_match(m, method_name, overload_count);
                    },
                    [&](auto&)
                    {
                    } // Ignore fields, type aliases, etc.
                },
                stmt->data
            );
        }

        emit(
            OpCode::BUILD_OVERLOAD_GROUP,
            overload_count,
            "overloads for " + method_name
        );
        unique_method_count++;
    }

    auto fields_offset = static_cast<int>(class_type->fields.size());

    emit(
        OpCode::GET_LOCAL,
        class_blueprint_physical_index,
        "load pre-allocated class for modification"
    );
    emit(
        OpCode::BUILD_CLASS,
        unique_method_count,
        fields_offset,
        "populate class " + class_definition.name
    );
    emit(
        OpCode::SET_LOCAL,
        class_blueprint_physical_index,
        "update local slot"
    );
}

void Compiler::emit_closure_upvalues(const std::vector<Upvalue>& upvalues)
{
    emit(OpCode::MAKE_FUNCTION, static_cast<int>(upvalues.size()));

    for (const auto& uv : upvalues)
    {
        emit_raw_byte(uv.is_local_to_parent ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }
}

void Compiler::compile_function_closure(
    const std::string& name,
    const std::vector<Symbol_ptr>& parameters,
    StatementVector body
)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    for (const auto& param_symbol : parameters)
    {
        func_compiler.stack.push_back(param_symbol);
    }

    func_compiler.visit(body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = workspace->pool->allocate_function_definition(
        std::move(code),
        name
    );
    emit(OpCode::LOAD_CONST, const_id, "fun " + name);

    emit_closure_upvalues(func_compiler.upvalues);
}

void Compiler::visit(TypeAliasDefinition& def)
{
    int physical_index = get_or_add_local_index(def.symbol);

    if (physical_index != -1)
    {
        emit(OpCode::LOAD_NONE);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "compile-time alias: " + def.name
        );
    }
}

void Compiler::visit(TraitDefinition& trait_definition)
{
}

void Compiler::visit(EnumDefinition& def)
{
}

void Compiler::visit(MethodDefinition& statement)
{
}

void Compiler::visit(FieldDefinition& statement)
{
}

// ============================================================================
// Expression Visitors
// ============================================================================

void Compiler::visit(std::vector<Expression_ptr>& expressions)
{
    for (const auto& expr : expressions)
    {
        visit(expr);
    }
}

void Compiler::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Compiler);

    std::visit(
        [this](auto& node)
        {
            if constexpr (requires { this->visit(node); })
            {
                this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unimplemented expression compilation"
                );
            }
        },
        expr->data
    );
}

void Compiler::visit(Identifier& expr)
{
    auto symbol = expr.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->is_native())
    {
        std::string mangled = mangle_name(
            symbol->name,
            "",
            symbol->module_path
        );
        auto id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, id, mangled);
    }
    else if (expr.must_be_captured)
    {
        int upval_index = resolve_upvalue(this, symbol);
        emit(OpCode::GET_UPVALUE, upval_index, symbol->name);
    }
    else
    {
        int stack_index = resolve_local(symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Attempted to read an unresolved local variable: " + symbol->name
        );

        emit(OpCode::GET_LOCAL, stack_index, symbol->name);
    }
}

void Compiler::visit(MemberAccess& access)
{
    if (access.is_enum_value)
    {
        int const_id = workspace->pool->allocate(access.member_index);
        emit(OpCode::LOAD_CONST, const_id);
        return;
    }

    visit(access.left);
    emit(OpCode::GET_MEMBER, access.member_index);
}

void Compiler::visit(Call& expr)
{
    visit(expr.callable);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;
    emit(OpCode::RESOLVE_FUNCTION, resolve_idx);

    int total_arguments = static_cast<int>(expr.arguments.size());

    if (expr.is_method_call)
    {
        auto& mac = expr.callable->as<MemberAccess>();
        visit(mac.left);
        total_arguments++;
    }

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    emit(OpCode::CALL, total_arguments);
}

void Compiler::visit(Constructor& expr)
{
    for (const auto& arg : expr.values)
    {
        visit(arg);
    }

    visit(expr.construtable);
    emit(OpCode::INSTANTIATE, static_cast<int>(expr.values.size()));
}

void Compiler::visit(TemplateAngular& expr)
{
    visit(expr.target);
}

// ============================================================================
// Primitives & Math
// ============================================================================

void Compiler::visit(IntegerLiteral& expr)
{
    emit(
        OpCode::LOAD_CONST,
        workspace->pool->allocate(expr.value),
        std::to_string(expr.value)
    );
}

void Compiler::visit(FloatLiteral& expr)
{
    emit(
        OpCode::LOAD_CONST,
        workspace->pool->allocate(expr.value),
        std::to_string(expr.value)
    );
}

void Compiler::visit(StringLiteral& expr)
{
    emit(OpCode::LOAD_CONST, workspace->pool->allocate(expr.value), expr.value);
}

void Compiler::visit(BooleanLiteral& expr)
{
    emit(expr.value ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
}

void Compiler::visit(Prefix& expr)
{
    visit(expr.operand);

    switch (expr.op.type)
    {
    case TokenType::MINUS:
        emit(OpCode::NEGATE);
        break;
    case TokenType::NOT:
        emit(OpCode::NOT);
        break;
    default:
        break;
    }
}

void Compiler::visit(Infix& expr)
{
    visit(expr.left);
    visit(expr.right);

    switch (expr.op.type)
    {
    case TokenType::PLUS:
        emit(OpCode::ADD);
        break;
    case TokenType::MINUS:
        emit(OpCode::SUB);
        break;
    case TokenType::STAR:
        emit(OpCode::MUL);
        break;
    case TokenType::DIVISION:
        emit(OpCode::DIV);
        break;
    case TokenType::MOD:
        emit(OpCode::MOD);
        break;
    case TokenType::EQUAL_EQUAL:
        emit(OpCode::EQ);
        break;
    case TokenType::BANG_EQUAL:
        emit(OpCode::NE);
        break;
    case TokenType::GREATER_THAN:
        emit(OpCode::GT);
        break;
    case TokenType::GREATER_THAN_EQUAL:
        emit(OpCode::GE);
        break;
    case TokenType::LESSER_THAN:
        emit(OpCode::LT);
        break;
    case TokenType::LESSER_THAN_EQUAL:
        emit(OpCode::LE);
        break;
    default:
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Unsupported infix operator in compiler: " + to_string(expr.op.type)
        );
        break;
    }
}

// ============================================================================
// Data Structures
// ============================================================================

void Compiler::visit(ListLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_LIST, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(TupleLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_TUPLE, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(SetLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_SET, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(MapLiteral& expr)
{
    for (auto& [k, v] : expr.pairs)
    {
        visit(k);
        visit(v);
    }
    emit(OpCode::BUILD_MAP, static_cast<int>(expr.pairs.size()));
}

void Compiler::visit(RangeLiteral& expr)
{
    if (expr.start)
    {
        visit(expr.start);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    if (expr.end)
    {
        visit(expr.end);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    if (expr.step)
    {
        visit(expr.step);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::BUILD_RANGE, expr.is_inclusive ? 1 : 0);
}

void Compiler::visit(DotLiteral& expr)
{
}

void Compiler::visit(Postfix& expr)
{
}

// -----------------------------------------------------------------------
// Ternary Branch
// -----------------------------------------------------------------------

void Compiler::visit(IfTernaryBranch& expr)
{
    BlockId test_block = graph.create_block();
    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    graph.add_edge(current_block_id, test_block);
    graph.add_edge(test_block, true_block);
    graph.add_edge(test_block, false_block);
    graph.add_edge(true_block, end_block);
    graph.add_edge(false_block, end_block);

    emit(OpCode::JUMP, static_cast<int>(test_block));

    set_current_block(test_block);
    enter_scope("test");
    visit(expr.test);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));
    emit(OpCode::JUMP, static_cast<int>(true_block));

    set_current_block(true_block);
    enter_scope("true branch");
    visit(expr.true_expression);
    leave_scope_keep_tos("true branch keep TOS");
    leave_scope_keep_tos("test");

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(false_block);
    dumb_leave_scope("test");
    enter_scope("false branch");

    if (expr.alternative)
    {
        visit(expr.alternative);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    leave_scope_keep_tos("false branch keep TOS");
    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(end_block);
}

void Compiler::visit(ElseTernaryBranch& expr)
{
    visit(expr.expression);
}

// -----------------------------------------------------------------------
// Conditional Branch
// -----------------------------------------------------------------------

void Compiler::visit(IfBranch& statement)
{
    BlockId test_block = graph.create_block();
    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    graph.add_edge(current_block_id, test_block);
    graph.add_edge(test_block, true_block);
    graph.add_edge(test_block, false_block);
    graph.add_edge(true_block, end_block);
    graph.add_edge(false_block, end_block);

    emit(OpCode::JUMP, static_cast<int>(test_block));

    set_current_block(test_block);
    enter_scope("test and true branch");
    visit(statement.test);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));
    emit(OpCode::JUMP, static_cast<int>(true_block));

    set_current_block(true_block);
    visit(statement.body);
    leave_scope("test and true branch");

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(false_block);
    dumb_leave_scope("test and true branch");

    if (statement.alternative.has_value())
    {
        auto& alt_variant = statement.alternative.value()->data;

        if (std::holds_alternative<IfBranch>(alt_variant))
        {
            visit(std::get<IfBranch>(alt_variant));
        }
        else if (std::holds_alternative<ElseBranch>(alt_variant))
        {
            visit(std::get<ElseBranch>(alt_variant));
        }
    }

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(end_block);
}

void Compiler::visit(ElseBranch& statement)
{
    enter_scope("else branch");
    visit(statement.body);
    leave_scope("else branch");
}

// ============================================================================
// Loops
// ============================================================================

void Compiler::visit(ForInLoop& statement)
{
    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId end = graph.create_block();

    graph.add_edge(current_block_id, header);
    graph.add_edge(header, body);
    graph.add_edge(header, end);
    graph.add_edge(body, header);

    enter_scope("for-in loop");

    visit(statement.iterable_expression);
    emit(OpCode::GET_ITER);

    stack.push_back(statement.iterator_symbol);

    Doctor::get().assert(
        statement.lhs->is<Identifier>(),
        WaspStage::Compiler,
        "For-in loop variable must be an identifier."
    );

    auto symbol = statement.lhs->as<Identifier>().symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);
    stack.push_back(symbol);

    emit(OpCode::JUMP, static_cast<int>(header));

    set_current_block(header);

    emit(OpCode::LOOP_ITER, static_cast<int>(end));
    emit(OpCode::JUMP, static_cast<int>(body));

    loop_tracking_stack.emplace_back(
        header,
        body,
        end,
        current_lexical_scope_depth,
        current_lexical_scope_depth
    );

    set_current_block(body);

    visit(statement.body);

    emit(OpCode::POP, "pop loop variable of this iteration");
    emit(OpCode::JUMP, static_cast<int>(header));

    loop_tracking_stack.pop_back();
    set_current_block(end);

    emit(OpCode::POP, "remove iterator object");
    leave_scope("for-in loop");
}

void Compiler::visit(SimpleLoop& statement)
{
    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId cleanup_block = graph.create_block();
    BlockId end = graph.create_block();

    graph.add_edge(current_block_id, header);
    graph.add_edge(header, body);
    graph.add_edge(header, cleanup_block);
    graph.add_edge(body, header);
    graph.add_edge(cleanup_block, end);

    emit(OpCode::JUMP, static_cast<int>(header));

    loop_tracking_stack.emplace_back(
        header,
        body,
        end,
        current_lexical_scope_depth,
        current_lexical_scope_depth + 1
    );

    set_current_block(header);
    enter_scope("loop header");
    visit(statement.condition);

    if (statement.style == TokenType::UNTIL ||
        statement.style == TokenType::UNLESS)
    {
        emit(OpCode::NOT);
    }

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(cleanup_block));
    emit(OpCode::JUMP, static_cast<int>(body));

    set_current_block(body);
    enter_scope("loop body");
    visit(statement.body);
    leave_scope("loop body");

    emit_local_cleanups(current_lexical_scope_depth - 1);

    emit(OpCode::JUMP, static_cast<int>(header));

    set_current_block(cleanup_block);
    leave_scope("loop header");

    emit(OpCode::JUMP, static_cast<int>(end));

    loop_tracking_stack.pop_back();
    set_current_block(end);
}

void Compiler::visit(LoopControl& statement)
{
    Doctor::get().assert(
        !loop_tracking_stack.empty(),
        WaspStage::Compiler,
        "Loop control outside loop"
    );

    auto [header, body, end, entry_depth, body_depth] = loop_tracking_stack
                                                            .back();

    int target_depth = (statement.type == TokenType::REDO) ? body_depth
                                                           : entry_depth;

    emit_local_cleanups(target_depth);

    if (statement.type == TokenType::BREAK)
    {
        emit(OpCode::JUMP, static_cast<int>(end));
        graph.add_edge(current_block_id, end);
    }
    else if (statement.type == TokenType::CONTINUE)
    {
        emit(OpCode::JUMP, static_cast<int>(header));
        graph.add_edge(current_block_id, header);
    }
    else if (statement.type == TokenType::REDO)
    {
        emit(OpCode::JUMP, static_cast<int>(body));
        graph.add_edge(current_block_id, body);
    }
}

// ============================================================================
// Other
// ============================================================================

void Compiler::visit(Pass& statement)
{
}

void Compiler::visit(Native& statement)
{
}

} // namespace Wasp
