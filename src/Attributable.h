#pragma once
#include "System.h"
#include <vector>

namespace rtti
{
struct Attribute
{
    const bool Inheritable;

    Attribute()
        : Inheritable(true)
    {
    }

    Attribute(bool inheritable)
        : Inheritable(inheritable)
    {
    }

    virtual ~Attribute() {}
};

template <typename T, typename... Args>
inline Attribute* ATTR(Args&&... args)
{
    return new T(args...);
}

class Attributable
{
protected:
    Attributable() = default;

    // virtual Attribute* GetAttributeImpl(const std::type_info& type) const = 0;

public:
    // template<typename T>
    // bool HasAttribute() const
    //{
    //	return GetAttribute<T>() != nullptr;
    // }

    // template<typename T>
    // T* GetAttribute() const
    //{
    //	return static_cast<T*>(GetAttributeImpl(typeid(T)));
    // }
};
} // namespace rtti