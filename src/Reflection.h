#pragma once
#include "nameof.hpp"
#include "System.h"

namespace rtti
{
template <typename T>
struct inner_type
{
    using type = T;
};

template <typename T>
struct inner_type<Ptr<T>>
{
    using type = T;
};

// Object Ptr<Object>
// type: Object
template <typename T, bool isobject = is_object<typename inner_type<remove_cr<T>>::type>>
struct TypeWarper
{
    using type = typename inner_type<remove_cr<T>>::type;
    using warper = type;
    using vartype = Ptr<type>;
    using base = type::BASE_TYPE;
};

template <>
struct TypeWarper<void, false>
{
    using type = void;
    using warper = void;
    using vartype = void;
    using base = void;
};

// ValueType Ptr<ValueType>
// type: ...
template <typename T>
struct TypeWarper<T, false>
{
    using type = remove_cr<T>;
    using warper = Boxed<T>;
    using vartype = type;
    using base = ObjectBox;
};

template <typename T>
using type_t = TypeWarper<T>::type;

template <typename T>
using warper_t = TypeWarper<T>::warper;

template <typename T>
using vartype_t = TypeWarper<T>::vartype;

template <typename T>
using base_type_t = TypeWarper<T>::base;

template <typename T>
Type* type_of();

struct ParameterInfo
{
    Type* ParameterType;
    bool IsRef;
    bool IsConst;
};

enum class TypeFlags
{
    None = 0,
    Pointer = 1,
    Enum = 2,
    Integral = 4,
    Floating = 8,
};

extern Type* NewType(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* base);

template <typename T>
rtti::Type* DefaultEnumRegister(rtti::Type* type);

template <typename CLS>
inline std::string GetTypeName()
{
    if constexpr (std::is_pointer_v<CLS>)
        return std::string(NAMEOF_SHORT_TYPE(std::remove_pointer_t<CLS>)) + "*";
    else
        return std::string(NAMEOF_SHORT_TYPE(CLS));
}

template <typename CLS, typename BASE>
inline Type* CreateType()
{
    if constexpr (std::is_same_v<CLS, Object>)
    {
        static_assert(std::is_same_v<BASE, void>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::None, nullptr, nullptr);
        return type;
    }
    else if constexpr (std::is_pointer_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::Pointer, type_of<typename std::remove_pointer_t<CLS>>(), type_of<BASE>());
        return type;
    }
    else if constexpr (std::is_enum_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::Enum, type_of<typename std::underlying_type_t<CLS>>(), type_of<BASE>());
        DefaultEnumRegister<CLS>(type);
        return type;
    }
    else if constexpr (std::is_integral_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::Integral, nullptr, type_of<BASE>());
        return type;
    }
    else if constexpr (std::is_floating_point_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::Floating, nullptr, type_of<BASE>());
        return type;
    }
    else
    {
        static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
        static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::None, nullptr, type_of<BASE>());
        return type;
    }
}

template <typename T>
inline Type* type_of()
{
    if constexpr (std::is_void_v<T>)
    {
        return nullptr;
    }
    else
    {
        using U = type_t<T>;

        if constexpr (is_object<U>)
            return CreateType<U, typename U::BASE_TYPE>();
        else
            return CreateType<U, ObjectBox>();
    }
}

#define TYPE_DECLARE(...)                                                                           \
public:                                                                                             \
    using BASE_TYPE = __VA_ARGS__;                                                                  \
    static_assert(std::is_base_of_v<rtti::Object, BASE_TYPE> || std::is_same_v<BASE_TYPE, Object>); \
    virtual rtti::Type* GetRttiType() const override                                                \
    {                                                                                               \
        using THIS_TYPE = rtti::remove_cr<decltype(*this)>;                                         \
        static_assert(std::is_base_of_v<BASE_TYPE, THIS_TYPE>);                                     \
        return rtti::type_of<THIS_TYPE>();                                                          \
    }

} // namespace rtti