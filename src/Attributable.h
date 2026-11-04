#pragma once

#include <any>
#include <map>
#include <string>
#include <utility>

#include "Error.h"
#include "System.h"

namespace rtti
{

class Attributable
{
public:
    virtual ~Attributable() = default;

    const std::map<size_t, std::any>& GetAttributes() const { return m_attributes; }

    bool HasAttribute(size_t id) const { return m_attributes.find(id) != m_attributes.end(); }

    std::any GetAttribute(size_t id) const
    {
        auto it = m_attributes.find(id);
        return it != m_attributes.end() ? it->second : std::any();
    }

    template <typename T>
    T GetAttribute(size_t id, T defaultValue = {}) const
    {
        const T* value = TryGetAttribute<T>(id);
        return value != nullptr ? *value : defaultValue;
    }

    template <typename T>
    const T* TryGetAttribute(size_t id) const
    {
        auto it = m_attributes.find(id);
        if (it == m_attributes.end())
            return nullptr;
        return std::any_cast<T>(&it->second);
    }

    void SetAttributes(const std::map<size_t, std::any>& attributes)
    {
        for (const auto& [id, value] : attributes)
            m_attributes[id] = value;
    }

protected:
    Attributable() = default;

    explicit Attributable(const std::map<size_t, std::any>& attributes)
    {
        SetAttributes(attributes);
    }

    void AddAttribute(size_t id, const std::string& name, std::any value)
    {
        auto nameIt = m_attributeNames.find(id);
        if (nameIt != m_attributeNames.end() && nameIt->second != name)
            FatalError(ErrorCode::InvalidOperation, "attribute hash collision: " + nameIt->second + " vs " + name);
        m_attributeNames[id] = name;
        m_attributes[id] = std::move(value);
    }

    std::map<size_t, std::any> m_attributes;
    std::map<size_t, std::string> m_attributeNames;
};

} // namespace rtti
