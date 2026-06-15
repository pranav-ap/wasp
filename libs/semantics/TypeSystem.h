#pragma once

#include "Type.h"

#include <memory>

namespace Wasp
{

struct TypeSystem
{
    explicit TypeSystem() {};

    // =========================================================================
    // Type Checks
    // =========================================================================

    bool is_boolean_type(const Type_ptr type) const;
    bool is_number_type(const Type_ptr type) const;
    bool is_int_type(const Type_ptr type) const;
    bool is_float_type(const Type_ptr type) const;
    bool is_string_type(const Type_ptr type) const;
    bool is_none_type(const Type_ptr type) const;
    bool is_primitive_type(const Type_ptr type) const;
};

using TypeSystem_ptr = std::shared_ptr<TypeSystem>;
} // namespace Wasp
