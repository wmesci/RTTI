#pragma once
#include "nameof.hpp"
#include "System.h"

namespace rtti
{
struct EnumInfo
{
    int64_t number;
    ObjectPtr value;
    std::string name;
};

struct ParameterInfo
{
    Type* ParameterType;
    bool IsRef;
    bool IsConst;
};

enum class TypeFlags
{
    None = 0,
    Enum = 1,
    Pointer = 2
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

template <typename CLS>
Type* CreateType()
{
    static_assert(std::is_same_v<Object, CLS>);
    static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::None, nullptr, nullptr);
    return type;
}

template <typename CLS, typename BASE>
std::enable_if_t<!std::is_enum_v<CLS> && !std::is_pointer_v<CLS>, Type*> CreateType()
{
    static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
    static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::None, nullptr, type_of<BASE>());
    return type;
}

template <typename CLS, typename BASE>
std::enable_if_t<std::is_pointer_v<CLS>, Type*> CreateType()
{
    static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
    static Type* type = NewType(GetTypeName<CLS>(), sizeof(CLS), TypeFlags::Pointer, type_of<typename std::remove_pointer_t<CLS>>(), type_of<BASE>());
    return type;
}

template <typename CLS, typename BASE>
std::enable_if_t<std::is_enum_v<CLS>, Type*> CreateType()
{
    static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
    static Type* type = DefaultEnumRegister<CLS>(NewType(std::string(GetTypeName<CLS>()), sizeof(CLS), TypeFlags::Enum, type_of<typename std::underlying_type_t<CLS>>(), type_of<BASE>()));
    return type;
}

template <typename T>
struct InnerType
{
    using type = T;
};

template <typename T>
struct InnerType<std::shared_ptr<T>>
{
    using type = T;
};

template <typename T>
struct InnerType<std::weak_ptr<T>>
{
    using type = T;
};

// Object Ptr<Object>
// type: Object
// objtype: Object
template <typename T, bool isobject = is_object<typename InnerType<remove_cr<T>>::type>>
struct TypeWarper
{
    using type = typename InnerType<remove_cr<T>>::type;
    using objtype = type;
    static Type* ClassType()
    {
        if constexpr (std::is_same_v<objtype, void>)
            return nullptr;
        else if constexpr (std::is_same_v<objtype, Object>)
            return CreateType<Object>();
        else
            return CreateType<type, typename objtype::RTTI_BASE_CLASS>();
    }
};

template <>
struct TypeWarper<void, false>
{
    static Type* ClassType() { return nullptr; }
};

// ValueType Ptr<ValueType>
// type: ...
// objtype: Boxed<...>
template <typename T>
struct TypeWarper<T, false>
{
    using type = remove_cr<T>;
    using objtype = Boxed<type>;
    static Type* ClassType() { return CreateType<type, typename objtype::RTTI_BASE_CLASS>(); }
};

template <typename T>
inline Type* type_of()
{
    return TypeWarper<rtti::remove_cr<T>>::ClassType();
}

#define TYPE_DECLARE(base)                                                 \
public:                                                                    \
    using RTTI_BASE_CLASS = base;                                          \
    virtual rtti::Type* GetRttiType() const override                       \
    {                                                                      \
        return rtti::CreateType<rtti::remove_cr<decltype(*this)>, base>(); \
    }                                                                      \
    template <class T, class... Args>                                      \
    friend rtti::ObjectPtr rtti::ctor(Args... args);
} // namespace rtti