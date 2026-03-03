#pragma once

#include "Statement.h"
#include "ObjectStores.h"
#include "CFGraph.h"
#include "SymbolScope.h"

#include <memory>
#include <vector>
#include <tuple>
#include <string>

namespace Wasp
{
	struct Upvalue
	{
		int index;	   // The ID in the parent's locals OR the parent's upvalues
		bool is_local; // True if it's a local in the immediate parent scope
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

		int add_upvalue(int index, bool is_local);
		int resolve_upvalue(Compiler *current_compiler, Symbol_ptr symbol);

		// ------------------------------------------------------------------------
		// Control Flow Graph
		// ------------------------------------------------------------------------

		CFGraph graph;
		BlockId current_block_id;

		// Stack of <HeaderBlockId, BodyBlockId, EndBlockId>
		std::vector<std::tuple<BlockId, BlockId, BlockId>> loop_tracking_stack;

		// -----------------------------------------------------------------------
		// Scoping and Symbol Resolution
		// -----------------------------------------------------------------------

		SymbolScope_ptr current_scope;
		// used for generating unique labels for jumps and branches
		int next_symbol_id;

		void enter_scope(ScopeType type, bool emit_opcodes);
		void leave_scope(bool emit_opcodes);

		std::map<int, std::string> debug_name_map;

		// -----------------------------------------------------------------------
		// Emit
		// ----------------------------------------------------------------------

		void emit(OpCode opcode);
		void emit(OpCode opcode, int operand);
		void emit(OpCode opcode, int operand_1, int operand_2);

		void emit_raw_byte(std::byte b);

		// ========================================================================
		// Statement Visitors
		// ========================================================================

		void visit(const Statement_ptr statement);
		void visit(std::vector<Statement_ptr> &statements);

		void visit(ExpressionStatement &statement);
		void visit(VariableDefinition &statement);

		void visit(IfBranch &statement);
		void visit_elif(IfBranch &statement, int exit_block_id);
		void visit(ElseBranch &statement);

		void visit(SimpleLoop &statement);
		void visit(ForInLoop &statement);

		void visit(Pass &statement);
		void visit(LoopControl &statement);

		void visit(FunctionDefinition &statement);
		void visit(Return &statement);

		// ========================================================================
		// Expression Visitors (Now returning void!)
		// ========================================================================

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

		// -----------------------------------------------------------------------
		// UTILS
		// -----------------------------------------------------------------------
		void set_current_block(BlockId block_id);

		std::map<BlockId, size_t> calculate_block_offsets() const;
		void resolve_jumps_in_block(ByteVector &bytes, const std::map<BlockId, size_t> &offsets) const;

		void compile_variable_definition(const Expression_ptr &assignment, bool is_mutable, bool as_expression = false);
		void compile_assignment(const Expression_ptr &lhs, const Expression_ptr &rhs);

		CodeObject flatten();

	public:
		Compiler();
		Compiler(ConstantPool_ptr pool, SymbolScope_ptr enclosing_scope, Compiler *parent = nullptr);

		const CFGraph &get_graph() const { return graph; }
		const std::map<int, std::string> &get_name_map() const { return debug_name_map; }

		std::tuple<ConstantPool_ptr, CodeObject> run(const Module &module);
	};

	using Compiler_ptr = std::shared_ptr<Compiler>;
}