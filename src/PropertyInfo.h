#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "Attributable.h"
#include "Error.h"
#include "MethodInfo.h"
#include "Object.h"
#include "ObjectBox.h"
#include "System.h"

namespace rtti
{

class PropertyInfo : public Attributable
{
private:
    template <typename T, bool>
    friend struct TypeRegister;

protected:
    PropertyInfo(Type* owner,
                 const std::string& name,
                 Type* type,
                 std::unique_ptr<MethodInfo> getter,
                 std::unique_ptr<MethodInfo> setter,
                 const std::map<size_t, std::any>& attributes)
        : Attributable(attributes)
        , m_owner(owner)
        , m_name(name)
        , m_type(type)
        , m_getter(std::move(getter))
        , m_setter(std::move(setter))
    {
    }

public:
    virtual ~PropertyInfo() = default;

    Type* GetOwnerType() const { return m_owner; }

    Type* GetPropertyType() const { return m_type; }

    const std::string& GetName() const { return m_name; }

    // Internal registration hook used by DynamicType.
    void SetOwner(Type* owner) { m_owner = owner; }

    virtual bool CanRead() const { return m_getter != nullptr; }

    virtual bool CanWrite() const { return m_setter != nullptr; }

    virtual ObjectPtr GetValue(const ObjectPtr& object) const
    {
        if (!m_getter)
        {
            ReportError(ErrorCode::InvalidOperation, "property '" + m_name + "' is not readable");
            return nullptr;
        }
        return m_getter->Invoke(object, {});
    }

    virtual bool TryGetValue(const ObjectPtr& object, ObjectPtr& value) const noexcept
    {
        if (!m_getter)
            return Fail(ErrorCode::InvalidOperation, "property '" + m_name + "' is not readable");
        return m_getter->TryInvoke(object, {}, value);
    }

    template <typename T>
    T GetValue(const ObjectPtr& object) const
    {
        static_assert(!is_object<remove_const_ref<T>>, "use GetValue<Ptr<T>>() for reference types");
        ObjectPtr value = GetValue(object);
        bool ok = false;
        T result = cast<T>(value, &ok);
        if (!ok)
        {
            ReportError(ErrorCode::TypeMismatch, "property '" + m_name + "' cannot be read as the requested type");
            return T{};
        }
        return result;
    }

    template <typename T>
    bool TryGetValue(const ObjectPtr& object, T& value) const noexcept
    {
        static_assert(!is_object<remove_const_ref<T>>, "use TryGetValue<Ptr<T>>() for reference types");
        ObjectPtr boxed;
        if (!TryGetValue(object, boxed))
            return false;
        return try_cast<T>(boxed, value);
    }

    virtual void SetValue(const ObjectPtr& object, const ObjectPtr& value) const
    {
        if (!m_setter)
        {
            ReportError(ErrorCode::ReadOnlyProperty, "property '" + m_name + "' is read-only");
            return;
        }
        m_setter->Invoke(object, { value });
    }

    virtual bool TrySetValue(const ObjectPtr& object, const ObjectPtr& value) const noexcept
    {
        if (!m_setter)
            return Fail(ErrorCode::ReadOnlyProperty, "property '" + m_name + "' is read-only");
        ObjectPtr ignored;
        return m_setter->TryInvoke(object, { value }, ignored);
    }

    template <typename T>
    void SetValue(const ObjectPtr& object, const T& value) const
    {
        SetValue(object, Box(value));
    }

    template <typename T>
    bool TrySetValue(const ObjectPtr& object, const T& value) const noexcept
    {
        ObjectPtr ignored;
        if (!m_setter)
            return Fail(ErrorCode::ReadOnlyProperty, "property '" + m_name + "' is read-only");
        return m_setter->TryInvoke(object, { Box(value) }, ignored);
    }

private:
    Type* m_owner = nullptr;
    std::string m_name;
    Type* m_type = nullptr;
    std::unique_ptr<MethodInfo> m_getter;
    std::unique_ptr<MethodInfo> m_setter;
};

} // namespace rtti
