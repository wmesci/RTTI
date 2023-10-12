#include "Type.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"

using namespace rtti;

Type* header = nullptr;

Type::Type(size_t size, bool is_trivially_copyable, Type* baseType)
    : Attributable()
    , m_name("Object"s)
    , m_size(size)
    , is_trivially_copyable(is_trivially_copyable)
    , m_baseType(baseType)
    , next(header)
{
    header = this;
}

// Attribute* Type::GetAttributeImpl(const std::type_info& type) const
//{
//     auto curType = this;
//     while (curType != nullptr)
//     {
//         for (const auto& attr : curType->Attributes)
//         {
//             if ((curType == this || attr->Inheritable) && typeid(*attr) == type)
//                 return attr;
//         }
//
//         curType = curType->GetBaseType();
//     }
//     return nullptr;
// }

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

bool Type::CanCast(Type* type) const
{
    return type == this || this->IsSubClassOf(type);
}

bool Type::IsBoxedType() const
{
    return GetBaseType() == typeof(ObjectBox);
}

bool Type::IsEnum() const
{
    return UnderlyingType != nullptr && EnumValues.size() > 0;
}

const std::vector<EnumInfo>& Type::GetEnumInfos() const
{
    return EnumValues;
}

bool Type::GetEnumInfo(const std::string& name, EnumInfo* pInfo) const
{
    assert(IsEnum());
    for (auto&& i : EnumValues)
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
    assert(IsEnum());
    for (auto&& i : EnumValues)
    {
        if (i.number == number)
        {
            *pInfo = i;
            return true;
        }
    }
    return false;
}

Type* Type::GetEnumUnderlyingType() const
{
    return UnderlyingType;
}

bool Type::IsCompatible(Type* type) const
{
    if (IsBoxedType())
        return this == type;
    else
        return this == type || type->IsSubClassOf(const_cast<Type*>(this));
}

bool Type::IsCompatible(Object* obj) const
{
    if (obj == nullptr)
        return !IsBoxedType();

    return IsCompatible(obj->GetType());
}

ObjectPtr Type::CreateInstance(const std::vector<ObjectPtr>& args) const
{
    for (int i = 0; i < Constructors.size(); i++)
    {
        auto ctor = Constructors[i];
        if (ctor->GetParameters().size() == args.size())
        {
            bool ok = true;
            for (int j = 0; j < args.size(); j++)
            {
                auto pt = ctor->GetParameters()[j];
                if (pt.Type->IsBoxedType())
                {
                    if (args[j] == nullptr || args[j]->GetType() != pt.Type)
                    {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    if (args[j] != nullptr && !args[j]->GetType()->CanCast(pt.Type))
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
    return Constructors;
}

ConstructorInfo* Type::GetConstructor() const
{
    for (int i = 0; i < Constructors.size(); i++)
    {
        auto ctor = Constructors[i];
        if (0 == ctor->GetParameters().size())
        {
            return ctor;
        }
    }
    return nullptr;
}

ConstructorInfo* Type::GetConstructor(std::initializer_list<Type*> args) const
{
    for (int i = 0; i < Constructors.size(); i++)
    {
        auto ctor = Constructors[i];
        if (args.size() == ctor->GetParameters().size())
        {
            bool paramMatch = true;
            for (int i = 0; i < ctor->GetParameters().size(); i++)
            {
                auto p = ctor->GetParameters()[i];
                if (p.Type != args.begin()[i])
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
    for (int i = 0; i < Constructors.size(); i++)
    {
        auto ctor = Constructors[i];
        if (args.size() == ctor->GetParameters().size())
        {
            bool paramMatch = true;
            for (int i = 0; i < ctor->GetParameters().size(); i++)
            {
                const auto& p = ctor->GetParameters()[i];
                const auto& ap = args.begin()[i];
                if (p.Type != ap.Type || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
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
        methods.insert(methods.end(), curType->Methods.begin(), curType->Methods.end());
        curType = curType->GetBaseType();
    }
    return methods;
}

MethodInfo* Type::GetMethod(const std::string& name) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (int i = 0; i < curType->Methods.size(); i++)
        {
            auto m = curType->Methods[i];
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
        for (int i = 0; i < curType->Methods.size(); i++)
        {
            auto m = curType->Methods[i];
            if (m->GetName() == name && args.size() == m->GetParameters().size())
            {
                bool paramMatch = true;
                for (int i = 0; i < m->GetParameters().size(); i++)
                {
                    auto p = m->GetParameters()[i];
                    if (p.Type != args.begin()[i])
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
        for (int i = 0; i < curType->Methods.size(); i++)
        {
            auto m = curType->Methods[i];
            if (m->GetName() == name && args.size() == m->GetParameters().size())
            {
                bool paramMatch = true;
                for (int i = 0; i < m->GetParameters().size(); i++)
                {
                    const auto& p = m->GetParameters()[i];
                    const auto& ap = args.begin()[i];
                    if (p.Type != ap.Type || p.IsConst != ap.IsConst || p.IsRef != ap.IsRef)
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
        rets.insert(rets.end(), curType->Properties.begin(), curType->Properties.end());
        curType = curType->GetBaseType();
    }
    return rets;
}

PropertyInfo* Type::GetProperty(const std::string& name) const
{
    auto curType = this;
    while (curType != nullptr)
    {
        for (int i = 0; i < curType->Properties.size(); i++)
        {
            if (curType->Properties[i]->GetName() == name)
                return curType->Properties[i];
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