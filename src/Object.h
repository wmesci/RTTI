#pragma once
#include "System.h"
#include "Reflection.h"

namespace rtti
{
RTTI_OBJECT_DEFINE
{
private:
    Object(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(const Object&) = delete;

public:
    Object() = default;

    virtual size_t GetHashCode() const
    {
        return std::hash<const void*>()(this);
    }

    virtual Type* GetRttiType() const
    {
        return CreateType<Object, void>();
    }

    virtual ~Object() = default;

public:
    using BASE_TYPE = void;
};
} // namespace rtti