#pragma once

#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include "MethodInfo.h"
#include "Object.h"
#include "ObjectBox.h"
#include "PropertyInfo.h"
#include "Reflection.h"
#include "System.h"
#include "Type.h"
#include "TypeRegister.h"

namespace rtti
{

namespace detail
{

template <typename T>
void RegisterNumericType(std::set<Type*>& registered)
{
    Type* type = type_of<T>();
    if (!registered.insert(type).second)
        return;

    TypeRegistry::Get().SetCanonicalName(type, GetTypeName<T>());
    TypeRegister<T>::New(GetTypeName<T>(), {})
        .template constructor<>()
        .template constructor<T>();
}

template <typename T>
void RegisterNamedNumericType(std::set<Type*>& registered, const char* name)
{
    Type* type = type_of<T>();
    if (!registered.insert(type).second)
        return;

    TypeRegistry::Get().SetCanonicalName(type, name);
    TypeRegister<T>::New(std::string(name), {})
        .template constructor<>()
        .template constructor<T>();
}

} // namespace detail

inline void InitCoreType()
{
    static std::once_flag once;
    std::call_once(once, []
    {
        // Root/value types first.
        TypeRegistry::Get().SetCanonicalName(type_of<Object>(), "Object");
        TypeRegistry::Get().SetCanonicalName(type_of<ObjectBox>(), "ObjectBox");

        std::set<Type*> registered;

        // Fixed-width names are canonical so serialized data is stable across
        // platforms. Additional fundamental types are registered afterwards
        // with their compiler name.
        detail::RegisterNamedNumericType<bool>(registered, "bool");
        detail::RegisterNamedNumericType<int8_t>(registered, "int8_t");
        detail::RegisterNamedNumericType<uint8_t>(registered, "uint8_t");
        detail::RegisterNamedNumericType<int16_t>(registered, "int16_t");
        detail::RegisterNamedNumericType<uint16_t>(registered, "uint16_t");
        detail::RegisterNamedNumericType<int32_t>(registered, "int32_t");
        detail::RegisterNamedNumericType<uint32_t>(registered, "uint32_t");
        detail::RegisterNamedNumericType<int64_t>(registered, "int64_t");
        detail::RegisterNamedNumericType<uint64_t>(registered, "uint64_t");
        detail::RegisterNamedNumericType<float>(registered, "float");
        detail::RegisterNamedNumericType<double>(registered, "double");

        detail::RegisterNumericType<char>(registered);
        detail::RegisterNumericType<wchar_t>(registered);
        detail::RegisterNumericType<char8_t>(registered);
        detail::RegisterNumericType<char16_t>(registered);
        detail::RegisterNumericType<char32_t>(registered);
        detail::RegisterNumericType<long>(registered);
        detail::RegisterNumericType<unsigned long>(registered);
        detail::RegisterNumericType<long long>(registered);
        detail::RegisterNumericType<unsigned long long>(registered);
        detail::RegisterNumericType<long double>(registered);

        TypeRegistry::Get().SetCanonicalName(type_of<std::string>(), "string");
        TypeRegister<std::string>::New("string")
            .template constructor<>()
            .constructor<const std::string&>()
            .compare<std::string>();

        TypeRegistry::Get().SetCanonicalName(type_of<void*>(), "void*");
        TypeRegister<void*>::New("void*").template constructor<>().constructor<void*>().compare<void*>();
    });
}

} // namespace rtti
