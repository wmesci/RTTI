#pragma once
#include "System.h"
#include "Object.h"
#include "ObjectBox.h"

namespace Albert
{
class DisplayNameAttribute : public Attribute
{
public:
    const std::string DisplayName;

    DisplayNameAttribute(const std::string& value)
        : DisplayName(value)
    {
    }
};

class HiddenAttribute : public Attribute
{
};

class ReadonlyAttribute : public Attribute
{
};

class OptionsAttribute : public Attribute
{
public:
    using OptionsCallback = std::function<void(Object*, std::function<void(const Object*)>)>;

    const OptionsCallback Options;

    OptionsAttribute(const OptionsCallback& value)
        : Options(value)
    {
    }

    template <typename... Args>
    OptionsAttribute(Args&&... args)
    {
        const_cast<OptionsCallback&>(Options) = [=]([[maybe_unused]] Object* obj, std::function<void(const Object*)> f)
        {
            const Object* values[sizeof...(Args)] = {Box(args)...};
            for (const auto& v : values)
                f(v);
        };
    }
};

template <typename T>
class RangeAttribute : public Attribute
{
public:
    const T Min;
    const T Max;

    RangeAttribute(T min, T max)
        : Min(min)
        , Max(max)
    {
        ENSURE(min < max)
        (min)(max);
    }
};
} // namespace Albert