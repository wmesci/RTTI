#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Error.h"
#include "MethodInfo.h"
#include "Object.h"
#include "ObjectBox.h"
#include "PropertyInfo.h"
#include "Reflection.h"
#include "System.h"
#include "Type.h"
#include "TypeRegister.h"

namespace rtti
{

class DynamicType;

class DynamicObject : public Object
{
    friend class DynamicPropertyInfo;
    friend class DynamicType;

private:
    Type* m_effectiveType = nullptr;
    std::map<std::string, ObjectPtr> m_properties;
    std::map<Type*, ObjectPtr> m_embeddedBases;

public:
    DynamicObject() = default;

    Type* GetRttiType() const override { return m_effectiveType != nullptr ? m_effectiveType : type_of<DynamicObject>(); }

    ObjectPtr GetEmbeddedObject(Type* expected) const override
    {
        if (expected == nullptr)
            return nullptr;
        if (expected == type_of<Object>())
            return nullptr; // caller treats the dynamic object itself as Object
        for (const auto& [type, instance] : m_embeddedBases)
            if (type == expected || type->IsAssignableTo(expected))
                return instance;
        return nullptr;
    }

    Object* GetNativeObject(Type* expected) override
    {
        if (expected == nullptr || expected->IsDynamicType() || expected == type_of<Object>())
            return this;
        ObjectPtr embedded = GetEmbeddedObject(expected);
        return embedded ? RawFromPtr(embedded) : nullptr;
    }

    void SetEffectiveType(Type* type) { m_effectiveType = type; }

    Type* GetEffectiveType() const { return m_effectiveType; }

    ObjectPtr Get(const std::string& name) const
    {
        auto it = m_properties.find(name);
        return it != m_properties.end() ? it->second : ObjectPtr();
    }

    bool TryGet(const std::string& name, ObjectPtr& value) const
    {
        auto it = m_properties.find(name);
        if (it == m_properties.end())
            return false;
        value = it->second;
        return true;
    }

    void Set(const std::string& name, const ObjectPtr& value) { m_properties[name] = value; }

    const std::map<std::string, ObjectPtr>& GetPropertyValues() const { return m_properties; }
};

class DynamicPropertyInfo : public PropertyInfo
{
public:
    DynamicPropertyInfo(Type* owner,
                        const std::string& name,
                        Type* propertyType,
                        const std::map<size_t, std::any>& attributes = {})
        : PropertyInfo(owner, name, propertyType, nullptr, nullptr, attributes)
    {
    }

    ObjectPtr GetValue(const ObjectPtr& object) const override
    {
        DynamicObject* dynamic = Resolve(object);
        if (dynamic == nullptr)
            return nullptr;

        auto it = dynamic->m_properties.find(GetName());
        if (it != dynamic->m_properties.end())
            return it->second;

        if (GetPropertyType() != nullptr && GetPropertyType()->IsValueType())
        {
            ObjectPtr defaultValue;
            if (GetPropertyType()->TryCreateInstance({}, defaultValue))
                return defaultValue;
        }
        return nullptr;
    }

    bool TryGetValue(const ObjectPtr& object, ObjectPtr& value) const noexcept override
    {
        DynamicObject* dynamic = Resolve(object);
        if (dynamic == nullptr)
            return false;
        auto it = dynamic->m_properties.find(GetName());
        if (it == dynamic->m_properties.end())
        {
            value = nullptr;
            return true;
        }
        value = it->second;
        return true;
    }

    void SetValue(const ObjectPtr& object, const ObjectPtr& value) const override
    {
        DynamicObject* dynamic = Resolve(object);
        if (dynamic == nullptr)
            return;

        ObjectPtr converted = ConvertValue(value);
        if (!converted && value)
            return;
        dynamic->m_properties[GetName()] = converted;
    }

    bool TrySetValue(const ObjectPtr& object, const ObjectPtr& value) const noexcept override
    {
        DynamicObject* dynamic = Resolve(object);
        if (dynamic == nullptr)
            return false;
        ObjectPtr converted;
        if (!TryConvertValue(value, converted))
            return false;
        dynamic->m_properties[GetName()] = converted;
        return true;
    }

    bool CanRead() const override { return true; }

    bool CanWrite() const override { return true; }

private:
    DynamicObject* Resolve(const ObjectPtr& object) const
    {
        if (!object || !GetOwnerType() || !GetOwnerType()->IsAssignableFrom(object))
        {
            ReportError(ErrorCode::TypeMismatch, "dynamic property '" + GetName() + "' used on an incompatible object");
            return nullptr;
        }
        Object* native = object->GetNativeObject(GetOwnerType());
        if (native == nullptr)
        {
            ReportError(ErrorCode::TypeMismatch, "dynamic property '" + GetName() + "' has no dynamic container");
            return nullptr;
        }
        return static_cast<DynamicObject*>(native);
    }

    ObjectPtr ConvertValue(const ObjectPtr& value) const
    {
        ObjectPtr converted;
        if (!TryConvertValue(value, converted))
        {
            ReportError(ErrorCode::TypeMismatch, "dynamic property '" + GetName() + "' type mismatch");
            return nullptr;
        }
        return converted;
    }

    bool TryConvertValue(const ObjectPtr& value, ObjectPtr& converted) const
    {
        converted = value;
        if (!value)
            return GetPropertyType() != nullptr && !GetPropertyType()->IsValueType();
        if (GetPropertyType() == nullptr)
            return true;
        if (GetPropertyType()->IsAssignableFrom(value))
            return true;
        return Type::Convert(value, GetPropertyType(), converted);
    }
};

class DynamicConstructorInfo : public ConstructorInfo
{
public:
    DynamicConstructorInfo(Type* owner,
                           std::initializer_list<ParameterInfo> parameters,
                           std::function<ObjectPtr(const std::vector<ObjectPtr>&)> function,
                           const std::map<size_t, std::any>& attributes = {})
        : ConstructorInfo(owner, parameters,
                          [function = std::move(function)](const std::vector<ObjectPtr>& args, ObjectPtr& result) -> bool
                          {
                              result = function(args);
                              return result != nullptr;
                          },
                          attributes)
    {
    }
};

class DynamicMethodInfo : public MethodInfo
{
public:
    DynamicMethodInfo(Type* owner,
                      const std::string& name,
                      Type* returnType,
                      std::initializer_list<ParameterInfo> parameters,
                      std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> function,
                      const std::map<size_t, std::any>& attributes = {})
        : MethodInfo(owner, name, false, returnType, parameters,
                     [function = std::move(function)](const ObjectPtr& target, const std::vector<ObjectPtr>& args,
                                                      ObjectPtr& result) -> bool
                     {
                         result = function(target, args);
                         return true;
                     },
                     attributes)
    {
    }
};

struct DynamicPropertyDesc
{
    Type* type = nullptr;
    std::map<size_t, std::any> attributes;

    DynamicPropertyDesc() = default;
    /*implicit*/ DynamicPropertyDesc(Type* t)
        : type(t)
    {
    }
    DynamicPropertyDesc(Type* t, std::map<size_t, std::any> attrs)
        : type(t)
        , attributes(std::move(attrs))
    {
    }
};

class DynamicType : public Type
{
    friend DynamicType* NewDynamicType(const std::string&, const std::map<std::string, DynamicPropertyDesc>&,
                                       const std::vector<MethodInfo*>&, const std::map<size_t, std::any>&, Type*);

private:
    Type* m_compileTimeBase = nullptr;

private:
    DynamicType(std::string name, Type* baseType, const std::map<size_t, std::any>& attributes)
        : Type(name, name, 0, TypeFlags::Dynamic, NumericKind::None, nullptr, nullptr, baseType, attributes,
               nullptr, nullptr)
    {
    }

public:
    Type* GetCompileTimeBase() const { return m_compileTimeBase; }

    void SetCompileTimeBase(Type* base) { m_compileTimeBase = base; }

    bool TryCreateInstance(const std::vector<ObjectPtr>& args, ObjectPtr& result) const override
    {
        if (!Type::TryCreateInstance(args, result))
            return false;
        if (!result)
            return Fail(ErrorCode::InvalidDynamicType, "dynamic constructor returned a null object");

        if (!result->GetRttiType()->IsDynamicType() && result->GetRttiType() != type_of<DynamicObject>())
            return Fail(ErrorCode::InvalidDynamicType, "dynamic constructor returned a non-dynamic object");

        auto* dynamicObject = static_cast<DynamicObject*>(RawFromPtr(result));
        dynamicObject->m_effectiveType = const_cast<DynamicType*>(this);

        if (m_compileTimeBase != nullptr && dynamicObject->m_embeddedBases.find(m_compileTimeBase) ==
                                                dynamicObject->m_embeddedBases.end())
        {
            ObjectPtr baseInstance;
            if (!m_compileTimeBase->TryCreateInstance({}, baseInstance))
                return false;
            dynamicObject->m_embeddedBases[m_compileTimeBase] = baseInstance;
        }
        return true;
    }

    void AttachProperty(DynamicPropertyInfo* property)
    {
        property->SetOwner(this);
        AddProperty(std::unique_ptr<PropertyInfo>(property));
    }

    void AttachMethod(DynamicMethodInfo* method)
    {
        method->SetOwner(this);
        AddMethod(std::unique_ptr<MethodInfo>(method));
    }

    void AttachConstructor(DynamicConstructorInfo* constructor)
    {
        constructor->SetOwner(this);
        AddConstructor(std::unique_ptr<ConstructorInfo>(constructor));
    }
};

inline DynamicType* NewDynamicType(const std::string& name,
                                   const std::map<std::string, DynamicPropertyDesc>& schema,
                                   const std::vector<MethodInfo*>& methods = {},
                                   const std::map<size_t, std::any>& attributes = {},
                                   Type* baseType = nullptr)
{
    if (name.empty())
        ThrowError(ErrorCode::InvalidDynamicType, "dynamic type name must not be empty");

    Type* effectiveBase = baseType != nullptr ? baseType : type_of<Object>();
    if (effectiveBase->IsValueType())
        ThrowError(ErrorCode::InvalidDynamicType, "dynamic type base must be a reference type");
    Type* compileTimeBase = nullptr;
    for (Type* current = effectiveBase; current != nullptr; current = current->GetBaseType())
    {
        if (!current->IsDynamicType())
        {
            if (current != type_of<Object>())
                compileTimeBase = current;
            break;
        }
    }
    if (compileTimeBase != nullptr && compileTimeBase->GetConstructor() == nullptr)
        ThrowError(ErrorCode::InvalidDynamicType,
                   "compile-time base '" + compileTimeBase->GetName() + "' must be default-constructible");

    auto dynamicType = std::unique_ptr<DynamicType>(new DynamicType(name, effectiveBase, attributes));
    DynamicType* raw = dynamicType.get();

    if (baseType != nullptr && baseType->IsDynamicType())
        raw->SetCompileTimeBase(static_cast<DynamicType*>(baseType)->GetCompileTimeBase());
    else
        raw->SetCompileTimeBase(compileTimeBase);

    // A default constructor that creates the dynamic container. Embedded
    // compile-time bases are created by TryCreateInstance() afterwards.
    raw->AttachConstructor(new DynamicConstructorInfo(
        raw, {}, [raw](const std::vector<ObjectPtr>&) -> ObjectPtr
        { auto object = ::rtti::MakePtr<DynamicObject>(); object->SetEffectiveType(raw); return object; }));

    for (const auto& [propertyName, descriptor] : schema)
        raw->AttachProperty(new DynamicPropertyInfo(raw, propertyName, descriptor.type, descriptor.attributes));

    for (MethodInfo* method : methods)
    {
        if (method == nullptr)
            continue;
        method->SetOwner(raw);
        raw->AddMethod(std::unique_ptr<MethodInfo>(method));
    }

    TypeRegistry::Get().Register(std::move(dynamicType), true);
    return raw;
}

inline DynamicPropertyInfo* NewDynamicProperty(Type* owner,
                                               const std::string& name,
                                               Type* propertyType,
                                               const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicPropertyInfo(owner, name, propertyType, attributes);
}

inline DynamicMethodInfo* NewDynamicMethod(Type* owner,
                                           const std::string& name,
                                           Type* returnType,
                                           std::initializer_list<ParameterInfo> parameters,
                                           std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> function,
                                           const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicMethodInfo(owner, name, returnType, parameters, std::move(function), attributes);
}

inline DynamicConstructorInfo* NewDynamicConstructor(
    Type* owner,
    std::initializer_list<ParameterInfo> parameters,
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> function,
    const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicConstructorInfo(owner, parameters, std::move(function), attributes);
}

} // namespace rtti
