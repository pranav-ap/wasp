#include "Expression.h"
#include "AST.h"
#include "Doctor.h"
#include <algorithm>

namespace Wasp
{
ExpressionVector MemberAccess::get_access_path() const
{
    ExpressionVector path;

    const MemberAccess* current = this;

    while (current)
    {
        path.push_back(current->right);

        if (current->left->is<Identifier>())
        {
            path.push_back(current->left);
            break;
        }

        Doctor::get().assert(
            current->left->is<MemberAccess>(),
            WaspStage::Semantics,
            "Invalid member access path. LHS of '.' must be either an identifier or another member "
            "access.");

        current = &(current->left->as<MemberAccess>());
    }

    // Reverse it so it reads left-to-right (e.g., [a, b, c])
    std::reverse(path.begin(), path.end());

    return path;
}
} // namespace Wasp
