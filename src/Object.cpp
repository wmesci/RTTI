﻿#include "Object.h"
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

Type* Object::GetType() const
{
    return Object::ClassType();
}

Object::~Object()
{
}