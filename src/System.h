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

template <class T>
T* cast(Object* obj);
} // namespace rtti