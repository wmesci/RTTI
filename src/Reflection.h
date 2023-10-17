#pragma once
#include "System.h"

namespace rtti
{
namespace
{
template <typename T, typename... Args>
static std::enable_if_t<std::is_constructible<T, Args...>::value, std::shared_ptr<T> (*)(Args&&...)> default_constructor()
{
    return [](Args&&... args) -> std::shared_ptr<T>
    { return std::make_shared<T>(std::forward<Args>(args)...); };
}

template <typename T, typename... Args>
static std::enable_if_t<!std::is_constructible<T, Args...>::value, std::shared_ptr<T> (*)(Args&&...)> default_constructor()
{
    return nullptr;
}
} // namespace

template <typename T, bool isobject = is_object<T>>
struct TypeWarper
{
    using type = remove_cr<T>;
    using objtype = type;
    static Type* ClassType() { return type::ClassType(); }
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
Type* type_of()
{
    return TypeWarper<remove_cr<T>>::ClassType();
}

#define typeof(...) type_of<__VA_ARGS__>()

struct ParameterInfo
{
    Type* Type;
    bool IsRef;
    bool IsConst;
};

template <typename T>
ParameterInfo GetParameterInfo()
{
    return ParameterInfo{
        .Type = typeof(T),
        .IsRef = std::is_reference_v<T>,
        .IsConst = std::is_const_v<std::remove_reference_t<T>>};
}

constexpr uint32_t TYPE_FLAG_TRIVIALLY_COPY = 1;

extern Type* NewType(const std::string& name, size_t size, uint32_t flags, Type* base);

template <typename CLS>
Type* CreateType()
{
    static Type* type = NewType(""s, sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, nullptr);
    return type;
}

template <typename CLS, typename BASE>
Type* CreateType()
{
    static Type* type = NewType(""s, sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, typeof(BASE));
    return type;
}

// template <typename CLS, typename WARPCLS, typename BASE>
// Type* CreateType()
//{
//     static Type* type = NewType(""s, sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, typeof(BASE));
//     return type;
// }

#define TYPE_DECLARE(cls, base, ...)               \
public:                                            \
    static Type* ClassType()                       \
    {                                              \
        return CreateType<cls, base>(__VA_ARGS__); \
    }                                              \
    virtual Type* GetType() const override         \
    {                                              \
        return ClassType();                        \
    }
} // namespace rtti