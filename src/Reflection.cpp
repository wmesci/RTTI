#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(size_t size, uint32_t flags, Type* base)
{
    return new Type(size, flags, base);
}