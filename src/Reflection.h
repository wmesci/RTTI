#pragma once
#include "System.h"
#include <typeinfo>

namespace Albert
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

// template<typename T>
// Type* typeof() { return TypeWarper<remove_cr<T>>::ClassType(); }

#define typeof(...) TypeWarper<remove_cr<__VA_ARGS__>>::ClassType()
// typeof<__VA_ARGS__>()

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

extern Type* NewType(size_t size, bool is_trivially_copyable, Type* base);

template <typename CLS>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS>, nullptr);
    return type;
}

template <typename CLS, typename BASE>
Type* CreateType()
{
    static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS>, typeof(BASE));
    return type;
}

// template <typename CLS, typename WARPCLS, typename BASE>
// Type* CreateType()
//{
//     static Type* type = NewType(sizeof(CLS), std::is_trivially_copyable_v<CLS>, typeof(BASE));
//     return type;
// }

#define TYPE_DECLARE(cls, base)            \
public:                                    \
    static Type* ClassType()               \
    {                                      \
        return CreateType<cls, base>();    \
    }                                      \
    virtual Type* GetType() const override \
    {                                      \
        return ClassType();                \
    }

struct TypeRegister
{
    template <typename T>
    static void Register();
};

#define TYPE_DEFINE_BEGIN(cls)                                    \
    template <>                                                   \
    void Albert::TypeRegister::Register<cls>()                    \
    {                                                             \
        using HOST = cls;                                         \
        [[maybe_unused]] auto type = typeof(HOST);                \
        type->m_name = #cls##s;                                   \
        [[maybe_unused]] auto& Attributes = type->Attributes;     \
        [[maybe_unused]] auto& Constructors = type->Constructors; \
        [[maybe_unused]] auto& Methods = type->Methods;           \
        [[maybe_unused]] auto& Properties = type->Properties;

#define TYPE_DEFINE_END()                                                                             \
    if constexpr (std::is_default_constructible<HOST>::value)                                         \
    {                                                                                                 \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST>, {}));              \
    }                                                                                                 \
    if constexpr (std::is_copy_constructible<HOST>::value)                                            \
    {                                                                                                 \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, const HOST&>, {})); \
    }                                                                                                 \
    }

#define TYPE_BOXED_BEGIN(cls)                                     \
    template <>                                                   \
    void Albert::TypeRegister::Register<Albert::Boxed<cls>>()     \
    {                                                             \
        using HOST = cls;                                         \
        [[maybe_unused]] auto type = typeof(HOST);                \
        type->m_name = #cls##s;                                   \
        [[maybe_unused]] auto& Attributes = type->Attributes;     \
        [[maybe_unused]] auto& Constructors = type->Constructors; \
        [[maybe_unused]] auto& Methods = type->Methods;           \
        [[maybe_unused]] auto& Properties = type->Properties;

#define TYPE_BOXED_END()                                                                              \
    if constexpr (std::is_default_constructible<HOST>::value)                                         \
    {                                                                                                 \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST>, {}));              \
    }                                                                                                 \
    if constexpr (std::is_copy_constructible<HOST>::value)                                            \
    {                                                                                                 \
        Constructors.push_back(ConstructorInfo::Register<HOST>(&constructor<HOST, const HOST&>, {})); \
    }                                                                                                 \
    }

#define ADD_ENUM_VALUE(v) type->EnumValues.push_back({.number = (int64_t)(HOST::v), .value = Box(HOST::v), .name = #v##s});

#define TYPE_DEFINE_ENUM(cls, ...)                                \
    template <>                                                   \
    void Albert::TypeRegister::Register<Albert::Boxed<cls>>()     \
    {                                                             \
        static_assert(std::is_enum_v<cls>);                       \
        using HOST = cls;                                         \
        using underlying_type = std::underlying_type<HOST>::type; \
        auto type = typeof(HOST);                                 \
        type->m_name = #cls##s;                                   \
        type->UnderlyingType = typeof(underlying_type);           \
        MACRO_FOR_EACH(ADD_ENUM_VALUE, __VA_ARGS__)               \
    }
} // namespace Albert