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

#define RTTI_LOG(x, msg) printf("[" #x "]: %s\n\t\t in %s[%s:%d]\n", (msg), __PRETTY_FUNCTION__, __FILE__, __LINE__)

#if !defined(RTTI_DEBUG)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_DEBUG(msg) RTTI_LOG(DEBUG, msg)
#else
#define RTTI_DEBUG(msg)
#endif
#endif

#if !defined(RTTI_WARNING)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_WARNING(msg) RTTI_LOG(WARNING, msg)
#else
#define RTTI_WARNING(msg)
#endif
#endif

#if !defined(RTTI_ERROR)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_ERROR(msg) RTTI_LOG(ERROR, msg)
#else
#define RTTI_ERROR(msg)
#endif
#endif

namespace rtti
{
template <typename T>
using Ptr = std::shared_ptr<T>;

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
    hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash;
}

template <typename... Args>
inline constexpr size_t hash(const Args&... args)
{
    size_t seed = 0;
    (hash_combine(seed, std::hash<Args>()(args)), ...);
    return seed;
}

constexpr size_t HashString(const char* str, size_t seed = 0)
{
    return 0 == *str ? seed : HashString(str + 1, seed ^ (*str + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
}

template <auto V>
static constexpr auto force_consteval = V;

#define HASH(str) rtti::force_consteval<rtti::HashString(str)>

template <typename T>
using remove_cr = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_object = std::is_base_of<Object, T>::value || std::is_same<Object, T>::value;

template <typename T>
concept ObjectType = is_object<T>;

template <typename T>
concept ValueType = !is_object<T>;

template <class T, class... Args>
inline ObjectPtr ctor(Args... args);

template <class To, class From>
inline auto cast(const From& from, bool* pOK = nullptr);
} // namespace rtti