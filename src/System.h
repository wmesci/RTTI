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
class Object;
using ObjectPtr = std::shared_ptr<Object>;

class ObjectBox;
using ObjectBoxPtr = std::shared_ptr<ObjectBox>;

template <typename T>
class Boxed;

class Type;
class MethodBase;
class ConstructorInfo;
class MethodInfo;
class PropertyInfo;

constexpr size_t Hash(const char* str, size_t seed = 0)
{
    return 0 == *str ? seed : Hash(str + 1, seed ^ (*str + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
}

template <auto V>
static constexpr auto force_consteval = V;

#define HASH(str) rtti::force_consteval<rtti::Hash(str)>

template <typename T>
using remove_cr = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_object = std::is_base_of<Object, T>::value || std::is_same<Object, T>::value;

template <class T, class... Args>
inline ObjectPtr ctor(Args... args);

template <typename T>
inline Type* type_of();

template <class To, class From>
inline auto cast(const From& from, bool* pOK = nullptr);
} // namespace rtti