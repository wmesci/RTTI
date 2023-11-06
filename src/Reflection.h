#pragma once
#include "nameof.hpp"
#include "System.h"

namespace rtti
{
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
    static Type* ClassType() { return objtype::ClassType(); }
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
    static Type* ClassType() { return objtype::ClassType(); }
};

template <typename T>
inline Type* type_of()
{
    return TypeWarper<remove_cr<T>>::ClassType();
}

struct EnumInfo
{
    int64_t number;
    ObjectPtr value;
    std::string name;
};

struct ParameterInfo
{
    Type* Type;
    bool IsRef;
    bool IsConst;
};

extern Type* NewType(const std::string& name, size_t size, Type* base);

template <typename T>
rtti::Type* DefaultEnumRegister(rtti::Type* type);

template <typename CLS>
constexpr std::string_view GetTypeName()
{
    if constexpr (std::is_pointer_v<CLS>)
        return "[ptr]";
    else
        return NAMEOF_SHORT_TYPE(CLS);
}

template <typename CLS>
Type* CreateType()
{
    static_assert(std::is_same_v<Object, CLS> || (!std::is_base_of_v<Object, CLS> && !std::is_pointer_v<CLS>));
    static Type* type = NewType(std::string(GetTypeName<CLS>()), sizeof(CLS), nullptr);
    return type;
}

template <typename CLS, typename BASE>
std::enable_if_t<!std::is_enum_v<CLS>, Type*> CreateType()
{
    static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
    static Type* type = NewType(std::string(GetTypeName<CLS>()), sizeof(CLS), type_of<BASE>());
    return type;
}

template <typename CLS, typename BASE>
std::enable_if_t<std::is_enum_v<CLS>, Type*> CreateType()
{
    static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox>);
    static Type* type = DefaultEnumRegister<CLS>(NewType(std::string(GetTypeName<CLS>()), sizeof(CLS), type_of<BASE>()));
    return type;
}

#define TYPE_DECLARE(cls, base)                      \
public:                                              \
    static rtti::Type* ClassType()                   \
    {                                                \
        return rtti::CreateType<cls, base>();        \
    }                                                \
    virtual rtti::Type* GetRttiType() const override \
    {                                                \
        return ClassType();                          \
    }
} // namespace rtti