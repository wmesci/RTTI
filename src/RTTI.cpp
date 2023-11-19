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

    TypeRegister<int8_t>::New("int8_t")
        .constructor<>()
        .constructor<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<uint8_t>::New("uint8_t")
        .constructor<>()
        .constructor<uint8_t>()
        .convert<int8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<int16_t>::New("int16_t")
        .constructor<>()
        .constructor<int16_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<uint16_t>::New("uint16_t")
        .constructor<>()
        .constructor<uint16_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<int32_t>::New("int32_t")
        .constructor<>()
        .constructor<int32_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<uint32_t>::New("uint32_t")
        .constructor<>()
        .constructor<uint32_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<int64_t>::New("int64_t")
        .constructor<>()
        .constructor<int64_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<uint64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<uint64_t>::New("uint64_t")
        .constructor<>()
        .constructor<uint64_t>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<float>()
        .convert<double>();

    TypeRegister<float>::New("float")
        .constructor<>()
        .constructor<float>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<double>();

    TypeRegister<double>::New("double")
        .constructor<>()
        .constructor<double>()
        .convert<int8_t>()
        .convert<uint8_t>()
        .convert<int16_t>()
        .convert<uint16_t>()
        .convert<int32_t>()
        .convert<uint32_t>()
        .convert<int64_t>()
        .convert<uint64_t>()
        .convert<float>();

    TypeRegister<std::string>::New("string");
}
} // namespace rtti