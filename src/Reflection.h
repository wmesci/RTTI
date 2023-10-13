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

extern Type* NewType(size_t size, uint32_t flags, Type* base);

template <typename CLS>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, nullptr);
    return type;
}

template <typename CLS, typename BASE>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, typeof(BASE));
    return type;
}

// template <typename CLS, typename WARPCLS, typename BASE>
// Type* CreateType()
//{
//     static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS> ? TYPE_FLAG_TRIVIALLY_COPY : 0, typeof(BASE));
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

struct TypeRegister
{
    template <typename T>
    static void Register();
};

#define TYPE_DEFINE_BEGIN(cls)                                      \
    template <>                                                     \
    void rtti::TypeRegister::Register<cls>()                        \
    {                                                               \
        using HOST = cls;                                           \
        [[maybe_unused]] auto type = typeof(HOST);                  \
        type->m_name = #cls##s;                                     \
        type->m_attributes = {};                                    \
        [[maybe_unused]] auto& Constructors = type->m_constructors; \
        [[maybe_unused]] auto& Methods = type->m_methods;           \
        [[maybe_unused]] auto& Properties = type->m_properties;

#define TYPE_DEFINE_END()                                                                         \
    if constexpr (std::is_default_constructible<HOST>::value)                                     \
    {                                                                                             \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST>));              \
    }                                                                                             \
    if constexpr (std::is_copy_constructible<HOST>::value)                                        \
    {                                                                                             \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, const HOST&>)); \
    }                                                                                             \
    }

#define TYPE_BOXED_BEGIN(cls)                                       \
    template <>                                                     \
    void rtti::TypeRegister::Register<rtti::Boxed<cls>>()           \
    {                                                               \
        using HOST = cls;                                           \
        [[maybe_unused]] auto type = typeof(HOST);                  \
        type->m_name = #cls##s;                                     \
        type->m_attributes = {};                                    \
        [[maybe_unused]] auto& Constructors = type->m_constructors; \
        [[maybe_unused]] auto& Methods = type->m_methods;           \
        [[maybe_unused]] auto& Properties = type->m_properties;

#define TYPE_BOXED_END()                                                                          \
    if constexpr (std::is_default_constructible<HOST>::value)                                     \
    {                                                                                             \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST>));              \
    }                                                                                             \
    if constexpr (std::is_copy_constructible<HOST>::value)                                        \
    {                                                                                             \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, const HOST&>)); \
    }                                                                                             \
    }

#define ADD_ENUM_VALUE(v) type->m_enumValues.push_back({.number = (int64_t)(HOST::v), .value = Box(HOST::v), .name = #v##s});

#define TYPE_DEFINE_ENUM(cls, ...)                                                                        \
    template <>                                                                                           \
    void rtti::TypeRegister::Register<rtti::Boxed<cls>>()                                                 \
    {                                                                                                     \
        static_assert(std::is_enum_v<cls>);                                                               \
        using HOST = cls;                                                                                 \
        using underlying_type = std::underlying_type<HOST>::type;                                         \
        auto type = typeof(HOST);                                                                         \
        type->m_name = #cls##s;                                                                           \
        type->m_underlyingType = typeof(underlying_type);                                                 \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST>));              \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, const HOST&>)); \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, cls>));         \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, int32_t>));     \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, uint32_t>));    \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, int64_t>));     \
        type->m_constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, uint64_t>));    \
        MACRO_FOR_EACH(ADD_ENUM_VALUE, __VA_ARGS__)                                                       \
    }
} // namespace rtti