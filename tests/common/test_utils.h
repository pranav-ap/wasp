#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"

#include <gtest/gtest.h>
#include <string>

Wasp::Block parse(const std::string& code);

template <typename T> const T& check(const Wasp::Expression_ptr& ptr)
{

    if (!ptr)
    {
        ADD_FAILURE() << "Target pointer is null";
        static T dummy{};
        return dummy;
    }

    if (ptr->is<T>())
    {
        return ptr->as<T>();
    }

    ADD_FAILURE() << "AST Node Type Mismatch";

    static T dummy{};
    return dummy;
}

template <typename T, typename PtrType> const T& check(const PtrType& ptr)
{
    if (!ptr)
    {
        ADD_FAILURE() << "Null pointer";
        static T dummy{};
        return dummy;
    }

    if (ptr->template is<T>())
    {
        return ptr->template as<T>();
    }

    ADD_FAILURE() << "Type mismatch";
    static T dummy{};
    return dummy;
}
