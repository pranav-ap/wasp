#pragma once

#include "AST.h"
#include "CFGraph.h"
#include "Expression.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace Wasp
{

struct Upvalue
{
    // TRUE:  The variable is a local variable on the immediate parent's stack.
    // FALSE: The variable is an upvalue already captured by the immediate
    // parent.
    bool is_local_to_parent;

    // If is_local_to_parent == true:  Offset from the parent's stack Base
    // Pointer. If is_local_to_parent == false: Offset into the parent's upvalue
    // array.
    int index;
};

class Compiler
{
public:
    Compiler(Workspace_ptr workspace);
    Compiler(Compiler* parent);

    const CFGraph& get_graph() const
    {
        return graph;
    }

    FunctionBlueprintObject_ptr run(
        const StatementVector& block,
        std::string filepath,
        bool is_main = false
    );

private:
    Workspace_ptr workspace;
    Compiler* parent;
    std::string module_path;

    // ------------------------------------------------------------------------
    // Symbols & Closure Support
    // ------------------------------------------------------------------------

    std::vector<Upvalue> upvalues;
    SymbolVector stack;

    int compiler_depth = 0;
    int current_lexical_scope_depth = 0;

    int add_upvalue(const Upvalue& uv, const std::string& name);
    int resolve_upvalue(Compiler* current_compiler, Symbol_ptr symbol);

    int resolve_local(int symbol_id);
    int resolve_local(const std::string& name);

    // ------------------------------------------------------------------------
    // Control Flow Graph
    // ------------------------------------------------------------------------

    CFGraph graph;
    BlockId current_block_id;

    // Stack of <HeaderBlockId, BodyBlockId, EndBlockId, lexical_scope_depth,
    // body depth>
    std::vector<std::tuple<BlockId, BlockId, BlockId, int, int>>
        loop_tracking_stack;

    // -----------------------------------------------------------------------
    // Scope
    // -----------------------------------------------------------------------

    void enter_scope(std::string comment = "");
    void leave_scope(std::string comment = "");
    void leave_scope_keep_tos(std::string comment = "");
    void dumb_leave_scope(std::string comment = "");

    // -----------------------------------------------------------------------
    // Emit
    // ----------------------------------------------------------------------

    void emit(OpCode opcode, std::string comment = "");
    void emit(OpCode opcode, int operand, std::string comment = "");
    void emit(
        OpCode opcode,
        int operand_1,
        int operand_2,
        std::string comment = ""
    );
    void emit_raw_byte(std::byte b);

    void emit_local_cleanups(int target_depth);
    void emit_exports();

    // ========================================================================
    // Visitors
    // ========================================================================

    void visit(const Statement_ptr statement);
    void visit(std::vector<Statement_ptr>& statements);

    void visit(ExpressionStatement& statement);

    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);

    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);

    void visit(LoopControl& statement);
    void visit(Import& statement);

    void visit(FunctionDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(TypeAliasDefinition& statement);
    void visit(EnumDefinition& statement);

    void visit(Return& statement);

    void compile_function_closure(
        const std::string& name,
        const std::vector<Symbol_ptr>& parameters,
        StatementVector body,
        Symbol_ptr context_symbol = nullptr
    );

    // Expressions

    void visit(const Expression_ptr expr);
    void visit(std::vector<Expression_ptr>& expressions);

    void visit(IntegerLiteral& expr);
    void visit(FloatLiteral& expr);
    void visit(StringLiteral& expr);
    void visit(BooleanLiteral& expr);

    void visit(Identifier& expr);

    void visit(MemberAccess& expr);
    void visit(EnumMember& expr);

    void visit(Call& expr);

    void visit(Constructor& expr);

    void visit(Prefix& expr);
    void visit(Infix& expr);
    void visit(Postfix& expr);

    void visit(ListLiteral& expr);
    void visit(TupleLiteral& expr);
    void visit(MapLiteral& expr);
    void visit(SetLiteral& expr);

    void visit(Assignment& expr);

    void visit(IfTernaryBranch& expr);
    void visit(ElseTernaryBranch& expr);

    void visit(TemplateAngular& expr);
    void visit(Box& node);

    // -----------------------------------------------------------------------
    // UTILS
    // -----------------------------------------------------------------------

    int get_or_add_local_index(const Symbol_ptr& symbol);

    void set_current_block(BlockId block_id)
    {
        current_block_id = block_id;
    }

    std::map<BlockId, size_t> calculate_block_offsets() const;

    void resolve_jumps_in_block(
        CodeObject& code,
        const std::map<BlockId, size_t>& offsets
    ) const;

    void compile_variable_definition(
        const Expression_ptr& assignment,
        bool as_expression = false
    );
    void compile_identifier_assignment(
        Identifier& id,
        const Expression_ptr& rhs
    );
    void compile_member_assignment(
        MemberAccess& mac,
        const Expression_ptr& rhs
    );

    std::string mangle_name(
        const std::string& fn_name,
        const std::string& class_name,
        const std::string& path_override
    );

    CodeObject flatten();
};

using Compiler_ptr = std::shared_ptr<Compiler>;
} // namespace Wasp
