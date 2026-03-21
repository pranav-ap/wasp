#include "Expression.h"
#include "AST.h"
#include <algorithm>

namespace Wasp
{
ExpressionVector MemberAccess::flatten_links() const
{
    ExpressionVector path;

    const MemberAccess* current = this;

    while (current)
    {
        path.push_back(current->right);

        // If the left side is another MemberAccess, keep traversing down the chain
        if (current->left->is<MemberAccess>())
        {
            current = &(current->left->as<MemberAccess>());
        }
        // Otherwise, we've hit the root expression (Identifier, Call, Literal, etc.)
        else
        {
            path.push_back(current->left);
            break;
        }
    }

    std::reverse(path.begin(), path.end());

    return path;
}
} // namespace Wasp
