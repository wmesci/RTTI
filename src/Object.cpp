#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"

using namespace rtti;

Type* Object::ClassType()
{
    return CreateType<Object>();
}

Object::Object()
{
}

Type* Object::GetRttiType() const
{
    return Object::ClassType();
}

size_t Object::GetHashCode() const
{
    return std::hash<const void*>()(this);
}

Object::~Object()
{
}