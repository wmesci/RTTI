#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"

using namespace rtti;

Object::Object()
{
}

Type* Object::GetRttiType() const
{
    return CreateType<Object>();
}

size_t Object::GetHashCode() const
{
    return std::hash<const void*>()(this);
}

Object::~Object()
{
}