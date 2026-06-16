#include "Statement.h"
#include "AST.h"
#include "Doctor.h"

#include <cctype>
#include <string>
#include <utility>

namespace Wasp
{

Statement_ptr Block::get(int index)
{
    Doctor::semantics().assert(
        index >= 0 || index < static_cast<int>(statements.size()),
        "Block index out of bounds: " + std::to_string(index) +
            " (size: " + std::to_string(statements.size()) + ")"
    );

    return statements[index];
}

void Block::add(Statement_ptr statement)
{
    statements.push_back(std::move(statement));
}

int Block::size() const
{
    return static_cast<int>(statements.size());
}

} // namespace Wasp
