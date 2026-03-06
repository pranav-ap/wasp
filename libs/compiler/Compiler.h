#pragma once

#include "Statement.h"
#include "ConstantPool.h"
#include "CFGraph.h"
#include "Symbol.h"

#include <memory>
#include <vector>
#include <tuple>
#include <string>
#include <map>

namespace Wasp
{
	struct Upvalue
	{
		// The index of the variable to capture.
		// If is_local is true: index into the parent's stack.
		// If is_local is false: index into the parent's own upvalue array.
		int index;

		// Determines where the value is located at runtime during closure creation.
		bool is_local;
	};

	class Compiler
	{
	private:
		ConstantPool_ptr constant_pool;

		// ------------------------------------------------------------------------
		// Closure Support
		// ------------------------------------------------------------------------

		Compiler *parent;
		std::vector<Upvalue> upvalues;

		// Tracks nesting closure_depth: 0 = Global/Module, 1 = Main Function, 2 = Inner Function
		int compiler_depth = 0;
		int current_lexical_scope_depth = 0;

		int add_upvalue(int index, bool is_local);
		int resolve_upvalue(Compiler *current_compiler, Symbol_ptr symbol);

		// ------------------------------------------------------------------------
		// Control Flow Graph
		// ------------------------------------------------------------------------

		CFGraph graph;
		BlockId current_block_id;

		// Stack of <HeaderBlockId, BodyBlockId, EndBlockId, lexical_scope_depth, body depth>
		std::vector<std::tuple<BlockId, BlockId, BlockId, int, int>> loop_tracking_stack;

		// -----------------------------------------------------------------------
		// Scope
		// -----------------------------------------------------------------------

		void enter_scope();
		void leave_scope();

		// -----------------------------------------------------------------------
		// Debugging
		// -----------------------------------------------------------------------
		std::map<int, std::string> debug_name_map;

		// -----------------------------------------------------------------------
		// Emit
		// ----------------------------------------------------------------------

		void emit(OpCode opcode);
		void emit(OpCode opcode, int operand);
		void emit(OpCode opcode, int operand_1, int operand_2);
		void emit_raw_byte(std::byte b);

		// -----------------------------------------------------------------------
		// UTILS
		// -----------------------------------------------------------------------

		void set_current_block(BlockId block_id) { current_block_id = block_id; }

		std::map<BlockId, size_t> calculate_block_offsets() const;
		void resolve_jumps_in_block(ByteVector &bytes, const std::map<BlockId, size_t> &offsets) const;

		void compile_variable_definition(const Expression_ptr &assignment, bool as_expression = false);
		void compile_assignment(const Expression_ptr &lhs, const Expression_ptr &rhs);

		CodeObject flatten();

	public:
		Compiler();
		Compiler(ConstantPool_ptr pool, Compiler *parent);

		const CFGraph &get_graph() const { return graph; }
		const std::map<int, std::string> &get_name_map() const { return debug_name_map; }

		std::tuple<ConstantPool_ptr, CodeObject> run(const Module &module);

		// ========================================================================
		// Visitors
		// ========================================================================

		void visit(const Statement_ptr statement);
		void visit(std::vector<Statement_ptr> &statements);

		void visit(ExpressionStatement &statement);
		void visit(VariableDefinition &statement);

		void visit(IfBranch &statement);
		void visit(ElseBranch &statement);

		void visit(SimpleLoop &statement);
		void visit(ForInLoop &statement);

		void visit(Pass &statement);
		void visit(LoopControl &statement);

		void visit(FunctionDefinition &statement);
		void visit(Return &statement);

		// Expressions
		void visit(const Expression_ptr expr);
		void visit(std::vector<Expression_ptr> &expressions);

		void visit(int expr);
		void visit(double expr);
		void visit(std::string expr);
		void visit(bool expr);

		void visit(Identifier &expr);
		void visit(DotLiteral &expr);

		void visit(Prefix &expr);
		void visit(Infix &expr);
		void visit(Postfix &expr);

		void visit(ListLiteral &expr);
		void visit(TupleLiteral &expr);
		void visit(MapLiteral &expr);
		void visit(SetLiteral &expr);
		void visit(RangeLiteral &expr);

		void visit(VariableDefinitionExpression &expr);
		void visit(UntypedAssignment &expr);
		void visit(TypedAssignment &expr);
		void visit(TypePattern &expr);

		void visit(IfTernaryBranch &expr);
		void visit(ElseTernaryBranch &expr);

		void visit(Call &expr);
	};

	using Compiler_ptr = std::shared_ptr<Compiler>;
}