#pragma once
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"
#include "EnumInfo.h"
#include <vector>

namespace rtti
{
class Type : public Attributable
{
private:
    template <typename T, bool, bool>
    friend struct TypeRegister;

    friend Type* NewType(const std::string& name, size_t size, uint32_t flags, Type* base);

    Type(const std::string& name, size_t size, uint32_t flags, Type* baseType, const std::map<std::string, std::any>& attributes);

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
        return IsSubClassOf(typeof(T));
    }

    // 当前类型是否能转换成指定类型
    bool CanCast(Type* type) const;

    template <typename T>
    bool CanCast() const
    {
        return CanCast(typeof(T));
    }

    bool IsBoxedType() const;

    bool IsTrivially() const
    {
        return (m_flags & TYPE_FLAG_TRIVIALLY_COPY) != 0;
    }

    bool IsEnum() const;

    const std::vector<EnumInfo>& GetEnumInfos() const;

    bool GetEnumInfo(const int64_t& number, EnumInfo* pInfo) const;

    bool GetEnumInfo(const std::string& name, EnumInfo* pInfo) const;

    Type* GetEnumUnderlyingType() const;

    bool IsCompatible(Type* type) const;

    template <typename T>
    bool IsCompatible() const
    {
        return IsCompatible(typeof(T));
    }

    bool IsCompatible(Object* obj) const;

    // 创建当前类型的实例
    ObjectPtr CreateInstance(const std::vector<ObjectPtr>& args) const;

    // 创建当前类型的实例
    template <typename T = Object, typename... Args>
    std::shared_ptr<T> Create(Args&&... args) const
    {
        return std::static_pointer_cast<T>(CreateInstance({Box(args)...}));
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

protected:
    std::string m_name;
    size_t m_size;
    uint32_t m_flags;
    Type* m_baseType;
    Type* m_underlyingType = nullptr;
    std::vector<ConstructorInfo*> m_constructors;
    std::vector<MethodInfo*> m_methods;
    std::vector<PropertyInfo*> m_properties;
    std::vector<EnumInfo> m_enumValues;
    Type* next;
};

template <typename T>
ObjectPtr Boxed<T>::Convert(Type* targetType) const
{
    if constexpr (std::is_enum_v<T>)
    {
        if (targetType == typeof(std::int32_t))
        {
            if constexpr (std::is_reference_v<T>)
                return Box(static_cast<std::int32_t&>(object));
            else
                return Box(static_cast<std::int32_t>(object));
        }
    }
    else if constexpr (std::is_same_v<T, std::int32_t>)
    {
        if (targetType->IsEnum())
        {
            EnumInfo info;
            if (targetType->GetEnumInfo(object, &info))
            {
                return info.value;
            }
        }
    }
    return nullptr;
}

// 将当前类型转换成指定类型
template <class T>
inline T* cast(Object* obj)
{
    if (obj != nullptr && obj->GetType()->CanCast<T>())
        return static_cast<T*>(obj);

    return nullptr;
}

template <class T>
inline std::shared_ptr<T> cast(const ObjectPtr& obj)
{
    if (obj != nullptr && obj->GetType()->CanCast<T>())
        return std::static_pointer_cast<T>(obj);

    return nullptr;
}

} // namespace rtti