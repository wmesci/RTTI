#pragma once
#if defined(__clang__)
#define __debugbreak __builtin_debugtrap
#elif defined(_MSC_VER)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#include <type_traits>
#include <cassert>
#include <string>
#include <memory>
#include <functional>

#include "Config.h"

namespace rtti
{
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
    return detail::pointer_ops<T>::from_raw(raw);
}

class Object;
using ObjectPtr = Ptr<Object>;

class ObjectBox;
using ObjectBoxPtr = Ptr<ObjectBox>;

template <typename T>
class Boxed;

class Type;
class MethodBase;
class ConstructorInfo;
class MethodInfo;
class PropertyInfo;

inline constexpr void hash_combine(size_t& seed, size_t hash)
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

constexpr size_t hash_string(const char* str, size_t size = 0)
{
    if (!str) return 0;

    size_t hash = 0;
    if (size == 0) {
        while (*str != '\0')
        {
            hash_combine(hash, static_cast<size_t>(static_cast<unsigned char>(*str)));
            str++;
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            hash_combine(hash, static_cast<size_t>(static_cast<unsigned char>(str[i])));
        }
    }
    return hash;
}

template <auto V>
static constexpr auto force_consteval = V;

#define HASH(str) rtti::force_consteval<rtti::hash_string(str)>

template <typename T>
using remove_cr = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_object = std::is_base_of<Object, T>::value;  // is_base_of<T,T> is true

template <typename T>
concept ObjectType = is_object<T>;

template <typename T>
concept ValueType = !is_object<T>;

template <class T, class... Args>
inline ObjectPtr ctor(Args... args);

template <class To, class From>
inline auto cast(const From& from, bool* pOK = nullptr);
} // namespace rtti