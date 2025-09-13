#include "Type.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"

using namespace rtti;

Type* header = nullptr;

Type::Type(const std::string& name, size_t size, TypeFlags flags, Type* underlyingType, Type* baseType, const std::map<size_t, std::any>& attributes)
    : Attributable(attributes)
    , m_name(name)
    , m_size((uint32_t)size)
    , m_flags(flags)
    , m_baseType(baseType)
    , m_underlyingType(underlyingType)
    , next(header)
{
    header = this;
}

const std::string& Type::GetName() const
{
    return m_name;
}

bool Type::IsSubClassOf(Type* type) const
{
    auto t = this;
    do
    {
        t = t->GetBaseType();
        if (t == type)
            return true;
    } while (t != nullptr);
    return false;
}

bool Type::IsValueType() const
{
    return GetBaseType() == type_of<ObjectBox>();
}

bool Type::IsEnum() const
{
    return HasFlag(TypeFlags::Enum);
}

bool Type::IsPointer() const
{
    return HasFlag(TypeFlags::Pointer);
}

bool Type::HasFlag(TypeFlags flag) const
{
    return ((int)m_flags & (int)flag) != 0;
}

const std::vector<EnumInfo>& Type::GetEnumInfos() const
{
    return m_enumValues;
}

bool Type::GetEnumInfo(const std::string& name, EnumInfo* pInfo) const
{
    for (auto&& i : m_enumValues)
    {
        if (i.name == name)
        {
            *pInfo = i;
            return true;
        }
    }
    return false;
}

bool Type::GetEnumInfo(const int64_t& number, EnumInfo* pInfo) const
{
    for (auto&& i : m_enumValues)
    {
        if (i.number == number)
        {
            *pInfo = i;
            return true;
        }
    }
    return false;
}

bool Type::IsAssignableTo(Type* type) const
{
    if (type == nullptr)
        return false;

    if (this == type)
        return true;

    if (IsValueType())
        return type == type_of<Object>();
    else
        return this->IsSubClassOf(type);
}

bool Type::IsAssignableFrom(Type* type) const
{
    if (type == nullptr)
        return false;

    return type->IsAssignableTo(const_cast<Type*>(this));
}

bool Type::IsAssignableFrom(const ObjectPtr& obj) const
{
    if (obj == nullptr)
        return !IsValueType();

    return IsAssignableFrom(obj->GetRttiType());
}

bool Type::CanConvertTo(Type* targetType) const
{
    Type* sourceType = const_cast<Type*>(this);

    if (targetType->IsAssignableFrom(sourceType))
        return true;

    if (sourceType->IsPointer() && targetType == type_of<void*>())
        return true;

    if (sourceType->IsEnum() && targetType->HasFlag(TypeFlags::Integral))
        return true;

    if (sourceType->HasFlag(TypeFlags::Integral) && targetType->IsEnum())
        return true;

    for (auto&& i : targetType->m_constructors)
    {
        if (i->GetParameters().size() == 1 && i->GetParameters()[0].ParameterType->IsAssignableFrom(sourceType))
            return true;
    }

    for (auto&& i : m_typeConvertors)
    {
        if (i.TargetType->IsAssignableTo(targetType))
            return true;
    }

    return false;
}

bool Type::Convert(const ObjectPtr& obj, Type* targetType, ObjectPtr& target)
{
    if (obj == nullptr)
    {
        if (!targetType->IsValueType())
        {
            target = nullptr;
            return true;
        }
        return false;
    }

    Type* sourceType = obj->GetRttiType();

    if (targetType->IsAssignableFrom(sourceType))
    {
        target = obj;
        return true;
    }

    if (sourceType->IsPointer() && targetType == type_of<void*>())
    {
        target = rtti::Box(rtti::Unbox<void*>(obj));
        return true;
    }

    for (auto&& i : targetType->m_constructors)
    {
        if (i->GetParameters().size() == 1 && i->GetParameters()[0].ParameterType->IsAssignableFrom(sourceType))
        {
            target = targetType->CreateInstance({obj});
            if (target != nullptr)
                return true;
        }
    }

    for (auto&& i : sourceType->m_typeConvertors)
    {
        if (i.TargetType->IsAssignableTo(targetType))
        {
            return i.Convert(obj, i.TargetType, target);
        }
    }

    if (sourceType->IsEnum() && targetType->HasFlag(TypeFlags::Integral))
    {
        ObjectPtr underlyingObj = nullptr;
        return Convert(obj, sourceType->GetUnderlyingType(), underlyingObj) && Convert(underlyingObj, targetType, target);
    }

    if (sourceType->HasFlag(TypeFlags::Integral) && targetType->IsEnum())
    {
        ObjectPtr underlyingObj = nullptr;
        return Convert(obj, sourceType->GetUnderlyingType(), underlyingObj) && Convert(underlyingObj, targetType, target);
    }

    return false;
}

bool Type::IsComparable(Type* leftType, Type* rightType)
{
    if (leftType == nullptr)
    {
        if (rightType == nullptr)
            return true;
        else
            return Type::IsComparable(rightType, leftType);
    }

    for (auto&& i : leftType->m_objectComparers)
    {
        if ((i.TargetType == nullptr && rightType == nullptr) || (i.TargetType != nullptr && rightType != nullptr && rightType->IsAssignableTo(i.TargetType)))
        {
            return true;
        }
    }

    if (rightType != nullptr)
    {
        for (auto&& i : rightType->m_objectComparers)
        {
            if (i.TargetType != nullptr && leftType->IsAssignableTo(i.TargetType))
            {
                return true;
            }
        }
    }

    return false;
}

CompareResult Type::Compare(const ObjectPtr& left, const ObjectPtr& right)
{
    if (left == nullptr)
    {
        if (right == nullptr)
            return CompareResult::Equals;
        else
            return Type::Compare(right, left);
    }

    Type* leftType = left->GetRttiType();
    Type* rightType = right != nullptr ? right->GetRttiType() : nullptr;
    for (auto&& i : leftType->m_objectComparers)
    {
        if ((i.TargetType == nullptr && rightType == nullptr) || (i.TargetType != nullptr && rightType != nullptr && rightType->IsAssignableTo(i.TargetType)))
        {
            return i.Compare(left, right);
        }
    }

    if (right != nullptr)
    {
        for (auto&& i : rightType->m_objectComparers)
        {
            if (i.TargetType != nullptr && leftType->IsAssignableTo(i.TargetType))
            {
                return i.Compare(right, left);
            }
        }
    }

    return CompareResult::Failed;
}

ObjectPtr Type::CreateInstance(const std::vector<ObjectPtr>& args) const
{
    for (size_t i = 0; i < m_constructors.size(); i++)
    {
        auto ctor = m_constructors[i];
        if (ctor->GetParameters().size() == args.size())
        {
            bool ok = true;
            for (size_t j = 0; j < args.size(); j++)
            {
                auto pt = ctor->GetParameters()[j];
                if (pt.ParameterType->IsValueType())
                {
                    if (args[j] == nullptr || args[j]->GetRttiType() != pt.ParameterType)
                    {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    if (args[j] != nullptr && !args[j]->GetRttiType()->CanConvertTo(pt.ParameterType))
                    {
                        ok = false;
                        break;
                    }
                }
            }
            if (ok)
            {
                return ctor->Invoke(args);
            }
        }
    }
    return nullptr;
}

const std::vector<ConstructorInfo*>& Type::GetConstructors() const
{
    return m_constructors;
}

ConstructorInfo* Type::GetConstructor() const
{
    for (size_t i = 0; i < m_constructors.size(); i++)
    {
        auto ctor = m_constructors[i];
        if (0 == ctor->GetParameters().size())
        {
            return ctor;
        }
    }
    return nullptr;
}

ConstructorInfo* Type::GetConstructor(std::initializer_list<Type*> args) const
{
    for (size_t i = 0; i < m_constructors.size(); i++)
    {
        auto ctor = m_constructors[i];
        if (args.size() == ctor->GetParameters().size())
        {
            bool paramMatch = true;
            for (size_t i = 0; i < ctor->GetParameters().size(); i++)
            {
                auto p = ctor->GetParameters()[i];
                if (p.ParameterType != args.begin()[i])
                {
                    paramMatch = false;
                    break;
                }
            }
            if (paramMatch)
                return ctor;
        }
    }
    return nullptr;
}

ConstructorInfo* Type::GetConstructor(std::initializer_list<ParameterInfo> args) const
{
    for (size_t i = 0; i < m_constructors.size(); i++)
    {
        auto ctor = m_constructors[i];
        if (args.size() == ctor->GetParameters().size())
        {
            bool paramMatch = true;
            for (size_t i = 0; i < ctor->GetParameters().size(); i++)
            {
                const auto& p = ctor->GetParameters()[i];
                const auto& ap = args.begin()[i];
                if (p.ParameterType != ap.ParameterType || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
                {
                    paramMatch = false;
                    break;
                }
            }
            if (paramMatch)
                return ctor;
        }
    }
    return nullptr;
}

std::vector<MethodInfo*> Type::GetMethods() const
{
    std::vector<MethodInfo*> methods;
    const Type* curType = this;
    while (curType != nullptr)
    {
        methods.insert(methods.end(), curType->m_methods.begin(), curType->m_methods.end());
        curType = curType->GetBaseType();
    }
    return methods;
}

MethodInfo* Type::GetMethod(const std::string& name) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (size_t i = 0; i < curType->m_methods.size(); i++)
        {
            auto m = curType->m_methods[i];
            if (m->GetName() == name)
            {
                return m;
            }
        }
        curType = curType->GetBaseType();
    }
    return nullptr;
}

MethodInfo* Type::GetMethod(const std::string& name, std::initializer_list<Type*> args) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (size_t i = 0; i < curType->m_methods.size(); i++)
        {
            auto m = curType->m_methods[i];
            if (m->GetName() == name && args.size() == m->GetParameters().size())
            {
                bool paramMatch = true;
                for (size_t i = 0; i < m->GetParameters().size(); i++)
                {
                    auto p = m->GetParameters()[i];
                    if (p.ParameterType != args.begin()[i])
                    {
                        paramMatch = false;
                        break;
                    }
                }

                if (paramMatch)
                    return m;
            }
        }
        curType = curType->GetBaseType();
    }
    return nullptr;
}

MethodInfo* Type::GetMethod(const std::string& name, std::initializer_list<ParameterInfo> args) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (size_t i = 0; i < curType->m_methods.size(); i++)
        {
            auto m = curType->m_methods[i];
            if (m->GetName() == name && args.size() == m->GetParameters().size())
            {
                bool paramMatch = true;
                for (size_t i = 0; i < m->GetParameters().size(); i++)
                {
                    const auto& p = m->GetParameters()[i];
                    const auto& ap = args.begin()[i];
                    if (p.ParameterType != ap.ParameterType || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
                    {
                        paramMatch = false;
                        break;
                    }
                }

                if (paramMatch)
                    return m;
            }
        }
        curType = curType->GetBaseType();
    }
    return nullptr;
}

std::vector<PropertyInfo*> Type::GetProperties() const
{
    std::vector<PropertyInfo*> rets;
    const Type* curType = this;
    while (curType != nullptr)
    {
        rets.insert(rets.end(), curType->m_properties.begin(), curType->m_properties.end());
        curType = curType->GetBaseType();
    }
    return rets;
}

PropertyInfo* Type::GetProperty(const std::string& name) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (size_t i = 0; i < curType->m_properties.size(); i++)
        {
            if (curType->m_properties[i]->GetName() == name)
                return curType->m_properties[i];
        }
        curType = curType->GetBaseType();
    }
    return nullptr;
}

Type* Type::Find(const std::string& name)
{
    Type* cur = header;
    while (cur != nullptr)
    {
        if (name == cur->GetName())
            return cur;
        cur = cur->next;
    }
    return nullptr;
}

void Type::ForEach(const std::function<void(Type*)>& callback)
{
    Type* cur = header;
    while (cur != nullptr)
    {
        callback(cur);
        cur = cur->next;
    }
}