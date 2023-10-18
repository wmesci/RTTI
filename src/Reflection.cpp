#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(size_t size, Type* base)
{
    return new Type(size, base, {});
}