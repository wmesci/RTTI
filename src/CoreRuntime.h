#pragma once
#include "System.h"

#define INIT_TYPE(...) TypeRegister::Register<typename TypeWarper<__VA_ARGS__>::objtype>()

namespace Albert
{
class CoreRuntime
{
public:
    static void Init(void (*init_custom)() = nullptr);

    static void UnInit();
};
} // namespace Albert
