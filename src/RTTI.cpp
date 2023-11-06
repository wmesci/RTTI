#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"
#include "Reflection.h"
#include "TypeRegister.h"

namespace rtti
{
void InitCoreType()
{
    TypeRegister<Object>::New("Object");
    TypeRegister<ObjectBox>::New("ObjectBox");

    TypeRegister<int8_t>::New("int8_t");
    TypeRegister<uint8_t>::New("uint8_t");
    TypeRegister<int16_t>::New("int16_t");
    TypeRegister<uint16_t>::New("uint16_t");
    TypeRegister<int32_t>::New("int32_t");
    TypeRegister<uint32_t>::New("uint32_t");
    TypeRegister<int64_t>::New("int64_t");
    TypeRegister<uint64_t>::New("uint64_t");
    TypeRegister<float>::New("float");
    TypeRegister<double>::New("double");

    TypeRegister<std::string>::New("string");
}
} // namespace rtti