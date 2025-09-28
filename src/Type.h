#pragma once
#include <vector>
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"

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

    Type(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* baseType, const std::map<size_t, std::any>& attributes)
        : Attributable(attributes)
        , m_name(name)
        , m_size((uint32_t)size)
        , m_flags(flags)
        , m_baseType(baseType)
        , m_underlyingType(underlyingType)
        , next(header)
    {
        header = this;
    }

    ~Type() {}

public:
    // 类型名称
    const std::string& GetName() const { return m_name; }

    // 类型大小
    size_t GetSize() const { return m_size; }

    // 获取底层类型
    Type* GetUnderlyingType() const { return m_underlyingType; }

    // 基类型
    Type* GetBaseType() const { return m_baseType; }

    // 当前类型是否是type的子类
    bool IsSubClassOf(Type* type) const
    {
        auto t = this;
        do
        {
            t = t->GetBaseType();
            if (t == type)
                return true;
        } while (t != nullptr);
        return false;
    }

    template <typename T>
    bool IsSubClassOf() const
    {
        return IsSubClassOf(type_of<T>());
    }

    bool IsValueType() const
    {
        return GetBaseType() == type_of<ObjectBox>();
    }

    bool IsEnum() const
    {
        return HasFlag(TypeFlags::Enum);
    }

    bool IsPointer() const
    {
        return HasFlag(TypeFlags::Pointer);
    }

    bool HasFlag(TypeFlags flag) const
    {
        return ((int)m_flags & (int)flag) != 0;
    }

    const std::map<std::string, ObjectPtr>& GetEnumInfos() const
    {
        return m_enumValues;
    }

    std::string GetEnumName(const ObjectPtr& value) const
    {
        for (auto&& i : m_enumValues)
        {
            if (Compare(i.second, value) == CompareResult::Equals)
            {
                return i.first;
            }
        }
        return std::string();
    }

    ObjectPtr GetEnumValue(const std::string& name) const
    {
        for (auto&& i : m_enumValues)
        {
            if (i.first == name)
            {
                return i.second;
            }
        }
        return nullptr;
    }

    bool IsAssignableFrom(Type* type) const
    {
        if (type == nullptr)
            return false;

        return type->IsAssignableTo(const_cast<Type*>(this));
    }

    template <typename T>
    bool IsAssignableFrom() const
    {
        return IsAssignableFrom(type_of<T>());
    }

    bool IsAssignableFrom(const ObjectPtr& obj) const
    {
        if (obj == nullptr)
            return !IsValueType();

        return IsAssignableFrom(obj->GetRttiType());
    }

    bool IsAssignableTo(Type* type) const
    {
        if (type == nullptr)
            return false;

        if (this == type)
            return true;

        if (IsValueType())
            return type == type_of<Object>() || type == type_of<ObjectBox>();
        else
            return this->IsSubClassOf(type);
    }

    template <typename T>
    bool IsAssignableTo() const
    {
        return IsAssignableTo(type_of<T>());
    }

    // 当前类型能否转换为目标类型，只支持单次转换
    bool CanConvertTo(Type* targetType) const
    {
        Type* sourceType = const_cast<Type*>(this);

        if (targetType->IsAssignableFrom(sourceType))
            return true;

        if (sourceType->IsPointer() && targetType == type_of<void*>())
            return true;

        if (sourceType->IsEnum() && targetType->HasFlag(TypeFlags::Integral))
            return true;

        if (sourceType->HasFlag(TypeFlags::Integral) && targetType->IsEnum())
            return true;

        for (auto&& i : targetType->m_constructors)
        {
            if (i->GetParameters().size() == 1 && i->GetParameters()[0].ParameterType->IsAssignableFrom(sourceType))
                return true;
        }

        for (auto&& i : m_typeConvertors)
        {
            if (i.TargetType->IsAssignableTo(targetType))
                return true;
        }

        return false;
    }

    template <typename T>
    bool CanConvertTo() const
    {
        return CanConvertTo(type_of<T>());
    }

    // 创建当前类型的实例
    ObjectPtr CreateInstance(const std::vector<ObjectPtr>& args) const
    {
        for (size_t i = 0; i < m_constructors.size(); i++)
        {
            auto ctor = m_constructors[i];
            if (ctor->GetParameters().size() == args.size())
            {
                bool ok = true;
                for (size_t j = 0; j < args.size(); j++)
                {
                    auto pt = ctor->GetParameters()[j];
                    if (pt.ParameterType->IsValueType())
                    {
                        if (args[j] == nullptr || args[j]->GetRttiType() != pt.ParameterType)
                        {
                            ok = false;
                            break;
                        }
                    }
                    else
                    {
                        if (args[j] != nullptr && !args[j]->GetRttiType()->CanConvertTo(pt.ParameterType))
                        {
                            ok = false;
                            break;
                        }
                    }
                }
                if (ok)
                {
                    return ctor->Invoke(args);
                }
            }
        }
        return nullptr;
    }

    // 创建当前类型的实例
    template <typename T = Object, typename... Args>
    Ptr<T> Create(Args&&... args) const
    {
        return RTTI_PTR_CAST(T, CreateInstance({cast<ObjectPtr>(args)...}));
    }

    // 获取类型的构造函数
    const std::vector<ConstructorInfo*>& GetConstructors() const
    {
        return m_constructors;
    }

    // 获取类型的构造函数
    ConstructorInfo* GetConstructor() const
    {
        for (size_t i = 0; i < m_constructors.size(); i++)
        {
            auto ctor = m_constructors[i];
            if (0 == ctor->GetParameters().size())
            {
                return ctor;
            }
        }
        return nullptr;
    }

    ConstructorInfo* GetConstructor(std::initializer_list<Type*> args) const
    {
        for (size_t i = 0; i < m_constructors.size(); i++)
        {
            auto ctor = m_constructors[i];
            if (args.size() == ctor->GetParameters().size())
            {
                bool paramMatch = true;
                for (size_t j = 0; j < ctor->GetParameters().size(); j++)
                {
                    auto p = ctor->GetParameters()[j];
                    if (p.ParameterType != args.begin()[j])
                    {
                        paramMatch = false;
                        break;
                    }
                }
                if (paramMatch)
                    return ctor;
            }
        }
        return nullptr;
    }

    ConstructorInfo* GetConstructor(std::initializer_list<ParameterInfo> args) const
    {
        for (size_t i = 0; i < m_constructors.size(); i++)
        {
            auto ctor = m_constructors[i];
            if (args.size() == ctor->GetParameters().size())
            {
                bool paramMatch = true;
                for (size_t j = 0; j < ctor->GetParameters().size(); j++)
                {
                    const auto& p = ctor->GetParameters()[j];
                    const auto& ap = args.begin()[j];
                    if (p.ParameterType != ap.ParameterType || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
                    {
                        paramMatch = false;
                        break;
                    }
                }
                if (paramMatch)
                    return ctor;
            }
        }
        return nullptr;
    }

    // 遍历类型的方法
    std::vector<MethodInfo*> GetMethods() const
    {
        std::vector<MethodInfo*> methods;
        const Type* curType = this;
        while (curType != nullptr)
        {
            methods.insert(methods.end(), curType->m_methods.begin(), curType->m_methods.end());
            curType = curType->GetBaseType();
        }
        return methods;
    }

    // 获取类型的方法
    MethodInfo* GetMethod(const std::string& name) const
    {
        auto curType = this;
        while (curType != nullptr)
        {
            for (size_t i = 0; i < curType->m_methods.size(); i++)
            {
                auto m = curType->m_methods[i];
                if (m->GetName() == name)
                {
                    return m;
                }
            }
            curType = curType->GetBaseType();
        }
        return nullptr;
    }

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<Type*> args) const
    {
        auto curType = this;
        while (curType != nullptr)
        {
            for (size_t i = 0; i < curType->m_methods.size(); i++)
            {
                auto m = curType->m_methods[i];
                if (m->GetName() == name && args.size() == m->GetParameters().size())
                {
                    bool paramMatch = true;
                    for (size_t j = 0; j < m->GetParameters().size(); j++)
                    {
                        auto p = m->GetParameters()[j];
                        if (p.ParameterType != args.begin()[j])
                        {
                            paramMatch = false;
                            break;
                        }
                    }

                    if (paramMatch)
                        return m;
                }
            }
            curType = curType->GetBaseType();
        }
        return nullptr;
    }

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<ParameterInfo> args) const
    {
        auto curType = this;
        while (curType != nullptr)
        {
            for (size_t i = 0; i < curType->m_methods.size(); i++)
            {
                auto m = curType->m_methods[i];
                if (m->GetName() == name && args.size() == m->GetParameters().size())
                {
                    bool paramMatch = true;
                    for (size_t j = 0; j < m->GetParameters().size(); j++)
                    {
                        const auto& p = m->GetParameters()[j];
                        const auto& ap = args.begin()[j];
                        if (p.ParameterType != ap.ParameterType || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
                        {
                            paramMatch = false;
                            break;
                        }
                    }

                    if (paramMatch)
                        return m;
                }
            }
            curType = curType->GetBaseType();
        }
        return nullptr;
    }

    // 遍历类型的属性
    std::vector<PropertyInfo*> GetProperties() const
    {
        std::vector<PropertyInfo*> rets;
        const Type* curType = this;
        while (curType != nullptr)
        {
            rets.insert(rets.end(), curType->m_properties.begin(), curType->m_properties.end());
            curType = curType->GetBaseType();
        }
        return rets;
    }

    // 获取类型的属性
    PropertyInfo* GetProperty(const std::string& name) const
    {
        auto curType = this;
        while (curType != nullptr)
        {
            for (size_t i = 0; i < curType->m_properties.size(); i++)
            {
                if (curType->m_properties[i]->GetName() == name)
                    return curType->m_properties[i];
            }
            curType = curType->GetBaseType();
        }
        return nullptr;
    }

    // 将 obj 转换为目标类型，只支持单次转换
    static bool Convert(const ObjectPtr& obj, Type* targetType, ObjectPtr& target)
    {
        if (obj == nullptr)
        {
            if (!targetType->IsValueType())
            {
                target = nullptr;
                return true;
            }
            return false;
        }

        Type* sourceType = obj->GetRttiType();

        if (targetType->IsAssignableFrom(sourceType))
        {
            target = obj;
            return true;
        }

        if (sourceType->IsPointer() && targetType == type_of<void*>())
        {
            target = rtti::Box(rtti::Unbox<void*>(obj));
            return true;
        }

        for (auto&& i : targetType->m_constructors)
        {
            if (i->GetParameters().size() == 1 && i->GetParameters()[0].ParameterType->IsAssignableFrom(sourceType))
            {
                target = targetType->CreateInstance({obj});
                if (target != nullptr)
                    return true;
            }
        }

        for (auto&& i : sourceType->m_typeConvertors)
        {
            if (i.TargetType->IsAssignableTo(targetType))
            {
                return i.Convert(obj, i.TargetType, target);
            }
        }

        if (sourceType->IsEnum() && targetType->HasFlag(TypeFlags::Integral))
        {
            ObjectPtr underlyingObj = nullptr;
            return Convert(obj, sourceType->GetUnderlyingType(), underlyingObj) && Convert(underlyingObj, targetType, target);
        }

        if (sourceType->HasFlag(TypeFlags::Integral) && targetType->IsEnum())
        {
            ObjectPtr underlyingObj = nullptr;
            return Convert(obj, sourceType->GetUnderlyingType(), underlyingObj) && Convert(underlyingObj, targetType, target);
        }

        return false;
    }

    // 比较 left 和 right
    static bool IsComparable(Type* leftType, Type* rightType)
    {
        if (leftType == nullptr)
        {
            if (rightType == nullptr)
                return true;
            else
                return Type::IsComparable(rightType, leftType);
        }

        for (auto&& i : leftType->m_objectComparers)
        {
            if ((i.TargetType == nullptr && rightType == nullptr) || (i.TargetType != nullptr && rightType != nullptr && rightType->IsAssignableTo(i.TargetType)))
            {
                return true;
            }
        }

        if (rightType != nullptr)
        {
            for (auto&& i : rightType->m_objectComparers)
            {
                if (i.TargetType != nullptr && leftType->IsAssignableTo(i.TargetType))
                {
                    return true;
                }
            }
        }

        return false;
    }

    static CompareResult Compare(const ObjectPtr& left, const ObjectPtr& right)
    {
        if (left == nullptr)
        {
            if (right == nullptr)
                return CompareResult::Equals;
            else
                return Type::Compare(right, left);
        }

        Type* leftType = left->GetRttiType();
        Type* rightType = right != nullptr ? right->GetRttiType() : nullptr;
        for (auto&& i : leftType->m_objectComparers)
        {
            if ((i.TargetType == nullptr && rightType == nullptr) || (i.TargetType != nullptr && rightType != nullptr && rightType->IsAssignableTo(i.TargetType)))
            {
                return i.Compare(left, right);
            }
        }

        if (right != nullptr)
        {
            for (auto&& i : rightType->m_objectComparers)
            {
                if (i.TargetType != nullptr && leftType->IsAssignableTo(i.TargetType))
                {
                    return i.Compare(right, left);
                }
            }
        }

        return CompareResult::Failed;
    }

    // 根据名称查找类型
    static Type* Find(const std::string& name)
    {
        Type* cur = header;
        while (cur != nullptr)
        {
            if (name == cur->GetName())
                return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    static void ForEach(const std::function<void(Type*)>& callback)
    {
        Type* cur = header;
        while (cur != nullptr)
        {
            callback(cur);
            cur = cur->next;
        }
    }

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
    std::map<std::string, ObjectPtr> m_enumValues;
    Type* next;

    static Type* header;
};

inline Type* Type::header = nullptr;

inline Type* NewType(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* base)
{
    return new Type(name, size, flags, underlyingType, base, {});
}

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
        static_assert(std::is_same_v<TFrom*, From> || std::is_same_v<Ptr<TFrom>, From>);
        if constexpr (is_object<TTo>)
        {
            static_assert(std::is_same_v<TTo, To> || std::is_same_v<TTo*, To> || std::is_same_v<Ptr<TTo>, To>);

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
            //        if constexpr (std::is_same_v<Ptr<TFrom>, From>)
            //        {
            //            if constexpr (std::is_same_v<TTo, To> || std::is_same_v<Ptr<TTo>, To>) // SPtr<Subclass> --> Base / SPtr<Base>
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

                    if constexpr (std::is_same_v<Ptr<TFrom>, From>)
                    {
                        if (from != nullptr)
                        {
                            ObjectPtr target = nullptr;
                            if (Type::Convert(from, type_of<TTo>(), target))
                                RETURN(RTTI_PTR_CAST(TTo, target), true)
                            else
                                RETURN(Ptr<TTo>(nullptr), false)
                        }
                        else
                        {
                            RETURN(Ptr<TTo>(nullptr), true)
                        }
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<void*, remove_cr<To>>)
        {
            // Ptr -> void*
            RETURN(RTTI_RAW_FROM_PTR(from), true)
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
                RETURN(RTTI_PTR_CAST(TTo, target), true)

            RETURN(Ptr<TTo>(nullptr), false)
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