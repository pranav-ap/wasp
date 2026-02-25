#pragma once

#include "Objects.h"
#include <memory>
#include <unordered_map> 
#include <string>

namespace Wasp {

class ObjectStore {
protected:
    std::unordered_map<int, Object_ptr> objects;

public:
    ObjectStore();
    virtual ~ObjectStore() = default;

    Object_ptr get(int id);

    Object_ptr get_any_type();
    Object_ptr get_int_type();
    Object_ptr get_float_type();
    Object_ptr get_string_type();
    Object_ptr get_boolean_type();
    Object_ptr get_none_type();

    Object_ptr get_true_literal_type();
    Object_ptr get_false_literal_type();

    Object_ptr get_true_object();
    Object_ptr get_false_object();

    Object_ptr make_object(bool value);
    Object_ptr make_object(int value);
    Object_ptr make_object(double value);
    Object_ptr make_object(std::string value);
    Object_ptr make_error_object(std::string text);
};

class ConstantPool : public ObjectStore {
private:
    int next_id;

    std::unordered_map<int, int> int_cache;
    std::unordered_map<double, int> float_cache;
    std::unordered_map<std::string, int> string_cache;

public:
    ConstantPool() : ObjectStore(), next_id(10) {};

    int allocate(); 
    int allocate(Object_ptr value);
    
    int allocate(int value);
    int allocate(double value);
    int allocate(std::string value);
    
    int allocate_type(Object_ptr value);
};

class DefinitionStore : public ObjectStore {
public:
    DefinitionStore() : ObjectStore() {};

    void create(int id, Object_ptr value);
    void set(int id, Object_ptr value);
    void discard(int id);
};

using ObjectStore_ptr = std::shared_ptr<ObjectStore>;
using ConstantPool_ptr = std::shared_ptr<ConstantPool>;
using DefinitionStore_ptr = std::shared_ptr<DefinitionStore>;

}