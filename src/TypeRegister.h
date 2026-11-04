#pragma once

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "MethodInfo.h"
#include "PropertyInfo.h"
#include "Reflection.h"
#include "System.h"
#include "Type.h"

namespace rtti
{

namespace detail
{

template <typename T, typename U>
concept HasConvertTo = requires(T& value, U& target) {
    { value.template ConvertTo<U>(target) } -> std::convertible_to<bool>;
};

template <typename T, typename U>
concept HasValueConverter = requires(const T& value, U& target) {
    { value.template ConvertTo<U>(target) } -> std::convertible_to<bool>;
};

template <typename T, typename U, typename F>
concept HasCallbackConverter = requires(F& function, const T& value, U& target) {
    { function(value, target) } -> std::convertible_to<bool>;
};

template <typename T, typename U, typename F>
concept HasObjectCallbackConverter = requires(F& function, const Ptr<T>& value, U& target) {
    { function(value, target) } -> std::convertible_to<bool>;
};

template <typename T, typename U>
CompareResult Comparer(const ObjectPtr& left, const ObjectPtr& right)
{
    if constexpr (is_object<T>)
    {
        bool leftOk = false;
        bool rightOk = false;
        Ptr<T> leftValue = cast<T>(left, &leftOk);
        Ptr<U> rightValue = cast<U>(right, &rightOk);
        if (!leftOk || !rightOk || !leftValue || !rightValue)
            return CompareResult::Failed;
        return RawFromPtr(leftValue) == RawFromPtr(rightValue) ? CompareResult::Equals : CompareResult::NotEquals;
    }
    else
    {
        bool leftOk = false;
        bool rightOk = false;
        T leftValue = cast<T>(left, &leftOk);
        U rightValue = cast<U>(right, &rightOk);
        if (!leftOk || !rightOk)
            return CompareResult::Failed;
        if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
        {
            const long double a = static_cast<long double>(leftValue);
            const long double b = static_cast<long double>(rightValue);
            return a == b ? CompareResult::Equals : CompareResult::NotEquals;
        }
        else
        {
            return static_cast<U>(leftValue) == rightValue ? CompareResult::Equals : CompareResult::NotEquals;
        }
    }
}

template <typename Setter>
struct SetterArgument;

template <typename Class, typename Arg>
struct SetterArgument<void (Class::*)(Arg)>
{
    using type = Arg;
};

template <typename Class, typename Arg>
struct SetterArgument<void (Class::*)(Arg) const>
{
    using type = Arg;
};

template <typename Class, typename Arg>
struct SetterArgument<void (Class::*)(Arg) noexcept>
{
    using type = Arg;
};

template <typename Class, typename Arg>
struct SetterArgument<void (Class::*)(Arg) const noexcept>
{
    using type = Arg;
};

template <typename Setter>
using SetterArgumentT = typename SetterArgument<Setter>::type;

} // namespace detail

template <class T, class... Args>
inline rtti::ObjectPtr ctor(Args... args)
{
    if constexpr (rtti::is_object<T>)
        return ::rtti::MakePtr<T>(std::forward<Args>(args)...);
    else
        return rtti::Box(T(std::forward<Args>(args)...));
}

template <typename T, bool is_enum = std::is_enum_v<T>>
struct TypeRegister
{
private:
    TypeRegister() = default;

    static void EnsureRegistrationOpen()
    {
        if (TypeRegistry::Get().IsFrozen())
            ThrowError(ErrorCode::RegistryFrozen, "cannot register metadata after TypeRegistry::Freeze()");
    }

public:
    static TypeRegister<T> New(const std::map<size_t, std::any>& attributes = {})
    {
        EnsureRegistrationOpen();
        Type* type = type_of<T>();
        type->SetAttributes(attributes);
        return TypeRegister<T>{};
    }

    template <std::size_t N>
    static TypeRegister<T> New(const char (&name)[N], const std::map<size_t, std::any>& attributes = {})
    {
        return New(std::string(name, N > 0 ? N - 1 : 0), attributes);
    }

    static TypeRegister<T> New(const std::string& name, const std::map<size_t, std::any>& attributes)
    {
        EnsureRegistrationOpen();
        Type* type = type_of<T>();
        TypeRegistry::Get().RegisterAlias(name, type, true);
        type->SetAttributes(attributes);
        return TypeRegister<T>{};
    }

    template <typename... Args>
    TypeRegister<T>& constructor(const std::map<size_t, std::any>& attributes = {})
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddConstructor(ConstructorInfo::Register<T>(&ctor<T, Args...>, attributes));
        return *this;
    }

    template <typename U>
    TypeRegister<T>& convert()
    {
        EnsureRegistrationOpen();
        Type* target = type_of<U>();
        using Target = type_t<U>;

        if constexpr (is_object<T>)
        {
            static_assert(detail::HasConvertTo<T, Target>,
                          "object conversion requires a member template: "
                          "template <typename U> bool ConvertTo(U& target); "
                          "Alternatively use Convert<U>(callback).");
            type_of<T>()->AddConverter(target, [](const ObjectPtr& object, ObjectPtr& result) -> bool
            {
                bool ok = false;
                Ptr<T> value = cast<T>(object, &ok);
                if (!ok || !value)
                    return false;
                Target converted{};
                if (!value->template ConvertTo<Target>(converted))
                    return false;
                if constexpr (is_object<Target>)
                    result = converted;
                else
                    result = Box(converted);
                return true;
            });
        }
        else
        {
            static_assert(std::is_convertible_v<T, Target> || std::is_arithmetic_v<T>,
                          "value conversion requires T to be convertible to the target type");
            type_of<T>()->AddConverter(target, [](const ObjectPtr& object, ObjectPtr& result) -> bool
            {
                if (!object || object->GetRttiType() != type_of<T>())
                    return false;
                result = Box(static_cast<Target>(Unbox<T>(object)));
                return true;
            });
        }
        return *this;
    }

    template <typename U, typename F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, std::map<size_t, std::any>>)
    TypeRegister<T>& Convert(F&& converter)
    {
        EnsureRegistrationOpen();
        using Target = type_t<U>;
        Type* target = type_of<U>();
        using Function = std::remove_cvref_t<F>;

        if constexpr (is_object<T>)
        {
            static_assert(detail::HasObjectCallbackConverter<T, Target, Function> ||
                              detail::HasCallbackConverter<T, Target, Function>,
                          "callback must be bool(const Ptr<T>&, U&) or bool(const T&, U&)");
            type_of<T>()->AddConverter(target, [function = Function(std::forward<F>(converter))](
                                                          const ObjectPtr& object, ObjectPtr& result) -> bool
            {
                bool ok = false;
                Ptr<T> value = cast<T>(object, &ok);
                if (!ok || !value)
                    return false;
                Target converted{};
                if constexpr (detail::HasObjectCallbackConverter<T, Target, Function>)
                {
                    if (!function(value, converted))
                        return false;
                }
                else
                {
                    if (!function(*RawFromPtr(value), converted))
                        return false;
                }
                if constexpr (is_object<Target>)
                    result = converted;
                else
                    result = Box(converted);
                return true;
            });
        }
        else
        {
            static_assert(detail::HasCallbackConverter<T, Target, Function>,
                          "callback must be bool(const T&, U&)");
            type_of<T>()->AddConverter(target, [function = Function(std::forward<F>(converter))](
                                                          const ObjectPtr& object, ObjectPtr& result) -> bool
            {
                if (!object || object->GetRttiType() != type_of<T>())
                    return false;
                Target converted{};
                if (!function(Unbox<const T&>(object), converted))
                    return false;
                if constexpr (is_object<Target>)
                    result = converted;
                else
                    result = Box(converted);
                return true;
            });
        }
        return *this;
    }

    template <typename U>
    TypeRegister<T>& compare()
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddComparer(type_of<U>(), &detail::Comparer<T, U>);
        return *this;
    }

    // Property: getter/setter member functions.
    //
    // The setter parameter is deduced from the getter's return type so that
    // overloaded setters (e.g. SetRotation(quat) / SetRotation(vec3)) can be
    // resolved using the getter signature.
    template <typename R, typename Getter, typename Setter>
    TypeRegister<T>& AddReadWriteProperty(const std::string& name,
                                          Getter getter,
                                          Setter setter,
                                          const std::map<size_t, std::any>& attributes)
    {
        EnsureRegistrationOpen();
        auto getterMethod = MethodInfo::Register<T, Getter, R>(name, getter);
        auto setterMethod = MethodInfo::Register<T, Setter, void, detail::SetterArgumentT<Setter>>(name, setter);
        type_of<T>()->AddProperty(
            std::unique_ptr<PropertyInfo>(new PropertyInfo(type_of<T>(), name, type_of<R>(), std::move(getterMethod),
                                                           std::move(setterMethod), attributes)));
        return *this;
    }

    template <typename R, typename Getter>
    TypeRegister<T>& AddReadOnlyProperty(const std::string& name,
                                         Getter getter,
                                         const std::map<size_t, std::any>& attributes)
    {
        EnsureRegistrationOpen();
        auto getterMethod = MethodInfo::Register<T, Getter, R>(name, getter);
        type_of<T>()->AddProperty(std::unique_ptr<PropertyInfo>(
            new PropertyInfo(type_of<T>(), name, type_of<R>(), std::move(getterMethod), nullptr, attributes)));
        return *this;
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)(),
                              void (U::*setter)(R),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)() const,
                              void (U::*setter)(R),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>) && (!std::is_reference_v<R>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)(),
                              void (U::*setter)(const R&),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>) && (!std::is_reference_v<R>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)() const,
                              void (U::*setter)(const R&),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>) && (!std::is_reference_v<R>) && (!std::is_const_v<R>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)(),
                              void (U::*setter)(R&),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>) && (!std::is_reference_v<R>) && (!std::is_const_v<R>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)() const,
                              void (U::*setter)(R&),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadWriteProperty<R>(name, getter, setter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)(),
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadOnlyProperty<R>(name, getter, attributes);
    }

    template <typename R, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& property(const std::string& name,
                              R (U::*getter)() const,
                              const std::map<size_t, std::any>& attributes = {})
    {
        return AddReadOnlyProperty<R>(name, getter, attributes);
    }

    template <typename R, bool READONLY = false, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>) && (!std::is_function_v<R>)
    TypeRegister<T>& property(const std::string& name,
                              R U::*field,
                              const std::map<size_t, std::any>& attributes = {})
    {
        auto getterMethod = MethodInfo::Register<T, decltype(field), R>(name, field);
        if constexpr (READONLY || std::is_const_v<std::remove_reference_t<R>>)
        {
            type_of<T>()->AddProperty(std::unique_ptr<PropertyInfo>(new PropertyInfo(
                type_of<T>(), name, type_of<R>(), std::move(getterMethod), nullptr, attributes)));
        }
        else
        {
            auto setterMethod = MethodInfo::Register<T, decltype(field), void, R>(name, field);
            type_of<T>()->AddProperty(std::unique_ptr<PropertyInfo>(new PropertyInfo(
                type_of<T>(), name, type_of<R>(), std::move(getterMethod), std::move(setterMethod), attributes)));
        }
        return *this;
    }

    template <typename R, typename... Args, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& method(const std::string& name,
                            R (U::*func)(Args...),
                            const std::map<size_t, std::any>& attributes = {})
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddMethod(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args, typename U = std::remove_pointer_t<T>>
        requires(!std::is_arithmetic_v<U>)
    TypeRegister<T>& method(const std::string& name,
                            R (U::*func)(Args...) const,
                            const std::map<size_t, std::any>& attributes = {})
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddMethod(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }

    template <typename R, typename... Args>
    TypeRegister<T>& method(const std::string& name,
                            R (*func)(Args...),
                            const std::map<size_t, std::any>& attributes = {})
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddMethod(MethodInfo::Register<T, decltype(func), R, Args...>(name, func, attributes));
        return *this;
    }
};

template <typename T>
struct TypeRegister<T, true>
{
private:
    TypeRegister() = default;

    static void EnsureRegistrationOpen()
    {
        if (TypeRegistry::Get().IsFrozen())
            ThrowError(ErrorCode::RegistryFrozen, "cannot register metadata after TypeRegistry::Freeze()");
    }

public:
    static TypeRegister<T> New(const std::map<size_t, std::any>& attributes = {})
    {
        if (TypeRegistry::Get().IsFrozen())
            ThrowError(ErrorCode::RegistryFrozen, "cannot register metadata after TypeRegistry::Freeze()");
        Type* type = type_of<T>();
        type->SetAttributes(attributes);
        return TypeRegister<T>{};
    }

    template <std::size_t N>
    static TypeRegister<T> New(const char (&name)[N], const std::map<size_t, std::any>& attributes = {})
    {
        return New(std::string(name, N > 0 ? N - 1 : 0), attributes);
    }

    static TypeRegister<T> New(const std::string& name, const std::map<size_t, std::any>& attributes)
    {
        EnsureRegistrationOpen();
        Type* type = type_of<T>();
        TypeRegistry::Get().RegisterAlias(name, type, true);
        type->SetAttributes(attributes);
        return TypeRegister<T>{};
    }

    TypeRegister<T>& value(const std::string& name, T value)
    {
        if (TypeRegistry::Get().IsFrozen())
            ThrowError(ErrorCode::RegistryFrozen, "cannot register metadata after TypeRegistry::Freeze()");
        type_of<T>()->AddEnumValue(name, Box(value));
        return *this;
    }

    template <typename U>
    TypeRegister<T>& compare()
    {
        EnsureRegistrationOpen();
        type_of<T>()->AddComparer(type_of<U>(), &detail::Comparer<T, U>);
        return *this;
    }
};

template <typename T>
inline Type* DefaultEnumRegister(Type* type)
{
    if (!type->GetConstructors().empty())
        return type;

    using UT = std::underlying_type_t<T>;

    type->AddConstructor(ConstructorInfo::Register(type, &ctor<T>));
    type->AddConstructor(ConstructorInfo::Register(type, &ctor<T, const T&>));
    type->AddConstructor(ConstructorInfo::Register(type, &ctor<T, UT>));

    type->AddConverter(type_of<UT>(), [](const ObjectPtr& object, ObjectPtr& result) -> bool
                       {
                           if (!object || object->GetRttiType() != type_of<T>())
                               return false;
                           result = Box(static_cast<UT>(Unbox<T>(object)));
                           return true;
                       });

    type->AddComparer(type_of<T>(), &detail::Comparer<T, T>);
    return type;
}

template <typename Signature>
Signature select_overload(Signature function)
{
    return function;
}

template <typename Signature, typename ClassType>
    requires std::is_function_v<Signature>
auto select_overload(Signature (ClassType::*function)) -> decltype(function)
{
    return function;
}

} // namespace rtti
