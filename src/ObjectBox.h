#pragma once
#include "System.h"
#include "Object.h"
#include "Reflection.h"

namespace Albert
{
class ObjectBox : public Object
{
    TYPE_DECLARE(ObjectBox, Object)
public:
    virtual ObjectPtr Convert(Type* targetType) const = 0;

    virtual bool IsPointer() const = 0;

    virtual void* GetPointer() = 0;
};

template <typename T>
class Boxed : public ObjectBox
{
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>);
    TYPE_DECLARE(T, ObjectBox)
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

    virtual ObjectPtr Convert(Type* targetType) const override;

    virtual bool IsPointer() const override
    {
        return std::is_pointer_v<T>;
    }

    virtual void* GetPointer() override
    {
        return &object;
    }
};

// 装箱
template <typename T>
inline std::enable_if_t<!is_object<remove_cr<T>>, ObjectPtr> Box(T value)
{
    return std::make_shared<Boxed<T>>(value);
}

template <typename T>
inline std::enable_if_t<is_object<T>, Object*> Box(T* value)
{
    return value;
}

template <typename T>
inline std::enable_if_t<is_object<T>, ObjectPtr> Box(const std::shared_ptr<T>& value)
{
    return value;
}

// 拆箱
//  Unbox<int>(...)   int   int&   int*
template <typename T>
inline std::enable_if_t<!is_object<typename TypeWarper<remove_cr<T>>::type>, T> Unbox(Object* ptr)
{
    static_assert(!std::is_rvalue_reference_v<T>);

    assert(ptr != nullptr);

    auto objbox = static_cast<ObjectBox*>(ptr);

    if constexpr (std::is_pointer_v<T>)
    {
        if (objbox->IsPointer())
        {
            // Unbox<int*>(int*)
            assert(ptr->GetType() == typeof(T));
            auto boxed = static_cast<Boxed<T>*>(objbox);
            assert(boxed != nullptr);
            return boxed->Unbox();
        }
        else
        {
            if constexpr (!std::is_void_v<std::remove_pointer_t<T>>)
            {
                // Unbox<int*>(int)
                assert(ptr->GetType() == typeof(std::remove_pointer_t<T>));
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
        assert(ptr->GetType() == typeof(T));
        auto boxed = static_cast<Boxed<remove_cr<T>>*>(objbox);
        return boxed->Unbox();
    }
    else
    {
        if (typeof(T) != ptr->GetType())
        {
            return Unbox<T>(objbox->Convert(typeof(T)));
        }

        auto boxed = static_cast<Boxed<T>*>(objbox);
        return boxed->Unbox();
    }
}

// Unbox<int>(...) ect...
template <typename T>
inline std::enable_if_t<!is_object<typename TypeWarper<remove_cr<T>>::type>, T> Unbox(const ObjectPtr& ptr)
{
    return Unbox<T>(ptr.get());
}

// Unbox<SubclassObject>(...)
template <typename T>
inline std::enable_if_t<is_object<typename TypeWarper<remove_cr<T>>::type> && std::is_same<remove_cr<T>, typename TypeWarper<remove_cr<T>>::type>::value, remove_cr<T>*> Unbox(Object* ptr)
{
    return static_cast<remove_cr<T>*>(ptr);
}

// Unbox<std::shared_ptr<SubclassObject>>(...)
template <typename T>
inline std::enable_if_t<is_object<typename TypeWarper<remove_cr<T>>::type> && std::is_same<remove_cr<T>, std::shared_ptr<typename TypeWarper<remove_cr<T>>::type>>::value, remove_cr<T>> Unbox(const ObjectPtr& ptr)
{
    return cast<typename TypeWarper<remove_cr<T>>::type>(ptr);
}
} // namespace Albert