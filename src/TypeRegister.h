#pragma once
#include "Reflection.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"
#include "Type.h"

namespace
{
template <class T, class... Args>
rtti::ObjectPtr ctor(Args... args)
{
    if constexpr (rtti::is_object<T>)
        return std::make_shared<T>(std::forward<Args>(args)...);
    else
        return rtti::Box(T(std::forward<Args>(args)...));
}
} // namespace

namespace rtti
{
template <typename T, bool is_enum = std::is_enum_v<T>, bool is_arithmetic = std::is_arithmetic_v<T>>
struct TypeRegister
{
private:
    TypeRegister() = default;

public:
    // 注册新类型
    static TypeRegister<T> New(const std::string& name)
    {
        Type* type = type_of<T>();
        type->m_name = name;

        TypeRegister<T> reg;

        if constexpr (std::is_default_constructible<T>::value)
        {
            reg.template constructor<>();
        }
        if constexpr (std::is_copy_constructible<T>::value)
        {
            reg.template constructor<const T&>();
        }

        return reg;
    }

    template <typename... Args>
    TypeRegister<T>& constructor(const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, Args...>, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)(), void (T::*setter)(R), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)(), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)() const, void (T::*setter)(R), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)() const, const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // field
    template <typename R, bool READONLY = false>
    TypeRegister<T>& property(const std::string& name, R T::*field, const std::map<std::string, std::any>& attributes = {})
    {
        PropertyGetter getter = [field](const ObjectPtr& obj)
        { return Box((*getSelf<T>(obj)).*field); };

        if constexpr (READONLY || std::is_const_v<std::remove_reference_t<R>>)
        {
            type_of<T>()->m_properties.push_back(PropertyInfo::Register<T, R>(name, getter, PropertySetter(), attributes));
        }
        else
        {
            PropertySetter setter = [field](const ObjectPtr& obj, const ObjectPtr& value)
            { (*getSelf<T>(obj)).*field = Unbox<remove_cr<R>>(value); };

            type_of<T>()->m_properties.push_back(PropertyInfo::Register<T, R>(name, getter, setter, attributes));
        }
        return *this;
    }

    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (T::*func)(Args...), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (T::*func)(Args...) const, const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }
};

template <typename T>
struct TypeRegister<T, true, false>
{
private:
    TypeRegister() = default;

public:
    // 注册新类型
    static TypeRegister<T> New(const std::string& name)
    {
        Type* type = type_of<T>();
        type->m_name = name;
        type->m_underlyingType = typeof(typename std::underlying_type<T>::type);

        TypeRegister<T> reg;

        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, const T&>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, int16_t>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, uint16_t>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, int32_t>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, uint32_t>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, int64_t>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, uint64_t>));

        return reg;
    }

    TypeRegister<T>& value(const std::string& name, T v)
    {
        type_of<T>()->m_enumValues.push_back({.number = (int64_t)v, .value = Box(v), .name = name});
        return *this;
    }
};

template <typename T>
struct TypeRegister<T, false, true>
{
private:
    TypeRegister() = default;

public:
    // 注册新类型
    static TypeRegister<T> New(const std::string& name)
    {
        Type* type = type_of<T>();
        type->m_name = name;

        TypeRegister<T> reg;

        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, const T&>));

        return reg;
    }
};
} // namespace rtti
