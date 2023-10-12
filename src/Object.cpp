#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"

using namespace Albert;

TYPE_DEFINE_BEGIN(Object)
TYPE_DEFINE_END()

Type* Object::ClassType()
{
    return CreateType<Object>();
}

Object::Object()
{
}

Type* Object::GetType() const
{
    return Object::ClassType();
}

Object::~Object()
{
}