#pragma once

#include "nameof.hpp"
#include "System.h"

namespace rtti
{

// ----------------------------------------------------------------------------
// Type dispatch: reference types (Object) vs value types (Boxed<T>)
// ----------------------------------------------------------------------------

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

template <typename T, bool isobject = is_object<typename inner_type<remove_const_ref<T>>::type>>
struct TypeWrapper
{
    using type = typename inner_type<remove_const_ref<T>>::type;
    using wrapper = type;
    using vartype = Ptr<type>;
    using base = type::BASE_TYPE;
};

template <>
struct TypeWrapper<void, false>
{
    using type = void;
    using wrapper = void;
    using vartype = void;
    using base = void;
};

template <typename T>
struct TypeWrapper<T, false>
{
    using type = remove_const_ref<T>;
    using wrapper = Boxed<T>;
    using vartype = type;
    using base = ObjectBox;
};

template <typename T>
using type_t = typename TypeWrapper<T>::type;

template <typename T>
using wrapper_t = typename TypeWrapper<T>::wrapper;

template <typename T>
using vartype_t = typename TypeWrapper<T>::vartype;

template <typename T>
using base_type_t = typename TypeWrapper<T>::base;

template <typename T>
Type* type_of();

enum class TypeFlags
{
    None = 0,
    Pointer = 1,
    Enum = 2,
    Integral = 4,
    Floating = 8,
    Dynamic = 16
};

// Numeric payload description used for erased numeric conversions.
enum class NumericKind
{
    None = 0,
    Bool,
    Char,
    SignedChar,
    UnsignedChar,
    WChar,
    Char8,
    Char16,
    Char32,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    LongDouble
};

struct NumericValue
{
    bool isFloat = false;
    bool isSigned = false;
    long double f = 0;
    long long i = 0;
    unsigned long long u = 0;
};

using NumericReader = bool (*)(const ObjectPtr&, NumericValue&);
using NumericWriter = bool (*)(const NumericValue&, ObjectPtr&);

template <typename T>
constexpr NumericKind NumericKindOf() noexcept
{
    if constexpr (std::is_same_v<T, bool>)
        return NumericKind::Bool;
    else if constexpr (std::is_same_v<T, char>)
        return NumericKind::Char;
    else if constexpr (std::is_same_v<T, signed char>)
        return NumericKind::SignedChar;
    else if constexpr (std::is_same_v<T, unsigned char>)
        return NumericKind::UnsignedChar;
    else if constexpr (std::is_same_v<T, wchar_t>)
        return NumericKind::WChar;
    else if constexpr (std::is_same_v<T, char8_t>)
        return NumericKind::Char8;
    else if constexpr (std::is_same_v<T, char16_t>)
        return NumericKind::Char16;
    else if constexpr (std::is_same_v<T, char32_t>)
        return NumericKind::Char32;
    else if constexpr (std::is_same_v<T, float>)
        return NumericKind::Float;
    else if constexpr (std::is_same_v<T, double>)
        return NumericKind::Double;
    else if constexpr (std::is_same_v<T, long double>)
        return NumericKind::LongDouble;
    else if constexpr (std::is_integral_v<T>)
    {
        if constexpr (std::is_signed_v<T>)
        {
            if constexpr (sizeof(T) == 2)
                return NumericKind::Int16;
            else if constexpr (sizeof(T) == 4)
                return NumericKind::Int32;
            else
                return NumericKind::Int64;
        }
        else
        {
            if constexpr (sizeof(T) == 2)
                return NumericKind::UInt16;
            else if constexpr (sizeof(T) == 4)
                return NumericKind::UInt32;
            else
                return NumericKind::UInt64;
        }
    }
    else
        return NumericKind::None;
}

template <typename T>
NumericValue MakeNumericValue(const T& value) noexcept
{
    NumericValue result;
    if constexpr (std::is_same_v<T, bool>)
    {
        result.isFloat = false;
        result.isSigned = false;
        result.u = value ? 1ull : 0ull;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        result.isFloat = true;
        result.f = static_cast<long double>(value);
    }
    else if constexpr (std::is_signed_v<T>)
    {
        result.isFloat = false;
        result.isSigned = true;
        result.i = static_cast<long long>(value);
    }
    else
    {
        result.isFloat = false;
        result.isSigned = false;
        result.u = static_cast<unsigned long long>(value);
    }
    return result;
}

template <typename T>
T NumericCast(const NumericValue& value) noexcept
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if (value.isFloat)
            return value.f != 0;
        return value.isSigned ? value.i != 0 : value.u != 0;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        const long double v = value.isFloat ? value.f
                                            : (value.isSigned ? static_cast<long double>(value.i)
                                                              : static_cast<long double>(value.u));
        return static_cast<T>(v);
    }
    else if constexpr (std::is_signed_v<T>)
    {
        if (value.isFloat)
            return static_cast<T>(value.f);
        return value.isSigned ? static_cast<T>(value.i) : static_cast<T>(value.u);
    }
    else
    {
        if (value.isFloat)
            return static_cast<T>(value.f);
        return value.isSigned ? static_cast<T>(value.i) : static_cast<T>(value.u);
    }
}

template <typename T>
inline std::string GetTypeName()
{
    using U = remove_const_ref<T>;
    return std::string(NAMEOF_TYPE(U));
}

template <typename T>
inline std::string GetShortTypeName()
{
    using U = remove_const_ref<T>;
    if constexpr (std::is_pointer_v<U>)
        return std::string(NAMEOF_TYPE(U));
    else
        return std::string(NAMEOF_SHORT_TYPE(U));
}

struct ParameterInfo
{
    Type* ParameterType = nullptr;
    bool IsRef = false;
    bool IsConst = false;
};

Type* RegisterType(const std::string& canonicalName,
                   const std::string& shortName,
                   size_t size,
                   TypeFlags flags,
                   NumericKind numericKind,
                   Type* enumUnderlyingType,
                   Type* pointeeType,
                   Type* baseType,
                   bool dynamic,
                   NumericReader numericReader,
                   NumericWriter numericWriter);

template <typename T>
Type* DefaultEnumRegister(Type* type);

template <typename CLS, typename BASE>
inline Type* CreateType()
{
    if constexpr (std::is_same_v<CLS, Object>)
    {
        static_assert(std::is_same_v<BASE, void>, "Object must not declare a base type");
        static Type* type = RegisterType(GetTypeName<CLS>(), GetShortTypeName<CLS>(), sizeof(CLS), TypeFlags::None,
                                         NumericKind::None, nullptr, nullptr, nullptr, false, nullptr, nullptr);
        return type;
    }
    else if constexpr (std::is_pointer_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>, "pointer types must be boxed values");
        static Type* type = RegisterType(GetTypeName<CLS>(), GetShortTypeName<CLS>(), sizeof(CLS), TypeFlags::Pointer,
                                         NumericKind::None, nullptr, type_of<std::remove_pointer_t<CLS>>(),
                                         type_of<ObjectBox>(), false, nullptr, nullptr);
        return type;
    }
    else if constexpr (std::is_enum_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>, "enum types must be boxed values");
        static Type* type = RegisterType(GetTypeName<CLS>(), GetShortTypeName<CLS>(), sizeof(CLS), TypeFlags::Enum,
                                         NumericKind::None, type_of<std::underlying_type_t<CLS>>(), nullptr,
                                         type_of<ObjectBox>(), false, nullptr, nullptr);
        DefaultEnumRegister<CLS>(type);
        return type;
    }
    else if constexpr (std::is_arithmetic_v<CLS>)
    {
        static_assert(std::is_same_v<BASE, ObjectBox>, "arithmetic types must be boxed values");
        static Type* type = RegisterType(
            GetTypeName<CLS>(), GetShortTypeName<CLS>(), sizeof(CLS),
            std::is_floating_point_v<CLS> ? TypeFlags::Floating : TypeFlags::Integral, NumericKindOf<CLS>(), nullptr,
            nullptr, type_of<ObjectBox>(), false,
            [](const ObjectPtr& obj, NumericValue& out) -> bool
            {
                if (!obj)
                    return false;
                out = MakeNumericValue(Unbox<CLS>(obj));
                return true;
            },
            [](const NumericValue& value, ObjectPtr& out) -> bool
            {
                out = Box(NumericCast<CLS>(value));
                return true;
            });
        return type;
    }
    else
    {
        static_assert(std::is_base_of_v<BASE, CLS> || std::is_same_v<BASE, ObjectBox> || std::is_same_v<BASE, void>,
                      "invalid base type: value types use ObjectBox, reference types use their BASE_TYPE");
        static Type* type = RegisterType(GetTypeName<CLS>(), GetShortTypeName<CLS>(), sizeof(CLS), TypeFlags::None,
                                         NumericKind::None, nullptr, nullptr, type_of<BASE>(), false, nullptr, nullptr);
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

#define RTTI_TYPE_DECLARE(...)                                                                     \
public:                                                                                            \
    using BASE_TYPE = __VA_ARGS__;                                                                 \
    static_assert(std::is_base_of_v<::rtti::Object, BASE_TYPE>, "RTTI_TYPE_DECLARE requires a base derived from rtti::Object");                                   \
    virtual ::rtti::Type* GetRttiType() const override                                             \
    {                                                                                              \
        using THIS_TYPE = ::rtti::remove_const_ref<decltype(*this)>;                               \
        static_assert(std::is_base_of_v<BASE_TYPE, THIS_TYPE>, "RTTI_TYPE_DECLARE base is not a base of this class");                                    \
        return ::rtti::type_of<THIS_TYPE>();                                                       \
    }

} // namespace rtti
