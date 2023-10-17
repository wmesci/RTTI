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
template <typename T>
struct TypeReg
{
private:
    TypeReg() = default;

public:
    // 注册新类型
    static TypeReg<T> New(const std::string& name)
    {
        Type* type = type_of<T>();
        type->m_name = name;

        TypeReg<T> reg;

        if constexpr (std::is_default_constructible<T>::value)
        {
            reg.constructor<>();
        }
        if constexpr (std::is_copy_constructible<T>::value)
        {
            reg.constructor<const T&>();
        }

        return reg;
    }

    template <typename... Args>
    TypeReg<T>& constructor(const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, Args...>, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeReg<T>& property(const std::string& name, R (T::*getter)(), void (T::*setter)(R), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeReg<T>& property(const std::string& name, R (T::*getter)(), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeReg<T>& property(const std::string& name, R (T::*getter)() const, void (T::*setter)(R), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeReg<T>& property(const std::string& name, R (T::*getter)() const, const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // field
    template <typename R, bool READONLY = false>
    TypeReg<T>& property(const std::string& name, R T::*field, const std::map<std::string, std::any>& attributes = {})
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
    TypeReg<T>& method(const std::string& name, R (T::*func)(Args...), const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args>
    TypeReg<T>& method(const std::string& name, R (T::*func)(Args...) const, const std::map<std::string, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }
};

} // namespace rtti
