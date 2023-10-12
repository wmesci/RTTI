#include "Reflection.h"
#include "ObjectBox.h"
#include "CoreRuntime.h"

using namespace rtti;

void InitCoreType()
{
    INIT_TYPE(Object);
    INIT_TYPE(ObjectBox);

    INIT_TYPE(std::byte);
    INIT_TYPE(int8_t);
    INIT_TYPE(uint8_t);
    INIT_TYPE(int16_t);
    INIT_TYPE(uint16_t);
    INIT_TYPE(int32_t);
    INIT_TYPE(uint32_t);
    INIT_TYPE(int64_t);
    INIT_TYPE(uint64_t);
    INIT_TYPE(float);
    INIT_TYPE(double);

    INIT_TYPE(std::string);
}

void CoreRuntime::Init(void (*init_custom)())
{
    InitCoreType();

    if (init_custom != nullptr)
        init_custom();
}

void CoreRuntime::UnInit()
{
}