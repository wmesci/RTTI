#pragma once

#include "Reflection.h"
#include "System.h"

namespace rtti
{

class Interface
{
public:
    using RTTI_INTERFACE_TAG = void;
    virtual ~Interface() = default;
};

class Object : public RTTI_OBJECT_BASE
{
private:
    Object(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(const Object&) = delete;
    Object& operator=(Object&&) = delete;

public:
    Object() = default;

    virtual ~Object() = default;

    virtual size_t GetHashCode() const { return std::hash<const void*>()(this); }

    virtual Type* GetRttiType() const { return type_of<Object>(); }

    /// 查询对象是否实现某个接口，返回调整后的接口指针；默认无接口。
    virtual void* QueryInterface(Type* interfaceType)
    {
        (void)interfaceType;
        return nullptr;
    }

    virtual void* QueryInterfaceExt(Type* interfaceType)
    {
        (void)interfaceType;
        return nullptr;
    }

    template <typename I>
    I* As()
    {
        return static_cast<I*>(QueryInterface(type_of<I>()));
    }

    template <typename I>
    const I* As() const
    {
        return static_cast<const I*>(const_cast<Object*>(this)->QueryInterface(type_of<I>()));
    }

    // Returns the physical object that should be used when a member declared by
    // `expected` is invoked on this object. Default objects are their own
    // physical object; DynamicObject overrides this to return an embedded
    // compile-time base instance.
    virtual ObjectPtr GetEmbeddedObject(Type* expected) const
    {
        (void)expected;
        return nullptr;
    }

    // Non-owning variant used by member dispatch.
    virtual Object* GetNativeObject(Type* expected)
    {
        (void)expected;
        return this;
    }

public:
    using BASE_TYPE = void;
};

} // namespace rtti
