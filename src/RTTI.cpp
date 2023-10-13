#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"
#include "Reflection.h"

using namespace rtti;

TYPE_DEFINE_BEGIN(ObjectBox)
TYPE_DEFINE_END()

TYPE_BOXED_BEGIN(std::byte)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(int8_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(uint8_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(int16_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(uint16_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(int32_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(uint32_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(int64_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(uint64_t)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(float)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(double)
TYPE_BOXED_END()

TYPE_BOXED_BEGIN(std::string)
TYPE_BOXED_END()

void InitCoreType()
{
    Type::Register<Object>();
    Type::Register<ObjectBox>();

    Type::Register<std::byte>();
    Type::Register<int8_t>();
    Type::Register<uint8_t>();
    Type::Register<int16_t>();
    Type::Register<uint16_t>();
    Type::Register<int32_t>();
    Type::Register<uint32_t>();
    Type::Register<int64_t>();
    Type::Register<uint64_t>();
    Type::Register<float>();
    Type::Register<double>();

    Type::Register<std::string>();
}