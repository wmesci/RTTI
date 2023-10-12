#pragma once
#include "Object.h"

namespace rtti
{
struct EnumInfo
{
    int64_t number;
    ObjectPtr value;
    std::string name;
};
} // namespace rtti