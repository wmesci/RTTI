#pragma once
#include <vector>
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"

namespace rtti
{

using Convertor = bool (*)(const ObjectPtr& obj, Type* targetType, ObjectPtr& target);

struct TypeConvertor
{
    // Type* SourceType;
    Type* TargetType;
    Convertor Convert;
};

enum class CompareResult
{
    Equals,
    NotEquals,
    Failed
};

using Comparer = CompareResult (*)(const ObjectPtr& left, const ObjectPtr& right);

struct ObjectComparer
{
    // Type* SourceType;
    Type* TargetType;
    Comparer Compare;
};

class Type : public Attributable
{
private:
    template <typename T, bool>
    friend struct TypeRegister;

    template <typename T>
    friend Type* DefaultEnumRegister(Type* type);

    friend Type* NewType(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* base);

    Type(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* baseType, const std::map<size_t, std::any>& attributes);

    ~Type() {}

public:
    // 类型名称
    const std::string& GetName() const;

    // 类型大小
    size_t GetSize() const { return m_size; }

    // 获取底层类型
    Type* GetUnderlyingType() const { return m_underlyingType; }

    // 基类型
    Type* GetBaseType() const { return m_baseType; }

    // 当前类型是否是type的子类
    bool IsSubClassOf(Type* type) const;

    template <typename T>
    bool IsSubClassOf() const
    {
        return IsSubClassOf(type_of<T>());
    }

    bool IsValueType() const;

    bool IsEnum() const;

    bool IsPointer() const;

    bool HasFlag(TypeFlags flag) const;

    const std::vector<EnumInfo>& GetEnumInfos() const;

    bool GetEnumInfo(const int64_t& number, EnumInfo* pInfo) const;

    bool GetEnumInfo(const std::string& name, EnumInfo* pInfo) const;

    bool IsAssignableFrom(Type* type) const;

    template <typename T>
    bool IsAssignableFrom() const
    {
        return IsAssignableFrom(type_of<T>());
    }

    bool IsAssignableFrom(const ObjectPtr& obj) const;

    bool IsAssignableTo(Type* type) const;

    template <typename T>
    bool IsAssignableTo() const
    {
        return IsAssignableTo(type_of<T>());
    }

    // 当前类型能否转换为目标类型，只支持单次转换
    bool CanConvertTo(Type* targetType) const;

    template <typename T>
    bool CanConvertTo() const
    {
        return CanConvertTo(type_of<T>());
    }

    // 创建当前类型的实例
    ObjectPtr CreateInstance(const std::vector<ObjectPtr>& args) const;

    // 创建当前类型的实例
    template <typename T = Object, typename... Args>
    std::shared_ptr<T> Create(Args&&... args) const
    {
        return std::static_pointer_cast<T>(CreateInstance({cast<ObjectPtr>(args)...}));
    }

    // 获取类型的构造函数
    const std::vector<ConstructorInfo*>& GetConstructors() const;

    // 获取类型的构造函数
    ConstructorInfo* GetConstructor() const;

    ConstructorInfo* GetConstructor(std::initializer_list<Type*> args) const;

    ConstructorInfo* GetConstructor(std::initializer_list<ParameterInfo> args) const;

    // 遍历类型的方法
    std::vector<MethodInfo*> GetMethods() const;

    // 获取类型的方法
    MethodInfo* GetMethod(const std::string& name) const;

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<Type*> args) const;

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<ParameterInfo> args) const;

    // 遍历类型的属性
    std::vector<PropertyInfo*> GetProperties() const;

    // 获取类型的属性
    PropertyInfo* GetProperty(const std::string& name) const;

    // 将 obj 转换为目标类型，只支持单次转换
    static bool Convert(const ObjectPtr& obj, Type* targetType, ObjectPtr& target);

    // 比较 left 和 right
    static bool IsComparable(Type* left, Type* right);

    static CompareResult Compare(const ObjectPtr& left, const ObjectPtr& right);

    // 根据名称查找类型
    static Type* Find(const std::string& name);

    static void ForEach(const std::function<void(Type*)>& callback);

protected:
    std::string m_name;
    uint32_t m_size;
    TypeFlags m_flags;
    Type* m_baseType;
    Type* m_underlyingType = nullptr;
    std::vector<ConstructorInfo*> m_constructors;
    std::vector<MethodInfo*> m_methods;
    std::vector<PropertyInfo*> m_properties;
    std::vector<TypeConvertor> m_typeConvertors;
    std::vector<ObjectComparer> m_objectComparers;
    std::vector<EnumInfo> m_enumValues;
    Type* next;
};

// 将 obj 转换为目标类型，只支持单次转换
inline bool convert(const ObjectPtr& obj, Type* targetType, ObjectPtr& target)
{
    return Type::Convert(obj, targetType, target);
}

// 比较 left 和 right
template <typename TL, typename TR>
inline bool is_comparable()
{
    return Type::IsComparable(type_of<TL>(), type_of<TR>());
}

template <typename TL, typename TR>
inline CompareResult compare(const TL& left, const TR& right)
{
    return Type::Compare(cast<ObjectPtr>(left), cast<ObjectPtr>(right));
}

// 将当前类型转换成指定类型
// For Object (F --> T):
//    To   |    T     |     T*   |  SPtr<T>  |  WPtr<T>  |
// --------+----------+----------+-----------+-----------+
// SPtr<F> | SPtr<T>+ |     /    |  SPtr<T>+ |  WPtr<T>  |
// WPtr<F> | SPtr<F>  |     /    |  SPtr<T>+ |  WPtr<T>  |
//    F*   |    T*    |     T*   |     /     |     /     |
//
// For ValueType (T/T* --> T/T*, Object --> T / T*, T / T* --> Object):
//    To        |       T     |      T*     |  Ptr<Object> | WPtr<Object> |
// -------------+-------------+-------------+--------------+--------------+
// Ptr<Object>  |    Unbox    |    Unbox    |       -      |       -      |
// WPtr<Object> |    Unbox    |    Unbox    |       -      |       -      |
//       F      |     (T)F    |    (T)F     |      Box     |       /      |
//       F*     |     (T)F    |    (T)F     |      Box     |       /      |

template <class To, class From>
inline auto cast(const From& from, bool* pOK)
{
    using TFrom = std::remove_pointer_t<type_t<From>>;
    using TTo = std::remove_pointer_t<type_t<To>>;

    static_assert(!std::is_pointer_v<TFrom> || !is_object<std::remove_pointer_t<TFrom>>);
    static_assert(!std::is_pointer_v<TTo> || !is_object<std::remove_pointer_t<TTo>>);

#define RETURN(v, ok)       \
    {                       \
        if (pOK != nullptr) \
            *pOK = ok;      \
        return v;           \
    }

    if constexpr (is_object<TFrom>)
    {
        static_assert(std::is_same_v<TFrom*, From> || std::is_same_v<std::shared_ptr<TFrom>, From> || std::is_same_v<std::weak_ptr<TFrom>, From>);
        if constexpr (is_object<TTo>)
        {
            static_assert(std::is_same_v<TTo, To> || std::is_same_v<TTo*, To> || std::is_same_v<std::shared_ptr<TTo>, To> || std::is_same_v<std::weak_ptr<TTo>, To>);

            // if constexpr (std::is_same_v<TFrom, TTo> || std::is_convertible_v<TFrom, TTo>)
            //{
            //     if constexpr (std::is_same_v<TFrom*, From>)
            //     {
            //         // Subclass* --> Base / Base*
            //         static_assert(std::is_same_v<TTo, To> || std::is_same_v<TTo*, To>);
            //         RETURN((TTo*)from, true)
            //     }
            //     else
            //     {
            //        static_assert(!std::is_same_v<TTo*, To>);
            //
            //        if constexpr (std::is_same_v<std::weak_ptr<TFrom>, From>)
            //        {
            //            if constexpr (std::is_same_v<TTo, To> || std::is_same_v<std::shared_ptr<TTo>, To>) // WPtr<Subclass> --> Base / SPtr<Base>
            //                RETURN(To(from.lock()), true)
            //            else if constexpr (std::is_same_v<std::weak_ptr<TTo>, To>) // WPtr<Subclass> --> WPtr<Base>
            //                RETURN(To(from), true)
            //        }
            //        else if constexpr (std::is_same_v<std::shared_ptr<TFrom>, From>)
            //        {
            //            if constexpr (std::is_same_v<TTo, To> || std::is_same_v<std::shared_ptr<TTo>, To>) // SPtr<Subclass> --> Base / SPtr<Base>
            //                RETURN(To(from), true)
            //            else if constexpr (std::is_same_v<std::weak_ptr<TTo>, To>) // SPtr<Subclass> --> WPtr<Base>
            //                RETURN(To(from), true)
            //        }
            //    }
            //}
            // else
            {
                bool convertible = std::is_same_v<TFrom, TTo> || std::is_convertible_v<TFrom, TTo> || from == nullptr || from->GetRttiType()->template IsAssignableTo<TTo>();

                if constexpr (std::is_same_v<TFrom*, From>)
                {
                    // Base* --> Subclass / Subclass*
                    static_assert(std::is_same_v<TTo, To> || std::is_same_v<TTo*, To>);
                    if (convertible)
                        RETURN((TTo*)from, true)
                    else
                        RETURN((TTo*)nullptr, false)
                }
                else
                {
                    static_assert(!std::is_same_v<TTo*, To>);

                    if constexpr (std::is_same_v<std::weak_ptr<TFrom>, From>)
                    {
                        // WPtr<Base> --> Subclass / SPtr<Subclass> / WPtr<Subclass>
                        auto obj = cast<std::shared_ptr<TTo>>(from.lock(), pOK);
                        if constexpr (std::is_same_v<std::weak_ptr<TTo>, To>)
                            RETURN(std::weak_ptr<TTo>(obj), true)
                        else
                            RETURN(obj, true)
                    }
                    else if constexpr (std::is_same_v<std::shared_ptr<TFrom>, From>)
                    {
                        if (from != nullptr)
                        {
                            ObjectPtr target = nullptr;
                            if (Type::Convert(from, type_of<TTo>(), target))
                                RETURN(std::static_pointer_cast<TTo>(target), true)
                            else
                                RETURN(std::shared_ptr<TTo>(nullptr), false)
                        }
                        else
                        {
                            RETURN(std::shared_ptr<TTo>(nullptr), true)
                        }
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<void*, remove_cr<To>>)
        {
            // Ptr -> void*
            RETURN(from.get(), true)
        }
        else
        {
            // ObjectPtr -> ValueType / ValueType*
            static_assert(std::is_same_v<From, ObjectPtr>);
            static_assert(std::is_same_v<TTo, remove_cr<To>> || std::is_same_v<TTo, remove_cr<To>*>);

            if (from != nullptr)
            {
                if (from->GetRttiType() == type_of<TTo>() || from->GetRttiType() == type_of<std::remove_pointer_t<TTo>>())
                    RETURN(Unbox<TTo>(from), true);

                if (from->GetRttiType()->IsPointer() && std::is_same_v<TTo, void*>)
                    RETURN(Unbox<TTo>(from), true);

                ObjectPtr target = nullptr;
                if (Type::Convert(from, type_of<TTo>(), target))
                    RETURN(Unbox<TTo>(target), true);
            }

            RTTI_ERROR((std::string("conversion of ") + (from == nullptr ? std::string("nullptr") : from->GetRttiType()->GetName()) + std::string(" to ") + type_of<TTo>()->GetName() + std::string(" is not allowed ")).c_str());
            RETURN(TTo(), false)
        }
    }
    else
    {
        if constexpr (is_object<TTo>)
        {
            // ValueType -> ObjectPtr
            // To -> ObjectPtr / Object, TTo -> Object

            static_assert(std::is_same_v<TTo, Object>);

            ObjectPtr target = nullptr;
            if (Type::Convert(Box(from), type_of<TTo>(), target))
                RETURN(std::static_pointer_cast<TTo>(target), true)

            RETURN(std::shared_ptr<TTo>(nullptr), false)
        }
        else
        {
            // ValueType -> ValueType

            if constexpr (std::is_convertible_v<From, remove_cr<To>>)
                RETURN(TTo(from), true)

            ObjectPtr target = nullptr;
            if (Type::Convert(Box(from), type_of<TTo>(), target))
                RETURN(Unbox<TTo>(target), true)

            RTTI_ERROR((std::string("conversion of ") + type_of<TFrom>()->GetName() + std::string(" to ") + type_of<TTo>()->GetName() + std::string(" is not allowed ")).c_str());
            RETURN(TTo(), false)
        }
    }

#undef RETURN
} // namespace rtti
} // namespace rtti