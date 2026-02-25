#pragma once

#include "Statement.h"
#include "ObjectStore.h"
#include "CFGraph.h"
#include "SymbolScope.h"

#include <memory>
#include <vector>
#include <tuple>
#include <string>

namespace Wasp
{
	class Compiler
	{
	private:
		ConstantPool_ptr constant_pool;

		CFGraph graph;
		BlockId current_block_id;

		// -----------------------------------------------------------------------
		// Scoping and Symbol Resolution
		// -----------------------------------------------------------------------

		SymbolScope_ptr current_scope;
		// used for generating unique labels for jumps and branches
		int next_symbol_id;

		void enter_scope(ScopeType type);
		void leave_scope();

		std::map<int, std::string> debug_name_map;

		// -----------------------------------------------------------------------
		// Emit
		// ----------------------------------------------------------------------

		void emit(OpCode opcode);
		void emit(OpCode opcode, int operand);
		void emit(OpCode opcode, int operand_1, int operand_2);

		// ========================================================================
		// Statement Visitors
		// ========================================================================

		void visit(const Statement_ptr statement);
		void visit(std::vector<Statement_ptr> &statements);

		void visit(ExpressionStatement &statement);

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
		void visit(DotDotLiteral &expr);
		void visit(DotDotDotLiteral &expr);

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

		CodeObject flatten();

	public:
		Compiler();

		const std::map<int, std::string> &get_name_map() const { return debug_name_map; }
		std::tuple<ConstantPool_ptr, CodeObject> run(const Module &module);
	};

	using Compiler_ptr = std::shared_ptr<Compiler>;
}