#pragma once
#include "System.h"

namespace rtti
{
class Object
{
private:
    Object(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(const Object&) = delete;

public:
    Object();

    virtual size_t GetHashCode() const;

    virtual Type* GetRttiType() const;

    virtual ~Object();

public:
    using RTTI_BASE_CLASS = void;
};
} // namespace rtti