#pragma once
#include "System.h"
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"
#include "MethodInfo.h"

namespace rtti
{
class PropertyInfo : public Attributable
{
private:
    template <typename T, bool>
    friend struct TypeRegister;

private:
    Type* owner;
    std::string name;
    Type* type;
    MethodInfo* getter;
    MethodInfo* setter;

protected:
    PropertyInfo(Type* owner, const std::string& name, Type* type, MethodInfo* getter, MethodInfo* setter, const std::map<size_t, std::any>& attributes)
        : Attributable(attributes)
        , owner(owner)
        , name(name)
        , type(type)
        , getter(getter)
        , setter(setter)
    {
    }

public:
    virtual ~PropertyInfo() = default;

    virtual Type* OwnerType() const { return owner; }

    virtual Type* PropertyType() const { return type; }

    virtual const std::string& GetName() const { return name; }

    virtual bool CanRead() const { return getter != nullptr; }

    virtual bool CanWrite() const { return setter != nullptr; }

    virtual ObjectPtr GetValue(const ObjectPtr& obj) const
    {
        return getter->Invoke(obj, {});
    }

    template <typename T>
    T GetValue(const ObjectPtr& obj) const
    {
        return cast<T>(GetValue(obj));
    }

    virtual void SetValue(const ObjectPtr& obj, const ObjectPtr& value) const
    {
        if (setter != nullptr)
            setter->Invoke(obj, {value});
    }
};
} // namespace rtti