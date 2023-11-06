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

class Type : public Attributable
{
private:
    template <typename T, bool, bool>
    friend struct TypeRegister;

    template <typename T>
    friend Type* DefaultEnumRegister(Type* type);

    friend Type* NewType(const std::string& name, size_t size, Type* base);

    Type(const std::string& name, size_t size, Type* baseType, const std::map<size_t, std::any>& attributes);

    ~Type() {}

public:
    // 类型名称
    const std::string& GetName() const;

    // 类型大小
    size_t GetSize() const { return m_size; }

    // 基类型
    Type* GetBaseType() const { return m_baseType; }

    // 当前类型是否是type的子类
    bool IsSubClassOf(Type* type) const;

    template <typename T>
    bool IsSubClassOf() const
    {
        return IsSubClassOf(type_of<T>());
    }

    bool IsBoxedType() const;

    bool IsEnum() const;

    const std::vector<EnumInfo>& GetEnumInfos() const;

    bool GetEnumInfo(const int64_t& number, EnumInfo* pInfo) const;

    bool GetEnumInfo(const std::string& name, EnumInfo* pInfo) const;

    Type* GetEnumUnderlyingType() const;

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

    // 将 obj 转换为目标类型，只支持单次转换
    static bool Convert(const ObjectPtr& obj, Type* targetType, ObjectPtr& targets);

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

    // 根据名称查找类型
    static Type* Find(const std::string& name);

    static void ForEach(const std::function<void(Type*)>& callback);

protected:
    std::string m_name;
    size_t m_size;
    Type* m_baseType;
    Type* m_underlyingType = nullptr;
    std::vector<ConstructorInfo*> m_constructors;
    std::vector<MethodInfo*> m_methods;
    std::vector<PropertyInfo*> m_properties;
    std::vector<TypeConvertor> m_typeConvertors;
    std::vector<EnumInfo> m_enumValues;
    Type* next;
};

// 将当前类型转换成指定类型
// 直接转换：
//   Ptr<Subclass>  -->  Ptr<Base>
//   Subclass*      -->  Base*
//   int            -->  float
// Unbox：
//   ObjectPtr --> ValueType / ValueType*
// Box：
//   ValueType --> ObjectPtr
template <class To, class From>
inline auto cast(const From& from)
{
    using TFrom = typename TypeWarper<remove_cr<From>>::type;
    using TTo = typename TypeWarper<remove_cr<To>>::type;

    static_assert(!std::is_pointer_v<TFrom> || !is_object<std::remove_pointer_t<TFrom>>);
    static_assert(!std::is_pointer_v<TTo> || !is_object<std::remove_pointer_t<TTo>>);

    if constexpr (is_object<TFrom>)
    {
        static_assert(std::is_same_v<std::shared_ptr<TFrom>, From> || std::is_same_v<std::weak_ptr<TFrom>, From>);
        if constexpr (is_object<TTo>)
        {
            // (S/W)Ptr<A> --> (S/W)Ptr<B>
            // From -> (S/W)Ptr<A>,     TFrom -> A
            // To   -> (S/W)Ptr<B> / B, TTo   -> B

            if constexpr (std::is_same_v<std::weak_ptr<TFrom>, From>)
            {
                return cast<To>(from.lock());
            }
            else
            {
                if constexpr (std::is_convertible_v<From, remove_cr<To>>)
                {
                    return To(from);
                }

                if (from != nullptr)
                {
                    ObjectPtr target = nullptr;
                    if (Type::Convert(from, type_of<TTo>(), target))
                        return std::static_pointer_cast<TTo>(target);
                }

                if constexpr (std::is_same_v<std::shared_ptr<TTo>, To> || std::is_same_v<std::weak_ptr<TTo>, To>)
                    return To(nullptr);
                else
                    return std::shared_ptr<TTo>(nullptr);
            }
        }
        else
        {
            // ObjectPtr -> ValueType / ValueType* / SPtr<ValueType>
            static_assert(std::is_same_v<From, ObjectPtr>);
            static_assert(std::is_same_v<TTo, remove_cr<To>> || std::is_same_v<TTo, remove_cr<To>*>);

            assert(from != nullptr);

            if (from->GetRttiType() == type_of<TTo>() || from->GetRttiType() == type_of<std::remove_pointer_t<TTo>>())
                return Unbox<TTo>(from);

            ObjectPtr target = nullptr;
            if (Type::Convert(from, type_of<TTo>(), target))
                return Unbox<TTo>(target);

            RTTI_ERROR((std::string("conversion of ") + from->GetRttiType()->GetName() + std::string(" to ") + type_of<TTo>()->GetName() + std::string(" is not allowed ")).c_str());
            return TTo();
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
                return std::static_pointer_cast<TTo>(target);

            return std::shared_ptr<TTo>(nullptr);
        }
        else
        {
            // ValueType -> ValueType

            if constexpr (std::is_convertible_v<From, remove_cr<To>>)
                return TTo(from);

            ObjectPtr target = nullptr;
            if (Type::Convert(Box(from), type_of<TTo>(), target))
                return Unbox<TTo>(target);

            RTTI_ERROR((std::string("conversion of ") + type_of<TFrom>()->GetName() + std::string(" to ") + type_of<TTo>()->GetName() + std::string(" is not allowed ")).c_str());
            return TTo();
        }
    }
}
} // namespace rtti