#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* base)
{
    return new Type(name, size, flags, underlyingType, base, {});
}