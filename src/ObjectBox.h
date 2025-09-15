#pragma once
#include "System.h"
#include "Object.h"
#include "Reflection.h"

namespace rtti
{
class ObjectBox : public Object
{
    TYPE_DECLARE(Object)
public:
    virtual bool IsPointer() const = 0;

    virtual void* GetPointer() = 0;
};

template <typename T>
class Boxed : public ObjectBox
{
    static_assert(!std::is_reference<T>::value);
    static_assert(!std::is_const<T>::value);

public:
    using BASE_TYPE = ObjectBox;

    virtual rtti::Type* GetRttiType() const override
    {
        return rtti::CreateType<T, ObjectBox>();
    }

    size_t GetHashCode() const override
    {
        if constexpr (std::is_default_constructible<std::hash<T>>::value)
            return std::hash<T>()(object);
        else
            return Object::GetHashCode();
    }

    Ptr<Object> Clone() override
    {
        if constexpr (std::is_copy_constructible<T>::value)
        {
            return std::make_shared<Boxed<T>>(object);
        }
        else
        {
            return nullptr;
        }
    }

private:
    T object;

public:
    Boxed(const T& _object)
        : object(_object)
    {
    }

    T& Unbox()
    {
        return object;
    }

    virtual bool IsPointer() const override
    {
        return std::is_pointer<T>::value;
    }

    virtual void* GetPointer() override
    {
        return &object;
    }
};

template <ValueType T>
inline ObjectPtr Box(T value)
{
    return std::make_shared<Boxed<T>>(value);
}

//  Unbox<int>(...)   int   int&   int*
template <ValueType T>
inline T Unbox(const ObjectPtr& ptr)
{
    static_assert(!std::is_rvalue_reference<T>::value);

    // assert(ptr != nullptr && ptr->GetRttiType()->IsBoxedType());
    assert(ptr != nullptr);

    auto objbox = static_cast<ObjectBox*>(ptr.get());

    if constexpr (std::is_pointer_v<T>)
    {
        if (objbox->IsPointer())
        {
            if constexpr (std::is_same_v<T, void*>)
            {
                // Unbox<void*>(int*)
                return objbox->GetPointer();
            }
            else
            {
                // Unbox<int*>(int*)
                assert(ptr->GetRttiType() == type_of<T>());
                auto boxed = static_cast<Boxed<T>*>(objbox);
                return boxed->Unbox();
            }
        }
        else
        {
            if constexpr (!std::is_void_v<std::remove_pointer_t<T>>)
            {
                // Unbox<int*>(int)
                assert(ptr->GetRttiType() == type_of<std::remove_pointer_t<T>>());
                auto boxed = static_cast<Boxed<std::remove_pointer_t<T>>*>(objbox);
                return &boxed->Unbox();
            }
            else
            {
                // Unbox<void*>(...)
                return objbox->GetPointer();
            }
        }
    }
    else if constexpr (std::is_reference_v<T>)
    {
        assert(ptr->GetRttiType() == type_of<T>());
        auto boxed = static_cast<Boxed<remove_cr<T>>*>(objbox);
        return boxed->Unbox();
    }
    else
    {
        auto boxed = static_cast<Boxed<T>*>(objbox);
        return boxed->Unbox();
    }
}
} // namespace rtti