#pragma once
#include "System.h"
#include <any>
#include <map>

namespace rtti
{
class Attributable
{
protected:
    std::map<size_t, std::any> m_attributes;

protected:
    Attributable(const std::map<size_t, std::any>& attributes)
        : m_attributes(attributes)
    {
    }

public:
    bool HasAttribute(size_t id) const
    {
        return m_attributes.find(id) != m_attributes.end();
    }

    std::any GetAttribute(size_t id) const
    {
        auto it = m_attributes.find(id);
        if (it != m_attributes.end())
        {
            return it->second;
        }

        return std::any();
    }
};
} // namespace rtti