#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(size_t size, bool is_trivially_copyable, Type* base)
{
    return new Type(size, is_trivially_copyable, base);
}