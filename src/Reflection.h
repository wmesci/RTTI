#pragma once
#include "System.h"

namespace rtti
{
template <typename T, bool isobject = is_object<T>>
struct TypeWarper
{
    using type = remove_cr<T>;
    using objtype = type;
    static Type* ClassType() { return type::ClassType(); }
};

template <>
struct TypeWarper<void, false>
{
    static Type* ClassType() { return nullptr; }
};

template <typename T>
struct TypeWarper<std::shared_ptr<T>, false>
{
    using type = remove_cr<T>;
    using objtype = typename TypeWarper<type>::objtype;
    static Type* ClassType() { return TypeWarper<type>::ClassType(); }
};

template <typename T>
struct TypeWarper<std::weak_ptr<T>, false>
{
    using type = remove_cr<T>;
    using objtype = typename TypeWarper<type>::objtype;
    static Type* ClassType() { return TypeWarper<type>::ClassType(); }
};

template <typename T>
struct TypeWarper<T, false>
{
    using type = remove_cr<T>;
    using objtype = Boxed<type>;
    static Type* ClassType() { return Boxed<type>::ClassType(); }
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

extern Type* NewType(size_t size, Type* base);

template <typename CLS>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), nullptr);
    return type;
}

template <typename CLS, typename BASE>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), type_of<BASE>());
    return type;
}

#define TYPE_DECLARE(cls, base)                  \
public:                                          \
    static rtti::Type* ClassType()               \
    {                                            \
        return rtti::CreateType<cls, base>();    \
    }                                            \
    virtual rtti::Type* GetType() const override \
    {                                            \
        return ClassType();                      \
    }
} // namespace rtti