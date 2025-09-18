#pragma once
#include "System.h"
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"

namespace
{
template <typename Tuple, typename Iterator, std::size_t... I>
static Tuple MakeArgsImpl(const Iterator& p, std::index_sequence<I...>)
{
    return Tuple(rtti::cast<std::tuple_element_t<I, Tuple>>(p[I])...);
}

// Convert each ObjectPtr in the Iterator to the type corresponding to Args and place it in a tuple.
template <typename... Args, typename Iterator>
static auto MakeArgs(const Iterator& p)
{
    static_assert(std::is_same_v<rtti::remove_cr<decltype(p[0])>, rtti::ObjectPtr>, "Iterator type must be ObjectPtr");

    using Tuple = std::tuple<rtti::remove_cr<Args>...>;
    return MakeArgsImpl<Tuple>(p, std::make_index_sequence<sizeof...(Args)>{});
}

template <typename T>
static rtti::ParameterInfo GetParameterInfo()
{
    static_assert(std::is_reference_v<T> == std::is_const_v<std::remove_reference_t<T>>);

    return {rtti::type_of<T>(), std::is_reference_v<T>, std::is_const_v<std::remove_reference_t<T>>};
}
} // namespace

namespace rtti
{
class MethodBase : public Attributable
{
private:
    std::string name;
    Type* owner;
    Type* rettype;
    std::vector<ParameterInfo> arguments;

protected:
    MethodBase(Type* owner, const std::string& name, Type* rettype, std::initializer_list<ParameterInfo> arguments, const std::map<size_t, std::any>& attributes)
        : Attributable(attributes)
        , name(name)
        , owner(owner)
        , rettype(rettype)
        , arguments(arguments)
    {
    }

public:
    const std::string& GetName() const { return name; }

    Type* OwnerType() const { return owner; }

    Type* ReturnType() const { return rettype; }

    const std::vector<ParameterInfo>& GetParameters() { return arguments; }
};

class ConstructorInfo : public MethodBase
{
private:
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func;

protected:
    ConstructorInfo(Type* owner, std::initializer_list<ParameterInfo> arguments, std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func, const std::map<size_t, std::any>& attributes)
        : MethodBase(owner, ".ctor", owner, arguments, attributes)
        , func(func)
    {
    }

public:
    ObjectPtr Invoke(const std::vector<ObjectPtr>& args)
    {
        return func(args);
    }

    template <typename... Args>
    ObjectPtr Invoke(Args... arguments)
    {
        return func({Box(std::forward<Args>(arguments))...});
    }

    template <typename... Args>
    static ConstructorInfo* Register(Type* host, ObjectPtr (*f)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func = [=](const std::vector<ObjectPtr>& args) -> ObjectPtr
        {
            if (args.size() != sizeof...(Args))
            {
                RTTI_ERROR((std::string("requires ") + std::to_string(sizeof...(Args)) + std::string(" parameters, but actually has ") + std::to_string(args.size())).c_str());
                return nullptr;
            }

            auto args_tuple = MakeArgs<Args...>(args);
            return std::apply([=](Args... args)
                              { return f(std::forward<Args>(args)...); },
                              args_tuple);
        };
        return new ConstructorInfo(host, {GetParameterInfo<Args>()...}, func, attributes);
    }

    template <typename Host, typename... Args>
    static ConstructorInfo* Register(ObjectPtr (*f)(Args...), const std::map<size_t, std::any>& attributes = {})
    {
        return Register(type_of<Host>(), f, attributes);
    }
};

class MethodInfo : public MethodBase
{
private:
    enum class MethodFlags
    {
        None = 0,
        Static = 1
    };

    MethodFlags m_flags = MethodFlags::None;
    std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> m_func;

public:
    bool IsStatic() const
    {
        return ((int32_t)m_flags & (int32_t)MethodFlags::Static) != 0;
    }

    ObjectPtr Invoke(const ObjectPtr& target, const std::vector<ObjectPtr>& args)
    {
        return m_func(target, args);
    }

    template <typename... Args>
    ObjectPtr Invoke(const ObjectPtr& target, Args... args)
    {
        return m_func(target, {Box(args)...});
    }

    MethodInfo(Type* owner, const std::string& name, MethodFlags flags, Type* rettype, std::initializer_list<ParameterInfo> arguments, std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> func, const std::map<size_t, std::any>& attributes)
        : MethodBase(owner, name, rettype, arguments, attributes)
        , m_flags(flags)
        , m_func(func)
    {
    }

    template <typename Host, typename FUNC, typename RET, typename... Args>
    static MethodInfo* Register(const std::string& name, FUNC f, const std::map<size_t, std::any>& attributes = {})
    {
        std::function<ObjectPtr(const ObjectPtr&, const std::vector<ObjectPtr>&)> func = [=](const ObjectPtr& target, const std::vector<ObjectPtr>& args) -> ObjectPtr
        {
            if (args.size() < sizeof...(Args))
            {
                RTTI_ERROR((std::string("requires ") + std::to_string(sizeof...(Args)) + std::string(" parameters, but actually has ") + std::to_string(args.size())).c_str());
                return nullptr;
            }

            using SelfType = std::remove_pointer_t<Host>;

            SelfType* self = nullptr;
            if constexpr (std::is_member_function_pointer_v<FUNC> || std::is_member_object_pointer_v<FUNC>)
            {
                // call menber function / field
                if (target == nullptr)
                {
                    RTTI_ERROR(std::string("target cannot be nullptr").c_str());
                    return nullptr;
                }

                Type* target_type = target->GetRttiType();

                if constexpr (is_object<SelfType>)
                {
                    if (!type_of<SelfType>()->IsAssignableFrom(target))
                    {
                        RTTI_ERROR((std::string("target must be ") + GetTypeName<SelfType>()).c_str());
                        return nullptr;
                    }
                    self = static_cast<SelfType*>(RTTI_RAW_FROM_PTR(target));
                }
                else
                {
                    if (type_of<SelfType>() != target_type && type_of<SelfType*>() != target_type)
                    {
                        RTTI_ERROR((std::string("target must be ") + GetTypeName<SelfType>()).c_str());
                        return nullptr;
                    }
                    self = Unbox<SelfType*>(target);
                }
            }

            auto args_tuple = MakeArgs<Args...>(args);
            if constexpr (std::is_void_v<RET>)
            {
                if constexpr (std::is_member_function_pointer_v<FUNC>)
                {
                    std::apply([=](Args... a)
                               { std::invoke(f, self, std::forward<Args>(a)...); },
                               args_tuple);
                }
                else if constexpr (std::is_member_object_pointer_v<FUNC>)
                {
                    static_assert(sizeof...(Args) == 1);
                    (self->*f) = std::get<0>(args_tuple);
                }
                else
                {
                    std::apply([=](Args... a)
                               { std::invoke(f, std::forward<Args>(a)...); },
                               args_tuple);
                }
                return nullptr;
            }
            else
            {
                if constexpr (std::is_member_function_pointer_v<FUNC>)
                {
                    return std::apply([=](Args... a)
                                      { return cast<ObjectPtr>(std::invoke(f, self, std::forward<Args>(a)...)); },
                                      args_tuple);
                }
                else if constexpr (std::is_member_object_pointer_v<FUNC>)
                {
                    static_assert(sizeof...(Args) == 0);
                    return cast<ObjectPtr>(self->*f);
                }
                else
                {
                    return std::apply([=](Args... a)
                                      { return cast<ObjectPtr>(std::invoke(f, std::forward<Args>(a)...)); },
                                      args_tuple);
                }
            }
        };

        Type* rettype;
        if constexpr (std::is_void_v<RET>)
        {
            rettype = nullptr;
        }
        else
        {
            rettype = type_of<RET>();
        }

        return new MethodInfo(type_of<Host>(), name, std::is_member_function_pointer_v<FUNC> ? MethodFlags::None : MethodFlags::Static, rettype, {GetParameterInfo<Args>()...}, func, attributes);
    }
};
} // namespace rtti