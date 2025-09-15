#include "Object.h"
#include "Type.h"
#include "MethodInfo.h"
#include "PropertyInfo.h"

using namespace rtti;

Object::Object()
{
}

Type* Object::GetRttiType() const
{
    return CreateType<Object, void>();
}

size_t Object::GetHashCode() const
{
    return std::hash<const void*>()(this);
}

Ptr<Object> Object::Clone()
{
    auto type = this->GetRttiType();
    if (!type->HasAttribute(ClonableAttribute))
        return nullptr;

#ifdef RTTI_PTR_FROM_RAW
    auto self = RTTI_PTR_FROM_RAW(this);

    auto ctor = type->GetConstructor({type});
    if (ctor != nullptr)
        return ctor->Invoke({self});
#endif

    auto obj = type->CreateInstance({});
    if (obj == nullptr)
        return nullptr;

    for (auto& p : type->GetProperties())
    {
        if (p->CanRead() && p->CanWrite())
        {
            auto cloneOption = p->GetAttribute<CloneOption>(CloneOptionAttribute, CloneOption::None);
            if (cloneOption != CloneOption::None)
            {
                auto v = p->GetValue(self);
                if (cloneOption == CloneOption::Deep)
                {
                    v = v->Clone();
                }

                p->SetValue(obj, v);
            }
        }
    }

    return obj;
}

Object::~Object()
{
}