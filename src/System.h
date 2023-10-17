#pragma once
#if defined(__clang__)
#define __debugbreak __builtin_debugtrap
#endif

#include <type_traits>
#include <cassert>
#include <string>

using namespace std::string_literals;

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

template <typename T>
using remove_cr = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_object = std::is_base_of<Object, T>::value || std::is_same<Object, T>::value;

// MACRO_FOR_EACH
#define PARENS () // Note space before (), so object-like macro

#define EXPAND(arg) EXPAND1(EXPAND1(EXPAND1(EXPAND1(arg))))
#define EXPAND1(arg) EXPAND2(EXPAND2(EXPAND2(EXPAND2(arg))))
#define EXPAND2(arg) EXPAND3(EXPAND3(EXPAND3(EXPAND3(arg))))
#define EXPAND3(arg) EXPAND4(EXPAND4(EXPAND4(EXPAND4(arg))))
#define EXPAND4(arg) arg

#define MACRO_FOR_EACH(macro, ...) __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

} // namespace rtti