#pragma once
#include "System.h"
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"

namespace rtti
{
using PropertyGetter = std::function<ObjectPtr(const ObjectPtr&)>;
using PropertySetter = std::function<void(const ObjectPtr&, const ObjectPtr&)>;

template <typename T>
inline T* getSelf(const ObjectPtr& obj)
{
    if constexpr (is_object<T>)
        return static_cast<T*>(obj.get());
    else
        return Unbox<T*>(obj);
}

class PropertyInfo : public Attributable
{
private:
    Type* owner;
    std::string name;
    Type* type;
    const PropertyGetter getter;
    const PropertySetter setter;

    PropertyInfo(Type* owner, const std::string& name, Type* type, const PropertyGetter& getter, const PropertySetter& setter)
        : Attributable({})
        , owner(owner)
        , name(name)
        , type(type)
        , getter(getter)
        , setter(setter)
    {
    }

public:
    Type* OwnerType() const { return owner; }

    Type* PropertyType() const { return type; }

    const std::string& GetName() const { return name; }

    bool CanRead() const { return getter != nullptr; }

    bool CanWrite() const { return setter != nullptr; }

    ObjectPtr GetValue(const ObjectPtr& obj) const
    {
        return getter(obj);
    }

    template <typename T>
    T GetValue(const ObjectPtr& obj) const
    {
        return Unbox<T>(GetValue(obj));
    }

    void SetValue(const ObjectPtr& obj, const ObjectPtr& value) const
    {
        setter(obj, value);
    }

    template <typename Host, typename T>
    static PropertyInfo* Register(const std::string& name, const PropertyGetter& getter, const PropertySetter& setter)
    {
        return new PropertyInfo(typeof(Host), name, typeof(T), getter, setter);
    }

    // 注册实例只读属性
    template <typename Host, typename T>
    static PropertyInfo* Register(const std::string& name, T (Host::*getter)() const)
    {
        assert(getter != nullptr);
        return Register<Host, T>(
            name, [=](const ObjectPtr& obj)
            { return Box((getSelf<Host>(obj)->*getter)()); },
            nullptr);
    }

    // 注册实例属性
    template <typename Host, typename T>
    static PropertyInfo* Register(const std::string& name, T (Host::*getter)() const, void (Host::*setter)(T))
    {
        PropertyGetter warpgetter = (getter == nullptr ? PropertyGetter(nullptr) : (PropertyGetter)[=](const ObjectPtr& obj) {
            return Box((getSelf<Host>(obj)->*getter)());
        });
        PropertySetter warpsetter = (setter == nullptr ? PropertySetter(nullptr) : (PropertySetter)[=](const ObjectPtr& obj, const ObjectPtr& value) {
            (getSelf<Host>(obj)->*setter)(Unbox<T>(value));
        });
        return Register<Host, T>(name, warpgetter, warpsetter);
    }

    // 注册实例属性
    template <typename Host, typename T>
    static PropertyInfo* Register(const std::string& name, T (Host::*getter)() const, void (Host::*setter)(const T&))
    {
        PropertyGetter warpgetter = (getter == nullptr ? PropertyGetter(nullptr) : (PropertyGetter)[=](const ObjectPtr& obj) {
            return Box((getSelf<Host>(obj)->*getter)());
        });
        PropertySetter warpsetter = (setter == nullptr ? PropertySetter(nullptr) : (PropertySetter)[=](const ObjectPtr& obj, const ObjectPtr& value) {
            (getSelf<Host>(obj)->*setter)(Unbox<T>(value));
        });
        return Register<Host, T>(name, warpgetter, warpsetter);
    }
};

#define PROPERTY(name) PropertyInfo::Register(#name##s, &HOST::Get##name, &HOST::Set##name)
#define PROPERTY_READONLY(name) PropertyInfo::Register(#name##s, &HOST::Get##name)
#define FIELD(name) PropertyInfo::Register<HOST, decltype(HOST::name)>( \
    #name##s,                                                           \
    [](const ObjectPtr& obj) { return Box(getSelf<HOST>(obj)->name); }, \
    [](const ObjectPtr& obj, const ObjectPtr& value) { getSelf<HOST>(obj)->name = Unbox<remove_cr<decltype(HOST::name)>>(value); })
#define FIELD_READONLY(name) PropertyInfo::Register<HOST, decltype(HOST::name)>( \
    #name##s,                                                                    \
    [](const ObjectPtr& obj) { return Box(getSelf<HOST>(obj)->name); },          \
    nullptr)
} // namespace rtti