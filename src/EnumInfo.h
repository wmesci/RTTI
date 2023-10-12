#pragma once
#include "Object.h"

namespace Albert
{
struct EnumInfo
{
    int64_t number;
    ObjectPtr value;
    std::string name;
};
} // namespace Albert