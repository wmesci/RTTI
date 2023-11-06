#include "Reflection.h"
#include "Type.h"

using namespace rtti;

Type* rtti::NewType(const std::string& name, size_t size, Type* base)
{
    return new Type(name, size, base, {});
}