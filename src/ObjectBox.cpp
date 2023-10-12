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