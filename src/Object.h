#pragma once
#include "System.h"

namespace Albert
{
class Object
{
private:
    Object(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(const Object&) = delete;

public:
    Object();

    virtual Type* GetType() const;

    virtual ~Object();

public:
    static Type* ClassType();
};
} // namespace Albert