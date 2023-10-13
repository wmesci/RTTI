#pragma once
#include "System.h"
#include <any>
#include <map>

namespace rtti
{
class Attributable
{
protected:
    std::map<std::string, std::any> m_attributes;

protected:
    Attributable(const std::map<std::string, std::any>& attributes)
        : m_attributes(attributes)
    {
    }

public:
    bool HasAttribute(const std::string& name) const
    {
        return m_attributes.find(name) != m_attributes.end();
    }

    std::any GetAttribute(const std::string& name) const
    {
        auto it = m_attributes.find(name);
        if (it != m_attributes.end())
        {
            return it->second;
        }

        return std::any();
    }
};
} // namespace rtti