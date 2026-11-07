#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Attributable.h"
#include "Error.h"
#include "MethodInfo.h"
#include "Object.h"
#include "ObjectBox.h"
#include "PropertyInfo.h"
#include "Reflection.h"
#include "System.h"

namespace rtti
{

enum class CompareResult
{
    Equals,
    NotEquals,
    Failed
};

using Converter = std::function<bool(const ObjectPtr&, ObjectPtr&)>;
using Comparer = CompareResult (*)(const ObjectPtr&, const ObjectPtr&);

struct TypeConverter
{
    Type* TargetType = nullptr;
    Converter Convert;
};

struct ObjectComparer
{
    Type* TargetType = nullptr;
    Comparer Compare = nullptr;
};

class Type : public Attributable
{
public:
    template <typename T, bool>
    friend struct TypeRegister;

    template <typename T>
    friend Type* DefaultEnumRegister(Type* type);

    friend class TypeRegistry;
    friend class ObjectBox;
    friend Type* RegisterType(const std::string&, const std::string&, size_t, TypeFlags, NumericKind, Type*, Type*,
                              Type*, bool, NumericReader, NumericWriter);
    friend Type* NewType(const std::string&, size_t, TypeFlags, Type*, Type*);
    friend class DynamicType;

protected:
    Type(std::string name,
         std::string shortName,
         size_t size,
         TypeFlags flags,
         NumericKind numericKind,
         Type* enumUnderlyingType,
         Type* pointeeType,
         Type* baseType,
         const std::map<size_t, std::any>& attributes,
         NumericReader numericReader,
         NumericWriter numericWriter)
        : Attributable(attributes)
        , m_name(std::move(name))
        , m_shortName(std::move(shortName))
        , m_size(size)
        , m_flags(flags)
        , m_numericKind(numericKind)
        , m_enumUnderlyingType(enumUnderlyingType)
        , m_pointeeType(pointeeType)
        , m_baseType(baseType)
        , m_numericReader(numericReader)
        , m_numericWriter(numericWriter)
    {
    }

private:
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;

    void EnsureMetadataMutable() const
    {
        if (!IsDynamicType() && IsTypeRegistryFrozen())
            ThrowError(ErrorCode::RegistryFrozen, "cannot modify type metadata after TypeRegistry::Freeze()");
    }

public:
    // Type objects are owned by TypeRegistry. Never delete a Type* returned by
    // Type::Find(); use the registry lifetime.
    virtual ~Type() = default;

    const std::string& GetName() const { return m_name; }

    const std::string& GetShortName() const { return m_shortName; }

    void SetAttributes(const std::map<size_t, std::any>& attributes)
    {
        EnsureMetadataMutable();
        Attributable::SetAttributes(attributes);
    }

    void SetPreferredName(std::string name) { m_preferredName = std::move(name); }

    // Name used for external identifiers / serialized documents. Fully
    // qualified canonical names are the identity; this returns the unique short
    // alias when available (preserving legacy data), otherwise the canonical
    // name. Use GetName() when you need the canonical identity.
    std::string GetSerializationName() const;

    uint64_t GetId() const noexcept { return m_id; }

    uint64_t GetRank() const noexcept { return m_id; }

    // Payload size: sizeof(T) for value types, sizeof(pointer) for pointer
    // types, and the target object size for reference types.
    size_t GetSize() const noexcept { return m_size; }

    TypeFlags GetFlags() const noexcept { return m_flags; }

    bool HasFlag(TypeFlags flag) const noexcept { return (static_cast<int>(m_flags) & static_cast<int>(flag)) != 0; }

    Type* GetBaseType() const noexcept { return m_baseType; }

    Type* GetEnumUnderlyingType() const noexcept { return m_enumUnderlyingType; }

    Type* GetPointeeType() const noexcept { return m_pointeeType; }

    bool IsSubclassOf(Type* type) const
    {
        for (Type* current = m_baseType; current != nullptr; current = current->m_baseType)
            if (current == type)
                return true;
        return false;
    }

    template <typename T>
    bool IsSubclassOf() const
    {
        return IsSubclassOf(type_of<T>());
    }

    bool IsValueType() const noexcept { return m_baseType == type_of<ObjectBox>(); }

    bool IsEnum() const noexcept { return HasFlag(TypeFlags::Enum); }

    bool IsPointer() const noexcept { return HasFlag(TypeFlags::Pointer); }

    bool IsIntegral() const noexcept { return HasFlag(TypeFlags::Integral); }

    bool IsFloating() const noexcept { return HasFlag(TypeFlags::Floating); }

    bool IsNumeric() const noexcept { return m_numericKind != NumericKind::None; }

    bool IsDynamicType() const noexcept { return HasFlag(TypeFlags::Dynamic); }

    bool IsInterface() const noexcept { return HasFlag(TypeFlags::Interface); }

    void AddInterface(Type* interfaceType)
    {
        if (!interfaceType)
            return;
        EnsureMetadataMutable();
        if (Implements(interfaceType))
            return;
        m_interfaces.push_back(interfaceType);
    }

    const std::vector<Type*>& GetInterfaces() const noexcept { return m_interfaces; }

    bool Implements(Type* interfaceType) const
    {
        if (!interfaceType)
            return false;
        for (Type* current = const_cast<Type*>(this); current != nullptr; current = current->m_baseType)
            for (Type* implemented : current->m_interfaces)
                if (implemented == interfaceType)
                    return true;
        return false;
    }

    NumericKind GetNumericKind() const noexcept { return m_numericKind; }

    const std::map<std::string, ObjectPtr>& GetEnumValues() const { return m_enumValues; }

    std::string GetEnumName(const ObjectPtr& value) const
    {
        for (const auto& [name, enumValue] : m_enumValues)
            if (Compare(enumValue, value) == CompareResult::Equals)
                return name;
        return {};
    }

    ObjectPtr GetEnumValue(const std::string& name) const
    {
        auto it = m_enumValues.find(name);
        return it != m_enumValues.end() ? it->second : ObjectPtr();
    }

    bool IsAssignableFrom(Type* type) const
    {
        if (type == nullptr)
            return false;
        return type->IsAssignableTo(const_cast<Type*>(this));
    }

    template <typename T>
    bool IsAssignableFrom() const
    {
        return IsAssignableFrom(type_of<T>());
    }

    bool IsAssignableFrom(const ObjectPtr& object) const
    {
        if (!object)
            return !IsValueType();
        return IsAssignableFrom(object->GetRttiType());
    }

    bool IsAssignableTo(Type* type) const
    {
        if (type == nullptr)
            return false;
        if (this == type)
            return true;
        if (IsValueType())
            return type == type_of<Object>() || type == type_of<ObjectBox>();
        return IsSubclassOf(type);
    }

    template <typename T>
    bool IsAssignableTo() const
    {
        return IsAssignableTo(type_of<T>());
    }

    bool CanConvertTo(Type* targetType) const { return CanConvertTo(targetType, 0, true); }

    template <typename T>
    bool CanConvertTo() const
    {
        return CanConvertTo(type_of<T>());
    }

    ObjectPtr CreateInstance(const std::vector<ObjectPtr>& args) const
    {
        ObjectPtr result;
        if (!TryCreateInstance(args, result))
        {
#if RTTI_HAS_EXCEPTIONS
            RethrowLastError();
#else
            return nullptr;
#endif
        }
        return result;
    }

    template <typename T = Object, typename... Args>
    Ptr<T> CreateInstance(Args&&... args) const
    {
        static_assert(is_object<T>, "CreateInstance<T> requires T to derive from rtti::Object");
        ObjectPtr object = CreateInstance({ Box(std::forward<Args>(args))... });
        bool ok = false;
        Ptr<T> result = cast<T>(object, &ok);
        if (!ok)
        {
            ReportError(ErrorCode::TypeMismatch, "created object cannot be converted to the requested type");
            return Ptr<T>();
        }
        return result;
    }

    virtual bool TryCreateInstance(const std::vector<ObjectPtr>& args, ObjectPtr& result) const
    {
        return TryCreateInstanceImpl(args, result, 0);
    }

    std::vector<ConstructorInfo*> GetConstructors() const
    {
        std::vector<ConstructorInfo*> result;
        result.reserve(m_constructors.size());
        for (const auto& constructor : m_constructors)
            result.push_back(constructor.get());
        return result;
    }

    virtual ConstructorInfo* GetConstructor() const
    {
        for (const auto& constructor : m_constructors)
            if (constructor->GetParameters().empty())
                return constructor.get();
        return nullptr;
    }

    virtual ConstructorInfo* GetConstructor(std::initializer_list<Type*> args) const
    {
        for (const auto& constructor : m_constructors)
        {
            if (constructor->GetParameters().size() != args.size())
                continue;
            bool match = true;
            size_t index = 0;
            for (const auto& parameter : constructor->GetParameters())
            {
                if (parameter.ParameterType != args.begin()[index++])
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return constructor.get();
        }
        return nullptr;
    }

    virtual ConstructorInfo* GetConstructor(std::initializer_list<ParameterInfo> args) const
    {
        for (const auto& constructor : m_constructors)
        {
            if (constructor->GetParameters().size() != args.size())
                continue;
            bool match = true;
            size_t index = 0;
            for (const auto& parameter : constructor->GetParameters())
            {
                const auto& expected = args.begin()[index++];
                if (parameter.ParameterType != expected.ParameterType || parameter.IsRef != expected.IsRef ||
                    parameter.IsConst != expected.IsConst)
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return constructor.get();
        }
        return nullptr;
    }

    std::vector<MethodInfo*> GetMethods() const
    {
        std::vector<MethodInfo*> result;
        std::set<std::string> seen;
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            for (const auto& method : current->m_methods)
            {
                std::string key = method->GetName();
                for (const auto& parameter : method->GetParameters())
                {
                    key += "|";
                    key += parameter.ParameterType ? parameter.ParameterType->GetName() : "?";
                    key += parameter.IsRef ? "&" : "";
                    key += parameter.IsConst ? "c" : "";
                }
                if (seen.insert(key).second)
                    result.push_back(method.get());
            }
        }
        return result;
    }

    MethodInfo* GetMethod(const std::string& name) const
    {
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            auto it = current->m_methodIndex.find(name);
            if (it != current->m_methodIndex.end())
                return it->second;
        }
        return nullptr;
    }

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<Type*> args) const
    {
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            for (const auto& method : current->m_methods)
            {
                if (method->GetName() != name || method->GetParameters().size() != args.size())
                    continue;
                bool match = true;
                size_t index = 0;
                for (const auto& parameter : method->GetParameters())
                {
                    if (parameter.ParameterType != args.begin()[index++])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return method.get();
            }
        }
        return nullptr;
    }

    MethodInfo* GetMethod(const std::string& name, std::initializer_list<ParameterInfo> args) const
    {
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            for (const auto& method : current->m_methods)
            {
                if (method->GetName() != name || method->GetParameters().size() != args.size())
                    continue;
                bool match = true;
                size_t index = 0;
                for (const auto& parameter : method->GetParameters())
                {
                    const auto& expected = args.begin()[index++];
                    if (parameter.ParameterType != expected.ParameterType || parameter.IsRef != expected.IsRef ||
                        parameter.IsConst != expected.IsConst)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return method.get();
            }
        }
        return nullptr;
    }

    std::vector<PropertyInfo*> GetProperties() const
    {
        std::vector<PropertyInfo*> result;
        std::set<std::string> seen;
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            for (const auto& property : current->m_properties)
            {
                if (seen.insert(property->GetName()).second)
                    result.push_back(property.get());
            }
        }
        return result;
    }

    PropertyInfo* GetProperty(const std::string& name) const
    {
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            auto it = current->m_propertyIndex.find(name);
            if (it != current->m_propertyIndex.end())
                return it->second;
        }
        return nullptr;
    }

    static bool Convert(const ObjectPtr& object, Type* targetType, ObjectPtr& target)
    {
        if (targetType == nullptr)
            return Fail(ErrorCode::TypeNotFound, "target type is null");
        if (!object)
        {
            if (!targetType->IsValueType())
            {
                target = nullptr;
                return true;
            }
            return Fail(ErrorCode::TypeMismatch, "cannot convert null to a value type");
        }
        return object->GetRttiType()->TryConvert(object, targetType, target, 0, true);
    }

    static ObjectPtr ConvertOrThrow(const ObjectPtr& object, Type* targetType)
    {
        ObjectPtr result;
        if (!Convert(object, targetType, result))
        {
#if RTTI_HAS_EXCEPTIONS
            RethrowLastError();
#else
            return nullptr;
#endif
        }
        return result;
    }

    static bool IsComparable(Type* leftType, Type* rightType)
    {
        if (leftType == nullptr)
            return rightType == nullptr || IsComparable(rightType, leftType);
        if (rightType == nullptr)
            return leftType->FindComparer(nullptr) != nullptr;
        if (leftType->IsNumeric() && rightType->IsNumeric())
            return true;
        if (!leftType->IsValueType() && !rightType->IsValueType())
            return true;
        if (leftType->FindComparer(rightType) != nullptr)
            return true;
        if (rightType->FindComparer(leftType) != nullptr)
            return true;
        return false;
    }

    static CompareResult Compare(const ObjectPtr& left, const ObjectPtr& right)
    {
        if (!left)
            return right ? CompareResult::NotEquals : CompareResult::Equals;
        if (!right)
            return CompareResult::NotEquals;

        Type* leftType = left->GetRttiType();
        Type* rightType = right->GetRttiType();

        const ObjectComparer* leftComparer = leftType->FindComparer(rightType);
        const ObjectComparer* rightComparer = rightType->FindComparer(leftType);
        if (leftComparer != nullptr && rightComparer != nullptr)
        {
            if (leftType->GetRank() <= rightType->GetRank())
                return leftComparer->Compare(left, right);
            return rightComparer->Compare(right, left);
        }
        if (leftComparer != nullptr)
            return leftComparer->Compare(left, right);
        if (rightComparer != nullptr)
            return rightComparer->Compare(right, left);
        if (leftType->IsNumeric() && rightType->IsNumeric())
            return NumericCompare(left, right);
        if (!leftType->IsValueType() && !rightType->IsValueType())
            return RawFromPtr(left) == RawFromPtr(right) ? CompareResult::Equals : CompareResult::NotEquals;
        return CompareResult::Failed;
    }

    static Type* Find(std::string_view name);

    template <typename F>
    static void ForEach(F&& callback);

    static std::vector<Type*> GetTypes();

    bool ReadNumeric(const ObjectPtr& object, NumericValue& value) const
    {
        if (m_numericReader == nullptr || !object)
            return false;
        return m_numericReader(object, value);
    }

    bool WriteNumeric(const NumericValue& value, ObjectPtr& object) const
    {
        if (m_numericWriter == nullptr)
            return false;
        return m_numericWriter(value, object);
    }

    // Internal registration helpers (used by TypeRegister / DynamicType and
    // the registry). They are idempotent so repeated registration does not
    // duplicate metadata.
    ConstructorInfo* AddConstructor(std::unique_ptr<ConstructorInfo> constructor)
    {
        EnsureMetadataMutable();
        for (const auto& existing : m_constructors)
        {
            if (existing->GetParameters().size() != constructor->GetParameters().size())
                continue;
            bool same = true;
            for (size_t i = 0; i < existing->GetParameters().size(); i++)
            {
                const auto& a = existing->GetParameters()[i];
                const auto& b = constructor->GetParameters()[i];
                if (a.ParameterType != b.ParameterType || a.IsRef != b.IsRef || a.IsConst != b.IsConst)
                {
                    same = false;
                    break;
                }
            }
            if (same)
                return existing.get();
        }
        ConstructorInfo* raw = constructor.get();
        m_constructors.push_back(std::move(constructor));
        return raw;
    }

    MethodInfo* AddMethod(std::unique_ptr<MethodInfo> method)
    {
        EnsureMetadataMutable();
        for (const auto& existing : m_methods)
        {
            if (existing->GetName() != method->GetName())
                continue;
            if (existing->GetParameters().size() != method->GetParameters().size())
                continue;
            bool same = true;
            for (size_t i = 0; i < existing->GetParameters().size(); i++)
            {
                const auto& a = existing->GetParameters()[i];
                const auto& b = method->GetParameters()[i];
                if (a.ParameterType != b.ParameterType || a.IsRef != b.IsRef || a.IsConst != b.IsConst)
                {
                    same = false;
                    break;
                }
            }
            if (same)
                return existing.get();
        }
        MethodInfo* raw = method.get();
        m_methods.push_back(std::move(method));
        if (m_methodIndex.find(raw->GetName()) == m_methodIndex.end())
            m_methodIndex.emplace(raw->GetName(), raw);
        return raw;
    }

    PropertyInfo* AddProperty(std::unique_ptr<PropertyInfo> property)
    {
        EnsureMetadataMutable();
        for (const auto& existing : m_properties)
            if (existing->GetName() == property->GetName())
                return existing.get();
        PropertyInfo* raw = property.get();
        m_properties.push_back(std::move(property));
        m_propertyIndex.emplace(raw->GetName(), raw);
        return raw;
    }

    void AddConverter(Type* targetType, Converter converter)
    {
        EnsureMetadataMutable();
        for (const auto& existing : m_converters)
            if (existing.TargetType == targetType)
                return;
        m_converters.push_back({ targetType, std::move(converter) });
    }

    void AddComparer(Type* targetType, Comparer comparer)
    {
        EnsureMetadataMutable();
        for (const auto& existing : m_comparers)
            if (existing.TargetType == targetType)
                return;
        m_comparers.push_back({ targetType, comparer });
    }

    void AddEnumValue(const std::string& name, ObjectPtr value) {
        EnsureMetadataMutable(); m_enumValues[name] = std::move(value); }

protected:
    friend class TypeRegistry;

    void SetCanonicalName(std::string name) { m_name = std::move(name); }

    void SetShortName(std::string name) { m_shortName = std::move(name); }

    void SetId(uint64_t id) noexcept { m_id = id; }

    bool TryConvert(const ObjectPtr& object, Type* targetType, ObjectPtr& result, int depth,
                    bool allowConstructors) const;

    bool CanConvertTo(Type* targetType, int depth, bool allowConstructors) const;

    bool TryCreateInstanceImpl(const std::vector<ObjectPtr>& args, ObjectPtr& result, int depth) const;

    const ObjectComparer* FindComparer(Type* targetType) const
    {
        for (const Type* current = this; current != nullptr; current = current->m_baseType)
        {
            for (const auto& comparer : current->m_comparers)
                if (comparer.TargetType == targetType)
                    return &comparer;
            for (const auto& comparer : current->m_comparers)
                if (targetType != nullptr && targetType->IsAssignableTo(comparer.TargetType))
                    return &comparer;
        }
        return nullptr;
    }

    static CompareResult NumericCompare(const ObjectPtr& left, const ObjectPtr& right);

private:
    std::string m_name;
    std::string m_shortName;
    std::string m_preferredName;
    uint64_t m_id = 0;
    size_t m_size = 0;
    TypeFlags m_flags = TypeFlags::None;
    NumericKind m_numericKind = NumericKind::None;
    Type* m_enumUnderlyingType = nullptr;
    Type* m_pointeeType = nullptr;
    Type* m_baseType = nullptr;
    NumericReader m_numericReader = nullptr;
    NumericWriter m_numericWriter = nullptr;
    std::vector<std::unique_ptr<ConstructorInfo>> m_constructors;
    std::vector<std::unique_ptr<MethodInfo>> m_methods;
    std::vector<std::unique_ptr<PropertyInfo>> m_properties;
    std::vector<Type*> m_interfaces;
    std::unordered_map<std::string, MethodInfo*> m_methodIndex;
    std::unordered_map<std::string, PropertyInfo*> m_propertyIndex;
    std::vector<TypeConverter> m_converters;
    std::vector<ObjectComparer> m_comparers;
    std::map<std::string, ObjectPtr> m_enumValues;
};

class TypeRegistry
{
public:
    static TypeRegistry& Get()
    {
        static TypeRegistry registry;
        return registry;
    }

    Type* Find(std::string_view name) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_names.find(name);
        return it != m_names.end() ? it->second : nullptr;
    }

    void Freeze()
    {
        std::unique_lock lock(m_mutex);
        m_frozen = true;
    }

    bool IsFrozen() const
    {
        std::shared_lock lock(m_mutex);
        return m_frozen;
    }

    template <typename F>
    void ForEach(F&& callback) const
    {
        std::vector<Type*> snapshot;
        {
            std::shared_lock lock(m_mutex);
            snapshot.reserve(m_owned.size());
            for (const auto& type : m_owned)
                snapshot.push_back(type.get());
        }
        for (Type* type : snapshot)
            callback(type);
    }

    std::vector<Type*> GetTypes() const
    {
        std::vector<Type*> result;
        std::shared_lock lock(m_mutex);
        result.reserve(m_owned.size());
        for (const auto& type : m_owned)
            result.push_back(type.get());
        return result;
    }

    Type* Register(std::unique_ptr<Type> type, bool dynamic)
    {
        std::unique_lock lock(m_mutex);
        if (m_frozen && !dynamic)
            ThrowError(ErrorCode::RegistryFrozen, "cannot register a compile-time type after Freeze(); type '" +
                                                      type->GetName() + "'");

        const std::string canonical = type->GetName();
        auto existing = m_names.find(canonical);
        if (existing != m_names.end())
        {
            if (existing->second != type.get() && m_softAliases.erase(canonical) > 0)
                m_names.erase(existing);
            else if (existing->second != type.get())
                ThrowError(ErrorCode::DuplicateTypeName, "type name already registered: " + canonical);
        }

        Type* raw = type.get();
        raw->SetId(static_cast<uint64_t>(m_owned.size() + 1));
        m_owned.push_back(std::move(type));
        m_names.emplace(canonical, raw);

        // Best-effort compatibility alias: old serialized data used short
        // names. If the short name is already taken by a different type the
        // canonical (fully qualified) name remains the unambiguous identity.
        if (!raw->GetShortName().empty() && raw->GetShortName() != canonical)
        {
            auto alias = m_names.emplace(raw->GetShortName(), raw);
            if (alias.second)
                m_softAliases.insert(raw->GetShortName());
        }

        return raw;
    }

    Type* RegisterAlias(std::string_view name, Type* type, bool explicitAlias)
    {
        std::unique_lock lock(m_mutex);
        if (m_frozen)
            ThrowError(ErrorCode::RegistryFrozen, "cannot add type aliases after Freeze()");
        if (name.empty())
            ThrowError(ErrorCode::InvalidOperation, "type alias must not be empty");
        auto it = m_names.find(name);
        if (it != m_names.end())
        {
            if (it->second == type)
                return type;
            if (explicitAlias)
                ThrowError(ErrorCode::DuplicateTypeName,
                           "type alias '" + std::string(name) + "' already belongs to '" + it->second->GetName() +
                               "'");
            return nullptr;
        }
        m_names.emplace(std::string(name), type);
        m_softAliases.erase(std::string(name));
        if (explicitAlias)
            type->SetPreferredName(std::string(name));
        return type;
    }

    void SetCanonicalName(Type* type, std::string name)
    {
        std::unique_lock lock(m_mutex);
        if (m_frozen)
            ThrowError(ErrorCode::RegistryFrozen, "cannot rename a type after Freeze()");
        auto existing = m_names.find(name);
        if (existing != m_names.end() && existing->second != type)
        {
            if (m_softAliases.erase(std::string(name)) > 0)
                m_names.erase(existing);
            else
                ThrowError(ErrorCode::DuplicateTypeName, "type name already registered: " + name);
        }

        const std::string oldName = type->GetName();
        if (oldName != name)
        {
            auto old = m_names.find(oldName);
            if (old != m_names.end() && old->second == type)
                m_names.erase(old);
        }
        type->SetCanonicalName(name);
        m_names[name] = type;
        if (!type->GetShortName().empty() && type->GetShortName() != name)
        {
            auto alias = m_names.emplace(type->GetShortName(), type);
            if (alias.second)
                m_softAliases.insert(type->GetShortName());
        }
    }

private:
    TypeRegistry() = default;

    mutable std::shared_mutex m_mutex;
    std::vector<std::unique_ptr<Type>> m_owned;
    std::map<std::string, Type*, std::less<>> m_names;
    std::set<std::string> m_softAliases;
    bool m_frozen = false;
};

// ----------------------------------------------------------------------------
// Registration entry points
// ----------------------------------------------------------------------------

inline Type* RegisterType(const std::string& canonicalName,
                          const std::string& shortName,
                          size_t size,
                          TypeFlags flags,
                          NumericKind numericKind,
                          Type* enumUnderlyingType,
                          Type* pointeeType,
                          Type* baseType,
                          bool dynamic,
                          NumericReader numericReader,
                          NumericWriter numericWriter)
{
    if (dynamic)
        flags = static_cast<TypeFlags>(static_cast<int>(flags) | static_cast<int>(TypeFlags::Dynamic));

    auto type = std::unique_ptr<Type>(new Type(canonicalName, shortName, size, flags, numericKind,
                                               enumUnderlyingType, pointeeType, baseType, {}, numericReader,
                                               numericWriter));
    return TypeRegistry::Get().Register(std::move(type), dynamic);
}

inline Type* NewType(const std::string& name, size_t size, TypeFlags flags, Type* enumOrPointee, Type* baseType)
{
    Type* enumUnderlying = flags == TypeFlags::Enum ? enumOrPointee : nullptr;
    Type* pointee = flags == TypeFlags::Pointer ? enumOrPointee : nullptr;
    return RegisterType(name, name, size, flags, NumericKind::None, enumUnderlying, pointee, baseType, false,
                        nullptr, nullptr);
}

inline bool IsTypeRegistryFrozen() noexcept { return TypeRegistry::Get().IsFrozen(); }

inline Type* Type::Find(std::string_view name) { return TypeRegistry::Get().Find(name); }

inline std::string Type::GetSerializationName() const
{
    if (!m_preferredName.empty() && TypeRegistry::Get().Find(m_preferredName) == this)
        return m_preferredName;
    if (m_shortName.empty() || m_shortName == m_name)
        return m_name;
    if (m_name.find("::") == std::string::npos)
        return m_name;
    if (TypeRegistry::Get().Find(m_shortName) == this)
        return m_shortName;
    return m_name;
}

template <typename F>
inline void Type::ForEach(F&& callback)
{
    TypeRegistry::Get().ForEach(std::forward<F>(callback));
}

inline std::vector<Type*> Type::GetTypes() { return TypeRegistry::Get().GetTypes(); }

// ----------------------------------------------------------------------------
// Boxing / unboxing
// ----------------------------------------------------------------------------

namespace detail
{
template <typename T>
void UnboxMismatch(const ObjectPtr& object, const std::string& expected);

[[noreturn]] void UnboxReferenceMismatch(const ObjectPtr& object, const std::string& expected);

template <typename T>
inline T UnboxDefault()
{
    static_assert(!std::is_reference_v<T>, "cannot materialize a default for a reference");
    if constexpr (std::is_default_constructible_v<remove_const_ref<T>>)
        return T{};
    else
        FatalError(ErrorCode::TypeMismatch, "cannot unbox to a non-default-constructible type");
}
}

template <ValueType T>
inline T Unbox(const ObjectPtr& ptr)
{
    static_assert(!std::is_rvalue_reference_v<T>);

    if (!ptr)
    {
        if constexpr (std::is_lvalue_reference_v<T>)
            detail::UnboxReferenceMismatch(ptr, GetTypeName<std::remove_cvref_t<T>>());
        else
        {
            detail::UnboxMismatch<T>(ptr, GetTypeName<std::remove_cvref_t<T>>());
            return detail::UnboxDefault<T>();
        }
    }

    ObjectBox* box = nullptr;
    if (ptr->GetRttiType()->IsValueType())
        box = static_cast<ObjectBox*>(RawFromPtr(ptr));
    if (box == nullptr)
    {
        if constexpr (std::is_lvalue_reference_v<T>)
            detail::UnboxReferenceMismatch(ptr, GetTypeName<std::remove_cvref_t<T>>());
        else
        {
            detail::UnboxMismatch<T>(ptr, GetTypeName<std::remove_cvref_t<T>>());
            return detail::UnboxDefault<T>();
        }
    }

    if constexpr (std::is_same_v<remove_const_ref<T>, void*>)
    {
        if (box->IsPointerPayload())
            return static_cast<T>(box->GetPointerValue());
        return static_cast<T>(box->GetValueAddress());
    }
    else if constexpr (std::is_pointer_v<remove_const_ref<T>>)
    {
        using P = std::remove_pointer_t<remove_const_ref<T>>;
        if (box->IsPointerPayload())
        {
            if (ptr->GetRttiType() != type_of<T>())
            {
                detail::UnboxMismatch<T>(ptr, GetTypeName<T>());
                return static_cast<T>(nullptr);
            }
            return static_cast<T>(static_cast<Boxed<T>*>(box)->Unbox());
        }
        if (ptr->GetRttiType() != type_of<P>())
        {
            detail::UnboxMismatch<T>(ptr, GetTypeName<P>());
            return static_cast<T>(nullptr);
        }
        return static_cast<T>(&static_cast<Boxed<P>*>(box)->Unbox());
    }
    else if constexpr (std::is_lvalue_reference_v<T>)
    {
        using V = remove_const_ref<T>;
        if (ptr->GetRttiType() != type_of<V>())
            detail::UnboxReferenceMismatch(ptr, GetTypeName<V>());
        return static_cast<T>(static_cast<Boxed<V>*>(box)->Unbox());
    }
    else
    {
        if (ptr->GetRttiType() != type_of<remove_const_ref<T>>())
        {
            detail::UnboxMismatch<T>(ptr, GetTypeName<remove_const_ref<T>>());
            return detail::UnboxDefault<T>();
        }
        return static_cast<T>(static_cast<Boxed<remove_const_ref<T>>*>(box)->Unbox());
    }
}

template <typename T>
inline bool try_unbox(const ObjectPtr& ptr, T& out) noexcept
{
    static_assert(!std::is_reference_v<T>, "try_unbox<T> requires a non-reference T");
    if (!ptr || !ptr->GetRttiType()->IsValueType() || ptr->GetRttiType() != type_of<T>())
        return Fail(ErrorCode::TypeMismatch, "try_unbox: type mismatch");
    out = static_cast<Boxed<T>*>(static_cast<ObjectBox*>(RawFromPtr(ptr)))->Unbox();
    return true;
}

// ----------------------------------------------------------------------------
// Conversion
// ----------------------------------------------------------------------------

inline bool Type::TryConvert(const ObjectPtr& object, Type* targetType, ObjectPtr& result, int depth,
                             bool allowConstructors) const
{
    if (targetType == nullptr)
        return Fail(ErrorCode::TypeNotFound, "target type is null");
    if (!object)
    {
        if (!targetType->IsValueType())
        {
            result = nullptr;
            return true;
        }
        return Fail(ErrorCode::TypeMismatch, "cannot convert null to a value type");
    }

    Type* sourceType = object->GetRttiType();

    if (sourceType == targetType || targetType->IsAssignableFrom(sourceType))
    {
        if (!sourceType->IsDynamicType() || targetType->IsDynamicType() || targetType == type_of<Object>())
        {
            result = object;
            return true;
        }
        ObjectPtr embedded = object->GetEmbeddedObject(targetType);
        if (embedded)
        {
            result = embedded;
            return true;
        }
        return Fail(ErrorCode::TypeMismatch, "dynamic object has no embedded base of type '" + targetType->GetName() +
                                                 "'");
    }

    if (sourceType->IsNumeric() && targetType->IsNumeric())
    {
        NumericValue value;
        if (!sourceType->ReadNumeric(object, value) || !targetType->WriteNumeric(value, result))
            return Fail(ErrorCode::TypeMismatch, "numeric conversion failed");
        return true;
    }

    if (sourceType->IsPointer() && targetType == type_of<void*>())
    {
        result = Box(Unbox<void*>(object));
        return true;
    }

    if (sourceType->IsEnum())
    {
        Type* underlying = sourceType->GetEnumUnderlyingType();
        if (underlying == nullptr)
            return Fail(ErrorCode::InvalidOperation, "enum type has no underlying type");

        ObjectPtr converted;
        bool convertedOk = false;
        for (const Type* current = sourceType; current != nullptr && !convertedOk; current = current->m_baseType)
        {
            for (const auto& converter : current->m_converters)
            {
                if (converter.TargetType == underlying && converter.Convert)
                {
                    convertedOk = converter.Convert(object, converted);
                    break;
                }
            }
        }
        if (!convertedOk || !converted)
            return Fail(ErrorCode::NotConvertible, "cannot convert enum '" + sourceType->GetName() +
                                                       "' to its underlying type");
        return converted->GetRttiType()->TryConvert(converted, targetType, result, depth + 1, allowConstructors);
    }

    if (targetType->IsEnum())
    {
        Type* underlying = targetType->GetEnumUnderlyingType();
        if (underlying == nullptr)
            return Fail(ErrorCode::InvalidOperation, "enum target has no underlying type");
        ObjectPtr tmp;
        if (!TryConvert(object, underlying, tmp, depth + 1, false))
            return false;
        return targetType->TryCreateInstanceImpl({ tmp }, result, depth + 1);
    }

    if (allowConstructors && depth < 4)
    {
        for (const auto& constructor : targetType->m_constructors)
        {
            if (constructor->GetParameters().size() != 1)
                continue;
            const ParameterInfo& parameter = constructor->GetParameters()[0];
            if (parameter.ParameterType == nullptr)
                continue;
            if (!parameter.ParameterType->CanConvertTo(sourceType) &&
                !sourceType->CanConvertTo(parameter.ParameterType, depth + 1, false))
                continue;
            if (targetType->TryCreateInstanceImpl({ object }, result, depth + 1))
                return true;
        }
    }

    for (const Type* current = this; current != nullptr; current = current->m_baseType)
    {
        for (const auto& converter : current->m_converters)
        {
            if (converter.TargetType == nullptr || !converter.TargetType->IsAssignableTo(targetType))
                continue;
            ObjectPtr converted;
            if (converter.Convert && converter.Convert(object, converted))
            {
                if (converted == nullptr)
                    continue;
                if (converted->GetRttiType()->IsAssignableTo(targetType))
                {
                    result = converted;
                    return true;
                }
                if (depth < 8 && converted->GetRttiType()->TryConvert(converted, targetType, result, depth + 1, false))
                    return true;
            }
        }
    }

    return Fail(ErrorCode::NotConvertible, "cannot convert '" + sourceType->GetName() + "' to '" +
                                               targetType->GetName() + "'");
}

inline bool Type::CanConvertTo(Type* targetType, int depth, bool allowConstructors) const
{
    if (targetType == nullptr)
        return false;
    if (targetType->IsAssignableFrom(const_cast<Type*>(this)))
        return true;
    if (IsNumeric() && targetType->IsNumeric())
        return true;
    if (IsPointer() && targetType == type_of<void*>())
        return true;
    if (IsEnum() && (targetType->IsNumeric() || targetType->IsEnum()))
        return GetEnumUnderlyingType() != nullptr;
    if (targetType->IsEnum() && (IsNumeric() || IsEnum()))
        return targetType->GetEnumUnderlyingType() != nullptr;
    if (allowConstructors && depth < 4)
    {
        for (const auto& constructor : targetType->m_constructors)
        {
            if (constructor->GetParameters().size() != 1)
                continue;
            Type* parameterType = constructor->GetParameters()[0].ParameterType;
            if (parameterType != nullptr && (parameterType->IsAssignableFrom(const_cast<Type*>(this)) ||
                                             CanConvertTo(parameterType, depth + 1, false)))
                return true;
        }
    }
    for (const Type* current = this; current != nullptr; current = current->m_baseType)
        for (const auto& converter : current->m_converters)
            if (converter.TargetType != nullptr && converter.TargetType->IsAssignableTo(targetType))
                return true;
    return false;
}

inline bool Type::TryCreateInstanceImpl(const std::vector<ObjectPtr>& args, ObjectPtr& result, int depth) const
{
    if (depth > 8)
        return Fail(ErrorCode::Internal, "constructor/convert recursion limit exceeded");

    for (const auto& constructor : m_constructors)
    {
        if (constructor->GetParameters().size() != args.size())
            continue;

        std::vector<ObjectPtr> converted;
        converted.reserve(args.size());
        bool ok = true;
        for (size_t i = 0; i < args.size(); i++)
        {
            Type* parameterType = constructor->GetParameters()[i].ParameterType;
            const ObjectPtr& argument = args[i];
            if (parameterType == nullptr)
            {
                ok = false;
                break;
            }
            if (!argument)
            {
                if (parameterType->IsValueType())
                {
                    ok = false;
                    break;
                }
                converted.push_back(nullptr);
                continue;
            }
            if (parameterType->IsAssignableFrom(argument))
            {
                converted.push_back(argument);
                continue;
            }
            ObjectPtr convertedArgument;
            if (argument->GetRttiType()->TryConvert(argument, parameterType, convertedArgument, depth + 1, false))
                converted.push_back(convertedArgument);
            else
            {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;
        if (constructor->TryInvoke(converted, result))
            return true;
        return false;
    }
    return Fail(ErrorCode::NoMatchingConstructor, "no matching constructor for '" + GetName() + "'");
}

inline CompareResult Type::NumericCompare(const ObjectPtr& left, const ObjectPtr& right)
{
    Type* leftType = left->GetRttiType();
    Type* rightType = right->GetRttiType();
    NumericValue a;
    NumericValue b;
    if (!leftType->ReadNumeric(left, a) || !rightType->ReadNumeric(right, b))
        return CompareResult::Failed;

    if (a.isFloat || b.isFloat)
    {
        const long double x = a.isFloat ? a.f : (a.isSigned ? static_cast<long double>(a.i)
                                                            : static_cast<long double>(a.u));
        const long double y = b.isFloat ? b.f : (b.isSigned ? static_cast<long double>(b.i)
                                                            : static_cast<long double>(b.u));
        return x == y ? CompareResult::Equals : CompareResult::NotEquals;
    }
    if (a.isSigned == b.isSigned)
    {
        if (a.isSigned)
            return a.i == b.i ? CompareResult::Equals : CompareResult::NotEquals;
        return a.u == b.u ? CompareResult::Equals : CompareResult::NotEquals;
    }
    const long double x = a.isSigned ? static_cast<long double>(a.i) : static_cast<long double>(a.u);
    const long double y = b.isSigned ? static_cast<long double>(b.i) : static_cast<long double>(b.u);
    return x == y ? CompareResult::Equals : CompareResult::NotEquals;
}

// ----------------------------------------------------------------------------
// Free helpers
// ----------------------------------------------------------------------------

inline bool convert(const ObjectPtr& object, Type* targetType, ObjectPtr& target)
{
    return Type::Convert(object, targetType, target);
}

template <typename TL, typename TR>
inline bool is_comparable()
{
    return Type::IsComparable(type_of<TL>(), type_of<TR>());
}

template <typename TL, typename TR>
inline CompareResult compare(const TL& left, const TR& right)
{
    return Type::Compare(cast<ObjectPtr>(left), cast<ObjectPtr>(right));
}

} // namespace rtti

namespace rtti
{

namespace detail
{

inline std::string UnboxMessage(const ObjectPtr& object, const std::string& expected)
{
    std::string message = "cannot unbox ";
    message += object && object->GetRttiType() ? object->GetRttiType()->GetName() : std::string("null");
    message += " as ";
    message += expected;
    return message;
}

template <typename T>
inline void UnboxMismatch(const ObjectPtr& object, const std::string& expected)
{
    const std::string message = UnboxMessage(object, expected);
#if RTTI_HAS_EXCEPTIONS
    ThrowError(ErrorCode::TypeMismatch, message);
#else
    (void)Fail(ErrorCode::TypeMismatch, message);
#endif
}

[[noreturn]] inline void UnboxReferenceMismatch(const ObjectPtr& object, const std::string& expected)
{
    const std::string message = UnboxMessage(object, expected);
#if RTTI_HAS_EXCEPTIONS
    ThrowError(ErrorCode::TypeMismatch, message);
#else
    FatalError(ErrorCode::TypeMismatch, message);
#endif
}

template <typename Arg>
inline Arg CastArgument(const ObjectPtr& object, bool& ok)
{
    static_assert(!std::is_rvalue_reference_v<Arg>, "rvalue-reference parameters are not supported");
    using V = remove_const_ref<Arg>;

    if constexpr (std::is_lvalue_reference_v<Arg>)
    {
        if constexpr (is_object<V> || is_ptr_v<V>)
        {
            static_assert(always_false_v<Arg>,
                          "object/reference parameters must be passed as Ptr<T> or ObjectPtr");
        }
        else
        {
            if (!object || !object->GetRttiType()->IsValueType() || object->GetRttiType() != type_of<V>())
                UnboxReferenceMismatch(object, GetTypeName<V>());
            return Unbox<Arg>(object);
        }
    }
    else if constexpr (is_ptr_v<V>)
    {
        return cast<Arg>(object, &ok);
    }
    else if constexpr (is_object<V>)
    {
        static_assert(always_false_v<Arg>, "object parameters must be passed as Ptr<T> or ObjectPtr");
    }
    else if constexpr (std::is_pointer_v<V>)
    {
        if (!object || !object->GetRttiType()->IsValueType())
        {
            ok = false;
            return static_cast<Arg>(nullptr);
        }
        Type* actual = object->GetRttiType();
        if (actual != type_of<Arg>() && actual != type_of<std::remove_pointer_t<Arg>>())
        {
            ok = false;
            return static_cast<Arg>(nullptr);
        }
        return Unbox<Arg>(object);
    }
    else
    {
        return cast<Arg>(object, &ok);
    }
}

template <typename... Args, typename Iterator, std::size_t... I>
inline std::optional<std::tuple<StoredArgumentT<Args>...>> MakeArgsImpl(const Iterator& p,
                                                                        std::index_sequence<I...>)
{
    bool ok = true;
    auto convert = [&]<std::size_t Index>() -> StoredArgumentT<std::tuple_element_t<Index, std::tuple<Args...>>>
    {
        using Arg = StoredArgumentT<std::tuple_element_t<Index, std::tuple<Args...>>>;
        bool localOk = true;
        decltype(auto) value = CastArgument<Arg>(p[Index], localOk);
        ok = ok && localOk;
        return static_cast<StoredArgumentT<std::tuple_element_t<Index, std::tuple<Args...>>>>(value);
    };
    std::tuple<StoredArgumentT<Args>...> tuple{ convert.template operator()<I>()... };
    if (!ok)
        return std::nullopt;
    return tuple;
}

template <typename... Args, typename Iterator>
inline std::optional<std::tuple<StoredArgumentT<Args>...>> MakeArgs(const Iterator& p)
{
    return MakeArgsImpl<Args...>(p, std::index_sequence_for<Args...>{});
}

template <typename T>
struct ptr_pointee
{
    using type = void;
};

template <typename T>
struct ptr_pointee<Ptr<T>>
{
    using type = T;
};

template <typename T>
inline Ptr<T> CastObject(const ObjectPtr& object, bool& ok)
{
    if (!object)
    {
        ok = true;
        return Ptr<T>();
    }

    Type* target = type_of<T>();
    if (!target->IsAssignableFrom(object->GetRttiType()))
    {
        ok = false;
        return Ptr<T>();
    }

    ObjectPtr resolved = object;
    if (object->GetRttiType()->IsDynamicType() && !target->IsDynamicType() && target != type_of<Object>())
    {
        resolved = object->GetEmbeddedObject(target);
        if (!resolved)
        {
            ok = false;
            return Ptr<T>();
        }
    }

    ok = true;
    return PtrCast<T>(resolved);
}

} // namespace detail

template <class To, class From>
inline auto cast(const From& from, bool* pOK)
{
    using FromRaw = remove_const_ref<From>;
    using ToRaw = remove_const_ref<To>;

    constexpr bool fromHandle = is_ptr_v<FromRaw>;
    constexpr bool fromObjectRawPointer =
        std::is_pointer_v<FromRaw> && !std::is_void_v<std::remove_pointer_t<FromRaw>> &&
        is_object<std::remove_pointer_t<FromRaw>>;

    constexpr bool toHandle = is_ptr_v<ToRaw>;
    constexpr bool toObjectRawPointer =
        std::is_pointer_v<ToRaw> && !std::is_void_v<std::remove_pointer_t<ToRaw>> &&
        is_object<std::remove_pointer_t<ToRaw>>;
    constexpr bool toObjectValue = is_object<ToRaw> && !std::is_pointer_v<ToRaw>;
    constexpr bool toVoidPointer = std::is_pointer_v<ToRaw> && std::is_void_v<std::remove_pointer_t<ToRaw>>;

    bool ok = false;

    auto setOk = [&](bool value) -> bool
    {
        ok = value;
        if (pOK != nullptr)
            *pOK = ok;
        return ok;
    };

    if constexpr (toHandle || toObjectValue)
    {
        using TargetObject = typename std::conditional_t<toHandle, detail::ptr_pointee<ToRaw>,
                                                          std::type_identity<ToRaw>>::type;
        static_assert(is_object<TargetObject>, "cast target must derive from rtti::Object");

        ObjectPtr object;
        if constexpr (fromHandle)
        {
            object = ObjectPtr(from);
        }
        else if constexpr (fromObjectRawPointer)
        {
            if (!from)
            {
                setOk(true);
                return Ptr<TargetObject>();
            }
            if (!from->GetRttiType()->template IsAssignableTo<TargetObject>())
            {
                setOk(false);
                return Ptr<TargetObject>();
            }
            ObjectPtr native = PtrFromRaw(static_cast<TargetObject*>(from));
            setOk(true);
            return PtrCast<TargetObject>(native);
        }
        else
        {
            object = Box(from);
        }

        Ptr<TargetObject> result = detail::CastObject<TargetObject>(object, ok);
        setOk(ok);
        return result;
    }
    else if constexpr (toObjectRawPointer)
    {
        using TargetObject = std::remove_pointer_t<ToRaw>;
        static_assert(is_object<TargetObject>, "cast target must derive from rtti::Object");
        if constexpr (fromHandle)
        {
            ObjectPtr object = ObjectPtr(from);
            if (!object)
            {
                setOk(true);
                return static_cast<TargetObject*>(nullptr);
            }
            if (!type_of<TargetObject>()->IsAssignableFrom(object->GetRttiType()))
            {
                setOk(false);
                return static_cast<TargetObject*>(nullptr);
            }
            ObjectPtr resolved = object;
            if (object->GetRttiType()->IsDynamicType() && !type_of<TargetObject>()->IsDynamicType() &&
                type_of<TargetObject>() != type_of<Object>())
            {
                resolved = object->GetEmbeddedObject(type_of<TargetObject>());
                if (!resolved)
                {
                    setOk(false);
                    return static_cast<TargetObject*>(nullptr);
                }
            }
            setOk(true);
            return static_cast<TargetObject*>(RawFromPtr(resolved));
        }
        else if constexpr (fromObjectRawPointer)
        {
            if (!from)
            {
                setOk(true);
                return static_cast<TargetObject*>(nullptr);
            }
            if (!from->GetRttiType()->template IsAssignableTo<TargetObject>())
            {
                setOk(false);
                return static_cast<TargetObject*>(nullptr);
            }
            setOk(true);
            return static_cast<TargetObject*>(from);
        }
        else
        {
            static_assert(detail::always_false_v<ToRaw>, "cannot cast a value to an object pointer");
        }
    }
    else if constexpr (toVoidPointer)
    {
        if constexpr (fromHandle)
        {
            const ObjectPtr object = ObjectPtr(from);
            if (!object)
            {
                setOk(true);
                return static_cast<void*>(nullptr);
            }
            setOk(true);
            if (object->GetRttiType()->IsValueType())
                return Unbox<void*>(object);
            return static_cast<void*>(RawFromPtr(object));
        }
        else if constexpr (fromObjectRawPointer)
        {
            setOk(true);
            return static_cast<void*>(from);
        }
        else
        {
            setOk(false);
            return static_cast<void*>(nullptr);
        }
    }
    else
    {
        using ValueType = std::remove_pointer_t<ToRaw>;
        if constexpr (fromObjectRawPointer)
        {
            static_assert(detail::always_false_v<ToRaw>, "cannot cast an object pointer to a value type");
        }
        else if constexpr (fromHandle)
        {
            const ObjectPtr object = ObjectPtr(from);
            if (!object)
            {
                setOk(false);
                if constexpr (std::is_pointer_v<ToRaw>)
                    return static_cast<ToRaw>(nullptr);
                else
                    return ValueType{};
            }

            Type* actual = object->GetRttiType();
            if (actual == type_of<ToRaw>() ||
                (std::is_pointer_v<ToRaw> && actual == type_of<ValueType>()) ||
                (!std::is_pointer_v<ToRaw> && actual == type_of<ValueType>()))
            {
                setOk(true);
                return Unbox<ToRaw>(object);
            }

            ObjectPtr converted;
            if (!Type::Convert(object, type_of<ValueType>(), converted))
            {
                setOk(false);
                if constexpr (std::is_pointer_v<ToRaw>)
                    return static_cast<ToRaw>(nullptr);
                else
                    return ValueType{};
            }
            setOk(true);
            return Unbox<ToRaw>(converted);
        }
        else
        {
            if constexpr (std::is_pointer_v<ToRaw>)
            {
                static_assert(detail::always_false_v<ToRaw>, "cannot cast a value to a pointer");
                setOk(false);
                return static_cast<ToRaw>(nullptr);
            }
            else if constexpr (std::is_convertible_v<From, ValueType>)
            {
                setOk(true);
                return ValueType(from);
            }
            else
            {
                ObjectPtr boxed = Box(from);
                ObjectPtr converted;
                if (!Type::Convert(boxed, type_of<ValueType>(), converted))
                {
                    setOk(false);
                    return ValueType{};
                }
                setOk(true);
                return Unbox<ToRaw>(converted);
            }
        }
    }
}

template <class To, class From>
inline auto cast_or_throw(const From& from)
{
    bool ok = false;
    auto result = cast<To>(from, &ok);
    if (!ok)
    {
#if RTTI_HAS_EXCEPTIONS
        RethrowLastError();
#else
        return result;
#endif
    }
    return result;
}

template <class To, class From>
inline bool try_cast(const From& from, To& out)
{
    static_assert(!std::is_reference_v<To>, "try_cast requires a non-reference output type");
    bool ok = false;
    To result = cast<To>(from, &ok);
    if (!ok)
        return false;
    out = std::move(result);
    return true;
}


template <typename T>
inline void RegisterTypeInterfaces(Type* type)
{
    if constexpr (requires { typename T::RTTI_INTERFACES; })
    {
        []<typename... Interfaces>(InterfaceList<Interfaces...>*, Type* target)
        {
            (target->AddInterface(type_of<Interfaces>()), ...);
        }(static_cast<typename T::RTTI_INTERFACES*>(nullptr), type);
    }
}

} // namespace rtti
