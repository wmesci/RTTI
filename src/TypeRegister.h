#pragma once
#include "Reflection.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"
#include "Type.h"

namespace
{
const std::string empty_string;
} // namespace

namespace rtti
{
template <class T, class... Args>
inline ObjectPtr ctor(Args... args)
{
    if constexpr (is_object<T>)
        return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
    else
        return Box(T(std::forward<Args>(args)...));
}

template <typename T, bool is_enum = std::is_enum_v<T>, bool is_arithmetic = std::is_arithmetic_v<T>>
struct TypeRegister
{
private:
    TypeRegister() = default;

public:
    // 注册新类型
    static TypeRegister<T> New(const std::string& name = empty_string, const std::map<size_t, std::any>& attributes = {})
    {
        Type* type = type_of<T>();
        if (!name.empty())
            type->m_name = name;
        type->m_attributes = attributes;

        TypeRegister<T> reg;

        return reg;
    }

    template <typename... Args>
    TypeRegister<T>& constructor(const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, Args...>, attributes));
        return *this;
    }

    template <typename U>
    TypeRegister<T>& convert()
    {
        type_of<T>()->m_typeConvertors.push_back({.TargetType = type_of<U>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                                  {
                                                      if constexpr (is_object<T>)
                                                      {
                                                          std::shared_ptr<T> tobj = cast<std::shared_ptr<T>>(obj);
                                                          if constexpr (is_object<U>)
                                                          {
                                                              std::shared_ptr<U> uobj = nullptr;
                                                              if (tobj->template ConvertTo<std::shared_ptr<U>>(uobj))
                                                              {
                                                                  target = uobj;
                                                                  return true;
                                                              }
                                                          }
                                                          else
                                                          {
                                                              U uobj;
                                                              if (tobj->template ConvertTo<U>(uobj))
                                                              {
                                                                  target = rtti::Box(uobj);
                                                                  return true;
                                                              }
                                                          }
                                                      }
                                                      else
                                                      {
                                                          if constexpr (is_object<U>)
                                                          {
                                                              target = static_cast<std::shared_ptr<U>>(rtti::Unbox<T>(obj));
                                                              return true;
                                                          }
                                                          else
                                                          {
                                                              target = rtti::Box(static_cast<U>(rtti::Unbox<T>(obj)));
                                                              return true;
                                                          }
                                                      }
                                                      return false;
                                                  }});
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)(), void (T::*setter)(R), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)(), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)() const, void (T::*setter)(R), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, setter, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R>
    TypeRegister<T>& property(const std::string& name, R (T::*getter)() const, const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(PropertyInfo::Register(name, getter, attributes));
        return *this;
    }

    // field
    template <typename R, bool READONLY = false>
    TypeRegister<T>& property(const std::string& name, R T::*field, const std::map<size_t, std::any>& attributes = {})
    {
        PropertyGetter getter = [field](const ObjectPtr& obj)
        {
            return cast<ObjectPtr>((*getSelf<T>(obj)).*field);
        };

        if constexpr (READONLY || std::is_const_v<std::remove_reference_t<R>>)
        {
            type_of<T>()->m_properties.push_back(PropertyInfo::Register<T, R>(name, getter, PropertySetter(), attributes));
        }
        else
        {
            PropertySetter setter = [field](const ObjectPtr& obj, const ObjectPtr& value)
            {
                (*getSelf<T>(obj)).*field = cast<remove_cr<R>>(value);
            };

            type_of<T>()->m_properties.push_back(PropertyInfo::Register<T, R>(name, getter, setter, attributes));
        }
        return *this;
    }

    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (T::*func)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (T::*func)(Args...) const, const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register(name, func, attributes));
        return *this;
    }

    // static
    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (*func)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register<T>(name, func, attributes));
        return *this;
    }
};

template <typename T>
struct TypeRegister<T, true, false>
{
private:
    TypeRegister() = default;

public:
    // 注册枚举类型
    static TypeRegister<T> New(const std::string& name = empty_string, const std::map<size_t, std::any>& attributes = {})
    {
        Type* type = type_of<T>();
        if (!name.empty())
            type->m_name = name;
        type->m_attributes = attributes;

        TypeRegister<T> reg;
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, const T&>));

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
    // 注册值类型
    static TypeRegister<T> New(const std::string& name = empty_string, const std::map<size_t, std::any>& attributes = {})
    {
        Type* type = type_of<T>();
        if (!name.empty())
            type->m_name = name;
        type->m_attributes = attributes;

        TypeRegister<T> reg;

        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T>));
        type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, const T&>));

        return reg;
    }
};

template <typename T>
inline Type* DefaultEnumRegister(Type* type)
{
    type->m_underlyingType = type_of<typename std::underlying_type<T>::type>();

    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T>));
    // FIXME: 这里会触发无限递归
    // type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, const T&>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int8_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint8_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int16_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint16_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int32_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint32_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int64_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint64_t>));

    type->m_typeConvertors.push_back({.TargetType = type_of<int8_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int8_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<uint8_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint8_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<int16_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int16_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<uint16_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint16_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<int32_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int32_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<uint32_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint32_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<int64_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int64_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({.TargetType = type_of<uint64_t>(), .Convert = [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint64_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});

    return type;
}

template <typename Signature>
Signature* select_overload(Signature* func)
{
    return func;
}

template <typename Signature, typename ClassType>
auto select_overload(Signature(ClassType::*func)) -> decltype(func)
{
    return func;
}
} // namespace rtti