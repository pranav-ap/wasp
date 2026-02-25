#include "ObjectStore.h"
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

namespace Wasp {

// ============================================================================
// ObjectStore
// ============================================================================

ObjectStore::ObjectStore() {
    objects[0] = MAKE_OBJECT_VARIANT(AnyType());
    objects[1] = MAKE_OBJECT_VARIANT(IntType());
    objects[2] = MAKE_OBJECT_VARIANT(FloatType());
    objects[3] = MAKE_OBJECT_VARIANT(StringType());
    objects[4] = MAKE_OBJECT_VARIANT(BooleanType());
    objects[5] = MAKE_OBJECT_VARIANT(NoneType());

    objects[6] = MAKE_OBJECT_VARIANT(BooleanLiteralType(true));
    objects[7] = MAKE_OBJECT_VARIANT(BooleanLiteralType(false));

    objects[8] = MAKE_OBJECT_VARIANT(BooleanObject(true));
    objects[9] = MAKE_OBJECT_VARIANT(BooleanObject(false));
}

Object_ptr ObjectStore::get(int id) {
    auto it = objects.find(id);
    ASSERT(it != objects.end(), "ID does not exist in ObjectStore");
    return it->second;
}

Object_ptr ObjectStore::get_any_type() { return get(0); }
Object_ptr ObjectStore::get_int_type() { return get(1); }
Object_ptr ObjectStore::get_float_type() { return get(2); }
Object_ptr ObjectStore::get_string_type() { return get(3); }
Object_ptr ObjectStore::get_boolean_type() { return get(4); }
Object_ptr ObjectStore::get_none_type() { return get(5); }
Object_ptr ObjectStore::get_true_literal_type() { return get(6); }
Object_ptr ObjectStore::get_false_literal_type() { return get(7); }
Object_ptr ObjectStore::get_true_object() { return get(8); }
Object_ptr ObjectStore::get_false_object() { return get(9); }

Object_ptr ObjectStore::make_object(bool value) {
    return value ? get_true_object() : get_false_object();
}

Object_ptr ObjectStore::make_object(int value) {
    return MAKE_OBJECT_VARIANT(IntObject(value));
}

Object_ptr ObjectStore::make_object(double value) {
    return MAKE_OBJECT_VARIANT(FloatObject(value));
}

Object_ptr ObjectStore::make_object(std::string value) {
    return MAKE_OBJECT_VARIANT(StringObject(value));
}

Object_ptr ObjectStore::make_error_object(std::string text) {
    return MAKE_OBJECT_VARIANT(std::make_shared<ErrorObject>(text));
}

// ============================================================================
// DefinitionStore
// ============================================================================

void DefinitionStore::create(int id, Object_ptr value) {
    ASSERT(objects.find(id) == objects.end(), "ID already exists in ObjectStore");
    objects[id] = value;
}

void DefinitionStore::set(int id, Object_ptr value) {
    // This will overwrite if it exists, or create if it doesn't
    objects[id] = value; 
}

void DefinitionStore::discard(int id) {
    ASSERT(objects.find(id) != objects.end(), "ID does not exist in ObjectStore");
    objects.erase(id);
}

// ============================================================================
// ConstantPool
// ============================================================================

int ConstantPool::allocate() {
    return next_id++;
}

int ConstantPool::allocate(Object_ptr value) {
    int id = next_id++;
    objects[id] = std::move(value);
    return id;
}

int ConstantPool::allocate(int number) {
    if (auto it = int_cache.find(number); it != int_cache.end()) {
        return it->second;
    }

    int id = next_id++;
    objects[id] = MAKE_OBJECT_VARIANT(IntObject(number));
    int_cache[number] = id;
    return id;
}

int ConstantPool::allocate(double number) {
    if (auto it = float_cache.find(number); it != float_cache.end()) {
        return it->second;
    }

    int id = next_id++;
    objects[id] = MAKE_OBJECT_VARIANT(FloatObject(number));
    float_cache[number] = id;
    return id;
}

int ConstantPool::allocate(std::string text) {
    if (auto it = string_cache.find(text); it != string_cache.end()) {
        return it->second;
    }

    int id = next_id++;
    objects[id] = MAKE_OBJECT_VARIANT(StringObject(text));
    string_cache[text] = id;
    return id;
}

int ConstantPool::allocate_type(Object_ptr new_value) {
    auto result = std::find_if(
        objects.begin(), objects.end(),
        [new_value](const auto& p) {
            return are_equal_types(new_value, p.second);
        });

    if (result != objects.end()) {
        return result->first;
    }

    int id = next_id++;
    objects[id] = new_value;
    return id;
}

}
