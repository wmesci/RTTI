#pragma once

#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

#include "Object.h"
#include "Reflection.h"
#include "System.h"

namespace rtti
{


class ObjectBox : public Object
{
    RTTI_TYPE_DECLARE(Object)

public:
    virtual bool IsPointerPayload() const noexcept = 0;
    virtual void* GetPointerValue() noexcept = 0;
    virtual void* GetValueAddress() noexcept = 0;
};

template <typename T>
class Boxed : public ObjectBox
{
    static_assert(!std::is_reference<T>::value, "Boxed<T> cannot hold a reference");
    static_assert(!std::is_const<T>::value, "Boxed<T> cannot hold a const value");

public:
    using BASE_TYPE = ObjectBox;

    Type* GetRttiType() const override { return type_of<T>(); }

    // NOTE: this hashes the boxed payload. Cross-type equality performed by
    // Type::Compare (for example int vs double) is not a hash-container
    // equality contract; hash containers should use a single value type.
    size_t GetHashCode() const override
    {
        if constexpr (std::is_default_constructible<std::hash<T>>::value)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                if (m_object == T(0))
                    return std::hash<T>()(T(0));
            }
            return std::hash<T>()(m_object);
        }
        else
        {
            return Object::GetHashCode();
        }
    }

    explicit Boxed(const T& object)
        : m_object(object)
    {
    }

    explicit Boxed(T&& object)
        : m_object(std::move(object))
    {
    }

    T& Unbox() noexcept { return m_object; }
    const T& Unbox() const noexcept { return m_object; }

    bool IsPointerPayload() const noexcept override { return std::is_pointer<T>::value; }

    void* GetPointerValue() noexcept override
    {
        if constexpr (std::is_pointer<T>::value)
            return reinterpret_cast<void*>(m_object);
        else
            return nullptr;
    }

    void* GetValueAddress() noexcept override { return &m_object; }

private:
    T m_object;
};

// Value types are boxed into a fresh Boxed<T>.
template <ValueType T>
    requires(!is_ptr_v<T>)
inline ObjectPtr Box(T value)
{
    return ::rtti::MakePtr<Boxed<T>>(std::move(value));
}

// Handles are reference types: Box(ptr) returns the object pointer itself and
// never wraps the handle in a Boxed<Ptr<T>>.
template <typename T>
inline ObjectPtr Box(const Ptr<T>& ptr)
{
    static_assert(is_object<T>, "Box(Ptr<T>) requires T to derive from rtti::Object");
    return ObjectPtr(ptr);
}

template <typename T>
inline ObjectPtr Box(Ptr<T>&& ptr)
{
    static_assert(is_object<T>, "Box(Ptr<T>) requires T to derive from rtti::Object");
    return ObjectPtr(std::move(ptr));
}

inline ObjectPtr Box(const char* value) { return Box(std::string(value ? value : "")); }


} // namespace rtti
