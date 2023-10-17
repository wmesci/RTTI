#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(const std::string& name, size_t size, uint32_t flags, Type* base)
{
    return new Type(name, size, flags, base, {});
}