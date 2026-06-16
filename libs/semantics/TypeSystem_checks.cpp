#include "Doctor.h"
// keep
#include "Expression.h"
#include "Type.h"
#include "TypeSystem.h"

namespace Wasp
{

bool TypeSystem::is_int_type(Type_ptr obj) const
{
    Doctor::semantics().fatal_if_nullptr(obj);

    if (obj->is<IntType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<IntegerLiteral>();
    }

    return false;
}

bool TypeSystem::is_float_type(Type_ptr obj) const
{
    Doctor::semantics().fatal_if_nullptr(obj);

    if (obj->is<FloatType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<FloatLiteral>();
    }

    return false;
}

bool TypeSystem::is_number_type(Type_ptr obj) const
{
    return is_int_type(obj) || is_float_type(obj);
}

bool TypeSystem::is_string_type(Type_ptr obj) const
{
    Doctor::semantics().fatal_if_nullptr(obj);

    if (obj->is<StringType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<StringLiteral>();
    }

    return false;
}

bool TypeSystem::is_boolean_type(Type_ptr obj) const
{
    Doctor::semantics().fatal_if_nullptr(obj);

    if (obj->is<BooleanType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<BooleanLiteral>();
    }

    return false;
}

bool TypeSystem::is_none_type(const Type_ptr type) const
{
    Doctor::semantics().fatal_if_nullptr(type);
    return type->is<NoneType_ptr>();
}

bool TypeSystem::is_primitive_type(const Type_ptr type) const
{
    Doctor::semantics().fatal_if_nullptr(type);

    return type->is<IntType_ptr>() || type->is<FloatType_ptr>() ||
           type->is<StringType_ptr>() || type->is<BooleanType_ptr>() ||
           type->is<NoneType_ptr>() || type->is<AnyType_ptr>() ||
           type->is<LiteralType_ptr>() || type->is<ListType_ptr>() ||
           type->is<SetType_ptr>() || type->is<MapType_ptr>() ||
           type->is<TupleType_ptr>() || type->is<VariantType_ptr>() ||
           type->is<IntersectionType_ptr>();
}

bool TypeSystem::is_key_type(const Type_ptr type) const
{
    return is_int_type(type) || is_string_type(type) ||
           is_boolean_type(type);
}

} // namespace Wasp
