#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Config.h"
#include "Error.h"

namespace rtti
{

class Object;
class ObjectBox;
class Interface;
class Type;
class TypeRegistry;
class MethodBase;
class ConstructorInfo;
class MethodInfo;
class PropertyInfo;

template <typename T>
class Boxed;

template <typename T, typename... Args>
inline Ptr<T> MakePtr(Args&&... args)
{
    return detail::pointer_ops<T>::template make<T>(std::forward<Args>(args)...);
}

template <typename To, typename From>
inline auto PtrCast(const Ptr<From>& p)
{
    return detail::pointer_ops<From>::template ptr_cast<To>(p);
}

template <typename T>
inline T* RawFromPtr(const Ptr<T>& p) noexcept
{
    return detail::pointer_ops<T>::to_raw(p);
}

template <typename T>
inline Ptr<T> PtrFromRaw(T* raw)
{
    if (raw == nullptr)
        return Ptr<T>();
    return detail::pointer_ops<T>::from_raw(raw);
}

using ObjectPtr = Ptr<Object>;
using ObjectBoxPtr = Ptr<ObjectBox>;

// ----------------------------------------------------------------------------
// Hashing helpers
// ----------------------------------------------------------------------------

inline constexpr void hash_combine(size_t& seed, size_t hash) noexcept
{
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Args>
inline constexpr size_t hash(const Args&... args)
{
    size_t seed = 0;
    (hash_combine(seed, std::hash<Args>()(args)), ...);
    return seed;
}

inline constexpr size_t hash_string_npos = static_cast<size_t>(-1);

constexpr size_t hash_string(const char* str, size_t size = hash_string_npos) noexcept
{
    if (str == nullptr)
        return 0;

    size_t result = 0;
    if (size == hash_string_npos)
    {
        while (*str != '\0')
        {
            hash_combine(result, static_cast<size_t>(static_cast<unsigned char>(*str)));
            str++;
        }
    }
    else
    {
        for (size_t i = 0; i < size; i++)
            hash_combine(result, static_cast<size_t>(static_cast<unsigned char>(str[i])));
    }
    return result;
}

template <auto V>
static constexpr auto force_consteval = V;

#define RTTI_HASH(str) ::rtti::force_consteval<::rtti::hash_string(str)>

// ----------------------------------------------------------------------------
// Type traits
// ----------------------------------------------------------------------------

template <typename T>
using remove_const_ref = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_object = std::is_base_of<Object, T>::value;

template <typename T>
constexpr bool is_interface = std::is_base_of_v<Interface, remove_const_ref<T>> &&
                              !std::is_base_of_v<Object, remove_const_ref<T>>;

template <typename T>
struct is_ptr : std::false_type
{
};

template <typename T>
struct is_ptr<Ptr<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_ptr_v = is_ptr<remove_const_ref<T>>::value;

template <typename T>
concept ObjectType = is_object<T>;

template <typename T>
concept ValueType = !is_object<T>;

template <class T, class... Args>
inline ObjectPtr ctor(Args... args);

template <class To, class From>
inline auto cast(const From& from, bool* pOK = nullptr);

template <class To, class From>
inline auto cast_or_throw(const From& from);

template <class To, class From>
inline bool try_cast(const From& from, To& out);

template <ValueType T>
    requires(!is_ptr_v<T>)
inline ObjectPtr Box(T value);

template <ValueType T>
inline T Unbox(const ObjectPtr& ptr);

bool IsTypeRegistryFrozen() noexcept;

} // namespace rtti
