#pragma once
#include "Reflection.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"
#include "Type.h"

namespace
{
const std::string empty_string;

template <class T, class U>
inline rtti::CompareResult comparer(const rtti::ObjectPtr& left, const rtti::ObjectPtr& right)
{
    bool ok = false;
    auto t = rtti::cast<T>(left, &ok);
    if (!ok)
        return rtti::CompareResult::Failed;

    auto u = rtti::cast<U>(right, &ok);
    if (!ok)
        return rtti::CompareResult::Failed;

    return static_cast<U>(t) == u ? rtti::CompareResult::Equals : rtti::CompareResult::NotEquals;
}
} // namespace

namespace rtti
{
template <class T, class... Args>
inline rtti::ObjectPtr ctor(Args... args)
{
    if constexpr (rtti::is_object<T>)
        return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
    else
        return rtti::Box(T(std::forward<Args>(args)...));
}

template <typename T, bool is_enum = std::is_enum_v<T>>
struct TypeRegister
{
private:
    TypeRegister() = default;

public:
    static TypeRegister<T> New(const std::string& name = empty_string, const std::map<size_t, std::any>& attributes = {})
    {
        Type* type = type_of<T>();
        if (!name.empty())
            type->m_name = name;
        type->m_attributes = attributes;

        TypeRegister<T> reg;

        // if constexpr(!is_object<T>)
        //{
        //     if constexpr(std::)
        //     type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T>));
        //     type->m_constructors.push_back(ConstructorInfo::Register<T>(&ctor<T, const T&>));
        // }

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
        type_of<T>()->m_typeConvertors.push_back({type_of<U>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                            {
                                using TU = typename rtti::TypeWarper<U>::type;
                                if constexpr (is_object<T>)
                                {
                                    std::shared_ptr<T> tobj = cast<std::shared_ptr<T>>(obj);
                                    if constexpr (is_object<TU>)
                                    {
                                        std::shared_ptr<TU> uobj = nullptr;
                                        if (tobj->template ConvertTo<std::shared_ptr<TU>>(uobj))
                                        {
                                            target = uobj;
                                            return true;
                                        }
                                    }
                                    else
                                    {
                                        TU uobj;
                                        if (tobj->template ConvertTo<TU>(uobj))
                                        {
                                            target = rtti::Box(uobj);
                                            return true;
                                        }
                                    }
                                }
                                else
                                {
                                    if constexpr (is_object<TU>)
                                    {
                                        target = static_cast<std::shared_ptr<TU>>(rtti::Unbox<T>(obj));
                                        return true;
                                    }
                                    else
                                    {
                                        target = rtti::Box(static_cast<TU>(rtti::Unbox<T>(obj)));
                                        return true;
                                    }
                                }
                                return false;
                            }});
        return *this;
    }

    template <typename U>
    TypeRegister<T>& compare()
    {
        type_of<T>()->m_objectComparers.push_back({type_of<U>(), &comparer<T, U>});
        return *this;
    }

    // R / R& / const R&
    template <typename R, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& property(const std::string& name, R (U::*getter)(), void (U::*setter)(R), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), MethodInfo::Register<T, decltype(getter), R>(name, getter), MethodInfo::Register<T, decltype(setter), void, R>(name, setter), attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& property(const std::string& name, R (U::*getter)(), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), MethodInfo::Register<T, decltype(getter), R>(name, getter), nullptr, attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& property(const std::string& name, R (U::*getter)() const, void (U::*setter)(R), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), MethodInfo::Register<T, decltype(getter), R>(name, getter), MethodInfo::Register<T, decltype(setter), void, R>(name, setter), attributes));
        return *this;
    }

    // R / R& / const R&
    template <typename R, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& property(const std::string& name, R (U::*getter)() const, const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), MethodInfo::Register<T, decltype(getter), R>(name, getter), nullptr, attributes));
        return *this;
    }

    // field
    template <typename R, bool READONLY = false, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& property(const std::string& name, R U::*field, const std::map<size_t, std::any>& attributes = {})
    {
        MethodInfo* getter = MethodInfo::Register<T, decltype(field), R>(name, field);
        if constexpr (READONLY || std::is_const_v<std::remove_reference_t<R>>)
        {
            type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), getter, nullptr, attributes));
        }
        else
        {
            type_of<T>()->m_properties.push_back(new PropertyInfo(type_of<T>(), name, type_of<R>(), getter, MethodInfo::Register<T, decltype(field), void, R>(name, field), attributes));
        }
        return *this;
    }

    template <typename R, typename... Args, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& method(const std::string& name, R (U::*func)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args, typename U = std::remove_pointer_t<T>, std::enable_if_t<!std::is_arithmetic_v<U>, bool> = true>
    TypeRegister<T>& method(const std::string& name, R (U::*func)(Args...) const, const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }

    // static
    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name, R (*func)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        type_of<T>()->m_methods.push_back(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }
};

template <typename T>
struct TypeRegister<T, true>
{
private:
    TypeRegister() = default;

public:
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
        type_of<T>()->m_enumValues.push_back({(int64_t)v, Box(v), name});
        return *this;
    }

    template <typename U>
    TypeRegister<T>& compare()
    {
        type_of<T>()->m_objectComparers.push_back({type_of<U>(), &comparer<T, U>});
        return *this;
    }
};

template <typename T>
inline Type* DefaultEnumRegister(Type* type)
{
    type->m_underlyingType = type_of<typename std::underlying_type<T>::type>();

    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T>));
    // FIXME: This will trigger infinite recursion here.
    // type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, const T&>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int8_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint8_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int16_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint16_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int32_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint32_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, int64_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, uint64_t>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, long>));
    type->m_constructors.push_back(ConstructorInfo::Register(type, &ctor<T, unsigned long>));

    type->m_typeConvertors.push_back({type_of<int8_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int8_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<uint8_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint8_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<int16_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int16_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<uint16_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint16_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<int32_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int32_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<uint32_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint32_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<int64_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<int64_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<uint64_t>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<uint64_t>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<long>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<long>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});
    type->m_typeConvertors.push_back({type_of<unsigned long>(), [](const ObjectPtr& obj, Type* targetType, ObjectPtr& target) -> bool
                                      {
                                          target = rtti::Box(static_cast<unsigned long>(rtti::Unbox<T>(obj)));
                                          return true;
                                      }});

    type->m_objectComparers.push_back({type_of<int8_t>(), &comparer<typename std::underlying_type<T>::type, int8_t>});
    type->m_objectComparers.push_back({type_of<uint8_t>(), &comparer<typename std::underlying_type<T>::type, uint8_t>});
    type->m_objectComparers.push_back({type_of<int16_t>(), &comparer<typename std::underlying_type<T>::type, int16_t>});
    type->m_objectComparers.push_back({type_of<uint16_t>(), &comparer<typename std::underlying_type<T>::type, uint16_t>});
    type->m_objectComparers.push_back({type_of<int32_t>(), &comparer<typename std::underlying_type<T>::type, int32_t>});
    type->m_objectComparers.push_back({type_of<uint32_t>(), &comparer<typename std::underlying_type<T>::type, uint32_t>});
    type->m_objectComparers.push_back({type_of<int64_t>(), &comparer<typename std::underlying_type<T>::type, int64_t>});
    type->m_objectComparers.push_back({type_of<uint64_t>(), &comparer<typename std::underlying_type<T>::type, uint64_t>});
    type->m_objectComparers.push_back({type_of<long>(), &comparer<typename std::underlying_type<T>::type, long>});
    type->m_objectComparers.push_back({type_of<unsigned long>(), &comparer<typename std::underlying_type<T>::type, unsigned long>});

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