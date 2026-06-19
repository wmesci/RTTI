#pragma once
#include "Type.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"
#include "Object.h"

namespace rtti
{

// ============================================================================
// DynamicObject — 属性值存储在 map<string, ObjectPtr> 中
// ============================================================================

class DynamicObject : public Object
{
public:
    using BASE_TYPE = Object;

    std::map<std::string, ObjectPtr> m_properties;

    friend class DynamicPropertyInfo;

private:
    Type* m_effectiveType = nullptr;
    friend class DynamicType;

public:
    DynamicObject() = default;

    Type* GetRttiType() const override
    {
        if (m_effectiveType != nullptr)
            return m_effectiveType;
        return type_of<DynamicObject>();
    }

    ObjectPtr Get(const std::string& name) const
    {
        auto it = m_properties.find(name);
        return it != m_properties.end() ? it->second : ObjectPtr(nullptr);
    }

    void Set(const std::string& name, const ObjectPtr& value)
    {
        m_properties[name] = value;
    }
};

// ============================================================================
// DynamicPropertyInfo — 直接读写 DynamicObject 内部 map
// ============================================================================

class DynamicPropertyInfo : public PropertyInfo
{
    std::string m_key;

public:
    DynamicPropertyInfo(Type* owner, const std::string& name, Type* propertyType,
                        const std::map<size_t, std::any>& attributes = {})
        : PropertyInfo(owner, name, propertyType, nullptr, nullptr, attributes)
        , m_key(name)
    {
    }

    ObjectPtr GetValue(const ObjectPtr& obj) const override
    {
        assert(obj != nullptr);
        auto* dyn = static_cast<DynamicObject*>(RawFromPtr(obj));
        return dyn->m_properties[m_key];
    }

    void SetValue(const ObjectPtr& obj, const ObjectPtr& value) const override
    {
        assert(obj != nullptr);
        auto* dyn = static_cast<DynamicObject*>(RawFromPtr(obj));
        dyn->m_properties[m_key] = value;
    }

    bool CanRead() const override { return true; }
    bool CanWrite() const override { return true; }
};

// ============================================================================
// DynamicConstructorInfo — 包装 std::function
// ============================================================================

class DynamicConstructorInfo : public ConstructorInfo
{
    using FuncType = std::function<ObjectPtr(const std::vector<ObjectPtr>&)>;
    FuncType m_func;

public:
    DynamicConstructorInfo(Type* owner, std::initializer_list<ParameterInfo> params,
                           FuncType func, const std::map<size_t, std::any>& attributes = {})
        : ConstructorInfo(owner, params,
                          [f = std::move(func)](const std::vector<ObjectPtr>& args) -> ObjectPtr {
                              return f(args);
                          }, attributes)
        , m_func(std::move(func))
    {
    }
};

// ============================================================================
// DynamicMethodInfo — 包装 std::function
// ============================================================================

class DynamicMethodInfo : public MethodInfo
{
    using FuncType = std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)>;
    FuncType m_func;

public:
    DynamicMethodInfo(Type* owner, const std::string& name, Type* retType,
                      std::initializer_list<ParameterInfo> params, FuncType func,
                      const std::map<size_t, std::any>& attributes = {})
        : MethodInfo(owner, name, MethodFlags::None, retType, params,
                     [f = std::move(func)](const ObjectPtr& target, const std::vector<ObjectPtr>& args) -> ObjectPtr {
                         return f(target, args);
                     }, attributes)
        , m_func(std::move(func))
    {
    }
};

// ============================================================================
// ── Per-property descriptor (type + optional attributes) ──────────────

struct DynamicPropertyDesc
{
    Type* type = nullptr;
    std::map<size_t, std::any> attributes;

    DynamicPropertyDesc() = default;
    /*implicit*/ DynamicPropertyDesc(Type* t) : type(t) {}
    DynamicPropertyDesc(Type* t, std::map<size_t, std::any> attrs)
        : type(t), attributes(std::move(attrs)) {}
};

// DynamicType — 由 map schema 定义的动态类型
// ============================================================================

class DynamicType : public Type
{
    friend DynamicType* NewDynamicType(
        const std::string&, const std::map<std::string, DynamicPropertyDesc>&,
        const std::vector<MethodInfo*>&, const std::map<size_t, std::any>&, Type*);

    std::map<std::string, Type*> m_schema;
    std::vector<PropertyInfo*>    m_dynamicProperties;
    std::vector<MethodInfo*>      m_dynamicMethods;
    std::vector<ConstructorInfo*> m_dynamicConstructors;

public:
    DynamicType(const std::string& name,
                const std::map<std::string, Type*>& schema,
                const std::vector<MethodInfo*>& methods = {},
                const std::vector<ConstructorInfo*>& constructors = {},
                const std::map<size_t, std::any>& attributes = {},
                Type* baseType = type_of<Object>())
        : Type(name, 0, TypeFlags::None, nullptr, baseType, attributes)
        , m_schema(schema)
        , m_dynamicConstructors(constructors)
    {
        // Always add a default constructor that creates DynamicObject
        // and sets its effective type so IsAssignableFrom works correctly
        auto* self = this;
        m_dynamicConstructors.push_back(new DynamicConstructorInfo(
            this, {}, [self](const std::vector<ObjectPtr>&) -> ObjectPtr {
                auto obj = MakePtr<DynamicObject>();
                static_cast<DynamicObject*>(RawFromPtr(obj))->m_effectiveType = self;
                return obj;
            }));

        for (auto& entry : m_schema)
            m_dynamicProperties.push_back(
                new DynamicPropertyInfo(this, entry.first, entry.second));

        for (auto* m : methods)
            if (m != nullptr)
                m_dynamicMethods.push_back(m);
    }

    std::vector<PropertyInfo*> GetProperties() const override
    {
        auto props = m_dynamicProperties;
        if (auto* base = GetBaseType())
        {
            auto baseProps = base->GetProperties();
            props.insert(props.end(), baseProps.begin(), baseProps.end());
        }
        return props;
    }

    PropertyInfo* GetProperty(const std::string& name) const override
    {
        for (auto* p : m_dynamicProperties)
            if (p->GetName() == name)
                return p;
        if (auto* base = GetBaseType())
            return base->GetProperty(name);
        return nullptr;
    }

    std::vector<MethodInfo*> GetMethods() const override
    {
        auto meths = m_dynamicMethods;
        if (auto* base = GetBaseType())
        {
            auto baseMeths = base->GetMethods();
            meths.insert(meths.end(), baseMeths.begin(), baseMeths.end());
        }
        return meths;
    }

    MethodInfo* GetMethod(const std::string& name) const override
    {
        for (auto* m : m_dynamicMethods)
            if (m != nullptr && m->GetName() == name)
                return m;
        if (auto* base = GetBaseType())
            return base->GetMethod(name);
        return nullptr;
    }

    const std::vector<ConstructorInfo*>& GetConstructors() const override
    {
        return m_dynamicConstructors;
    }

    ConstructorInfo* GetConstructor() const override
    {
        for (auto* c : m_dynamicConstructors)
            if (c->GetParameters().empty())
                return c;
        return nullptr;
    }

    ObjectPtr CreateInstance(const std::vector<ObjectPtr>& args) const override
    {
        for (auto* ctor : m_dynamicConstructors)
            if (ctor->GetParameters().size() == args.size())
                return ctor->Invoke(args);
        return nullptr;
    }
};

// ============================================================================
// 工厂函数
// ============================================================================

// ── NewDynamicType (only overload; Type* → DynamicPropertyDesc is implicit)

inline DynamicType* NewDynamicType(
    const std::string& name,
    const std::map<std::string, DynamicPropertyDesc>& schema,
    const std::vector<MethodInfo*>& methods = {},
    const std::map<size_t, std::any>& attributes = {},
    Type* baseType = type_of<Object>())
{
    auto* dt = new DynamicType(name, {}, methods, {}, attributes, baseType);

    // Build properties with per-property attributes
    for (auto& entry : schema)
        dt->m_dynamicProperties.push_back(
            new DynamicPropertyInfo(dt, entry.first, entry.second.type, entry.second.attributes));

    Type::GetRegistry()[name] = dt;
    return dt;
}

// ── NewDynamicProperty (standalone, not tied to a DynamicType) ────────

inline DynamicPropertyInfo* NewDynamicProperty(
    Type* owner, const std::string& name, Type* propertyType,
    const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicPropertyInfo(owner, name, propertyType, attributes);
}

// ── NewDynamicMethod ───────────────────────────────────────────────────

inline DynamicMethodInfo* NewDynamicMethod(
    Type* owner, const std::string& name, Type* retType,
    std::initializer_list<ParameterInfo> params,
    std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> func,
    const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicMethodInfo(owner, name, retType, params, std::move(func), attributes);
}

inline DynamicConstructorInfo* NewDynamicConstructor(
    Type* owner,
    std::initializer_list<ParameterInfo> params,
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func,
    const std::map<size_t, std::any>& attributes = {})
{
    return new DynamicConstructorInfo(owner, params, std::move(func), attributes);
}

} // namespace rtti
