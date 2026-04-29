#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{
template <typename DefType>
void parse_common_members(
    DefType& def,
    ObjectStringMap& members,
    StringVector& methods,
    StringVector& pures,
    StringVector& statics
)
{
    auto add_m = [&](const std::string& n, auto& vec)
    {
        if (std::find(vec.begin(), vec.end(), n) == vec.end())
        {
            vec.push_back(n);
            members[n] = make_object(std::make_shared<ObjectOverloadList>(n));
        }
    };

    auto add_s = [&](const std::string& n)
    {
        if (std::find(statics.begin(), statics.end(), n) == statics.end())
        {
            statics.push_back(n);
        }
    };

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    add_m(m.name, methods);
                },
                [&](OurMethodDefinition& m)
                {
                    add_m(m.name, methods);
                    add_s(m.name);
                },
                [&](PureMethodDefinition& p)
                {
                    add_m(p.name, pures);
                },
                [&](OurPureMethodDefinition& p)
                {
                    add_m(p.name, pures);
                    add_s(p.name);
                },
                [](FieldDefinition&)
                {
                },
                [](auto&)
                {
                    Doctor::get().fatal(WaspStage::Semantics, "Invalid statement in body.");
                }
            },
            stmt->data
        );
    }
}
} // namespace

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch."
    );
}

template <typename DefType, typename TypeObjPtr, typename BaseTypePtr>
void SemanticAnalyzer::analyze_membered_type(
    DefType& def,
    TypeObjPtr type_obj,
    BaseTypePtr base_type
)
{
    def.symbol->set_type(type_obj);

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    hoist_method(base_type, m);
                },
                [&](PureMethodDefinition& m)
                {
                    hoist_method(base_type, m);
                },
                [&](OurMethodDefinition& m)
                {
                    hoist_method(base_type, m);
                },
                [&](OurPureMethodDefinition& m)
                {
                    hoist_method(base_type, m);
                },
                [](auto&)
                {
                }
            },
            stmt->data
        );
    }

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    analyze_method_base(type_obj, m, ScopeType::METHOD, "my", true);
                },
                [&](OurMethodDefinition& m)
                {
                    analyze_method_base(type_obj, m, ScopeType::METHOD, "our", true);
                },
                [&](PureMethodDefinition& m)
                {
                    analyze_method_base(nullptr, m, ScopeType::PURE_METHOD, "my", false);
                },
                [&](OurPureMethodDefinition& m)
                {
                    analyze_method_base(nullptr, m, ScopeType::PURE_METHOD, "our", false);
                },
                [](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = initialize_class_type(def);
    analyze_membered_type(def, make_object(class_type), class_type);

    for (const auto& trait_name : def.traits)
    {
        verify_trait_compliance(def, class_type, trait_name);
    }
}

void SemanticAnalyzer::verify_trait_compliance(
    ClassDefinition& def,
    ClassType_ptr class_type,
    const std::string& trait_name
)
{
    auto symbol = current_scope->lookup(trait_name);
    Doctor::get()
        .fatal_if_nullptr(symbol, WaspStage::Semantics, "Trait '" + trait_name + "' not found.");

    auto trait_type = symbol->get_type()->as<TraitType_ptr>();

    for (auto const& [member_name, trait_member_type] : trait_type->members)
    {
        Doctor::get().assert(
            class_type->members.contains(member_name),
            WaspStage::Semantics,
            "Class '" + def.name + "' fails to implement required trait member: " + trait_name +
                "." + member_name
        );

        auto class_member_type = class_type->members.at(member_name);

        Doctor::get().assert(
            are_types_compatible(trait_member_type, class_member_type),
            WaspStage::Semantics,
            "Implementation of '" + member_name + "' in class '" + def.name +
                "' is incompatible with trait '" + trait_name + "'"
        );
    }
}

ObjectVector SemanticAnalyzer::extract_overloads(Object_ptr type_obj)
{
    if (type_obj->is<ObjectOverloadList_ptr>())
    {
        return type_obj->as<ObjectOverloadList_ptr>()->overloads;
    }
    return {type_obj};
}

bool SemanticAnalyzer::is_signature_compatible(Object_ptr trait_func, Object_ptr class_func)
{
    if (trait_func->is<MethodType_ptr>() && class_func->is<MethodType_ptr>())
    {
        auto trait_sig = trait_func->as<MethodType_ptr>();
        auto class_sig = class_func->as<MethodType_ptr>();

        if (trait_sig->parameter_types.size() != class_sig->parameter_types.size())
        {
            return false;
        }

        for (size_t i = 1; i < trait_sig->parameter_types.size(); ++i)
        {
            if (!type_checker->assignable(
                    current_scope,
                    trait_sig->parameter_types[i],
                    class_sig->parameter_types[i]
                ))
            {
                return false;
            }
        }

        return type_checker
            ->assignable(current_scope, trait_sig->return_type, class_sig->return_type);
    }

    return type_checker->assignable(current_scope, trait_func, class_func);
}

bool SemanticAnalyzer::are_types_compatible(
    Object_ptr trait_member_type,
    Object_ptr class_member_type
)
{
    ObjectVector trait_overloads = extract_overloads(trait_member_type);
    ObjectVector class_overloads = extract_overloads(class_member_type);

    for (const auto& trait_func : trait_overloads)
    {
        bool found_match = false;
        for (const auto& class_func : class_overloads)
        {
            if (is_signature_compatible(trait_func, class_func))
            {
                found_match = true;
                break;
            }
        }

        if (!found_match)
        {
            return false;
        }
    }

    return true;
}

ClassType_ptr SemanticAnalyzer::initialize_class_type(ClassDefinition& def)
{
    ObjectStringMap members;
    StringVector fields;
    StringVector methods;
    StringVector pures;
    StringVector statics;

    for (auto& stmt : def.members)
    {
        if (auto* f = std::get_if<FieldDefinition>(&stmt->data))
        {
            auto field_type = visit(f->type);

            Doctor::get().assert(
                std::find(fields.begin(), fields.end(), f->name) == fields.end(),
                WaspStage::Semantics,
                "Duplicate field name " + f->name
            );

            fields.push_back(f->name);
            members[f->name] = field_type;
        }
    }

    parse_common_members(def, members, methods, pures, statics);

    return std::make_shared<ClassType>(
        def.name,
        std::move(members),
        std::move(fields),
        std::move(methods),
        std::move(pures),
        std::move(statics)
    );
}

void SemanticAnalyzer::visit(TraitDefinition& def)
{
    auto trait_type = initialize_trait_type(def);
    analyze_membered_type(def, make_object(trait_type), trait_type);
}

TraitType_ptr SemanticAnalyzer::initialize_trait_type(TraitDefinition& def)
{
    ObjectStringMap members;
    StringVector methods;
    StringVector pures;
    StringVector statics;

    for (auto& stmt : def.members)
    {
        if (auto* f = std::get_if<FieldDefinition>(&stmt->data))
        {
            Doctor::get().fatal(WaspStage::Semantics, "Traits cannot contain fields.");
        }
    }

    parse_common_members(def, members, methods, pures, statics);

    return std::make_shared<TraitType>(
        def.name,
        std::move(members),
        std::move(methods),
        std::move(pures),
        std::move(statics)
    );
}

void SemanticAnalyzer::visit(FieldDefinition& stat)
{
    Doctor::get().fatal(WaspStage::Semantics, "Fields cannot be defined outside of a class.");
}

void SemanticAnalyzer::visit(MethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

void SemanticAnalyzer::visit(PureMethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

void SemanticAnalyzer::visit(OurMethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

void SemanticAnalyzer::visit(OurPureMethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

template <typename BaseTypePtr, typename MethodDef>
void SemanticAnalyzer::hoist_method(BaseTypePtr base_type, MethodDef& m)
{
    auto [return_type, parameter_types] = get_function_signature(m);

    Object_ptr signature = make_object(std::make_shared<MethodType>(parameter_types, return_type));

    Symbol_ptr symbol = SymbolFactory::create_method(
        m.name,
        signature,
        false,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    type_checker
        ->validate_new_method_overload(current_scope, base_type->get_overloads(m.name), symbol);

    base_type->add_overload(m.name, signature);
    m.symbol = symbol;
}

template <typename T>
void SemanticAnalyzer::analyze_method_base(
    Object_ptr class_or_trait_type_obj,
    T& m,
    ScopeType scope_type,
    const std::string& receiver_name,
    bool is_mutable
)
{
    enter_scope(scope_type);

    auto signature = m.symbol->get_type();
    auto [return_type, param_types] = get_function_signature(signature);
    return_type_stack.push_back(return_type);

    Doctor::get().assert(
        m.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty at this stage"
    );

    auto define_param = [&](const std::string& name, Object_ptr type)
    {
        auto sym = SymbolFactory::create_variable(
            name,
            type,
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(sym);
        m.parameter_symbols.push_back(sym);
    };

    if (!receiver_name.empty() && class_or_trait_type_obj)
    {
        define_param(receiver_name, class_or_trait_type_obj);
    }

    for (size_t i = 0; i < m.parameters.size(); ++i)
    {
        define_param(m.parameters[i].first, param_types[i]);
    }

    if (m.body.size() == 1 && m.body.front()->template is<Native>())
    {
        m.symbol->mark_as_native();
    }

    visit(m.body);

    return_type_stack.pop_back();

    leave_scope();
}

template <typename T>
void SemanticAnalyzer::analyze_function(T& def, ScopeType scope_type, bool is_mutable)
{
    enter_scope(scope_type);

    auto signature = def.symbol->get_type();
    auto [return_type, parameter_types] = get_function_signature(signature);
    return_type_stack.push_back(return_type);

    Doctor::get().assert(
        def.parameter_symbols.empty(),
        WaspStage::Semantics,
        "Expect parameter symbols to be empty at this stage"
    );

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            parameter_types[i],
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        def.parameter_symbols.push_back(symbol);
    }

    if (def.body.size() == 1 && def.body.front()->template is<Native>())
    {
        def.symbol->mark_as_native();
    }

    visit(def.body);
    return_type_stack.pop_back();

    leave_scope();
}

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    analyze_function(def, ScopeType::FUNCTION, true);
}

void SemanticAnalyzer::visit(PureFunctionDefinition& def)
{
    analyze_function(def, ScopeType::PURE_FUNCTION, false);
}

void SemanticAnalyzer::visit(TemplateDefinition& statement)
{
    enter_scope(ScopeType::TEMPLATE);

    ObjectStringMap generics;

    for (auto& field : statement.members)
    {
        auto constraint_type = visit(field.type);
        auto generic_type_obj = make_object(std::make_shared<GenericType>(constraint_type));

        auto symbol = SymbolFactory::create_generic(field.name, generic_type_obj);
        field.symbol = current_scope->define(symbol);

        generics[field.name] = generic_type_obj;
    }

    std::visit(
        overloaded{
            [&](FunctionDefinition& f)
            {
                analyze_function(f, ScopeType::FUNCTION, true);
            },
            [&](PureFunctionDefinition& f)
            {
                analyze_function(f, ScopeType::PURE_FUNCTION, false);
            },
            [&](ClassDefinition& c)
            {
                auto class_type = initialize_class_type(c);
                analyze_membered_type(
                    c,
                    make_object(std::make_shared<ClassTemplateType>(generics, class_type)),
                    class_type
                );
            },
            [&](TraitDefinition& t)
            {
                auto trait_type = initialize_trait_type(t);
                analyze_membered_type(
                    t,
                    make_object(std::make_shared<TraitTemplateType>(generics, trait_type)),
                    trait_type
                );
            },
            [&](auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid template target");
            }
        },
        statement.target->data
    );

    leave_scope();
}

void SemanticAnalyzer::visit(Native& statement)
{
    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    std::string path = current_module->absolute_filepath.generic_string();

    Doctor::get().assert(
        path.find("/libs/core/") != std::string::npos,
        WaspStage::Semantics,
        "The 'native' keyword is strictly reserved for internal core libraries."
    );
}

void SemanticAnalyzer::visit(AliasDefinition& statement)
{
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
}

void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

} // namespace Wasp
