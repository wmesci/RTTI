#pragma once
#include "System.h"

namespace rtti
{
enum class CloneOption
{
    None,
    Shallow,
    Deep
};

constexpr size_t CloneOptionAttribute = HASH("CloneOptionAttribute");
constexpr size_t ClonableAttribute = HASH("ClonableAttribute");

RTTI_OBJECT_DEFINE
{
private:
    Object(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(const Object&) = delete;

public:
    Object();

    virtual size_t GetHashCode() const;

    virtual Type* GetRttiType() const;

    virtual Ptr<Object> Clone();

    virtual ~Object();

public:
    using BASE_TYPE = void;
};
} // namespace rtti