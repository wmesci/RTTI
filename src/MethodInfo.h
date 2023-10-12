#pragma once
#include "System.h"
#include "Object.h"
#include "ObjectBox.h"
#include "Attributable.h"

namespace Albert
{
template <typename T, bool isobj = std::is_convertible_v<remove_cr<T>, ObjectPtr>>
struct wrap_reference
{
    using type = T;
};

template <typename T>
struct wrap_reference<T, true>
{
    using type = remove_cr<T>;
};

template <typename T>
struct wrap_reference<T&, false>
{
    using type = typename std::reference_wrapper<T>;
};

template <typename T>
struct wrap_reference<const T&, false>
{
    using type = typename std::reference_wrapper<const T>;
};

// template<typename T>
// struct wrap_reference<T&&>
//{
//     using type = typename std::reference_wrapper<T>;
// };

template <typename T>
using wrap_reference_t = typename wrap_reference<T>::type;

template <typename T>
static T UnboxArg(const ObjectPtr& p)
{
    using unref_t = std::unwrap_reference_t<T>;

    if constexpr (std::is_convertible_v<unref_t, Object>)
    {
        return Unbox<remove_cr<unref_t>>(p);
    }
    else if constexpr (std::is_reference_v<unref_t>)
    {
        if constexpr (std::is_same_v<std::reference_wrapper<const std::remove_reference_t<unref_t>>, T>)
            return (const unref_t&)Unbox<remove_cr<unref_t>&>(p);
        else
            return (unref_t&)Unbox<remove_cr<unref_t>&>(p);
    }
    else
    {
        return Unbox<T>(p);
    }
}

template <typename Tuple, typename Iterator, std::size_t... I>
static Tuple MakeArgsImpl(Iterator p, std::index_sequence<I...>)
{
    return Tuple(UnboxArg<std::tuple_element_t<I, Tuple>>(p[I])...);
}

// 将 Iterator 里的 Object* 逐个转换为 Args 对应的类型，并放在 tuple 里
template <typename Iterator, typename... Args>
static auto MakeArgs(Iterator p)
{
    static_assert(std::is_same_v<remove_cr<decltype(p[0])>, ObjectPtr>, "Iterator type must be ObjectPtr");

    using Tuple = std::tuple<wrap_reference_t<Args>...>;
    return MakeArgsImpl<Tuple>(p, std::make_index_sequence<sizeof...(Args)>{});
}

class MethodBase : public Attributable
{
private:
    std::string name;
    Type* owner;
    Type* rettype;
    std::vector<ParameterInfo> arguments;
    const std::vector<Attribute*> attributes;

protected:
    MethodBase(Type* owner, const std::string& name, Type* rettype, std::initializer_list<ParameterInfo> arguments, std::initializer_list<Attribute*> attributes)
        : Attributable()
        , name(name)
        , owner(owner)
        , rettype(rettype)
        , arguments(arguments)
        , attributes(attributes)
    {
    }

    // Attribute* GetAttributeImpl(const std::type_info& type) const override
    //{
    //     for (const auto& attr : attributes)
    //     {
    //         if (typeid(*attr) == type)
    //             return attr;
    //     }
    //     return nullptr;
    // }

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
    ConstructorInfo(Type* owner, std::initializer_list<ParameterInfo> arguments, std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func, std::initializer_list<Attribute*> attributes)
        : MethodBase(owner, ".ctor"s, owner, arguments, attributes)
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

    template <typename Host, typename... Args>
    static ConstructorInfo* Register(ObjectPtr (*f)(Args...), std::initializer_list<Attribute*> attributes)
    {
        std::function<ObjectPtr(const std::vector<ObjectPtr>&)> func = [=](const std::vector<ObjectPtr>& args) -> ObjectPtr
        {
            if (args.size() != sizeof...(Args))
            {
                // Log(LogLevel::Error, "args.size() != "s + sizeof...(Args));
                return nullptr;
            }

            auto args_tuple = MakeArgs<std::vector<ObjectPtr>, Args...>(args);
            return std::apply([=](Args... args)
                              { return f(std::forward<Args>(args)...); },
                              args_tuple);
        };
        return new ConstructorInfo(typeof(Host), {GetParameterInfo<Args>()...}, func, attributes);
    }
};

class MethodInfo : public MethodBase
{
private:
    std::function<ObjectPtr(Object*, const std::vector<ObjectPtr>&)> func;

protected:
    MethodInfo(Type* owner, const std::string& name, Type* rettype, std::initializer_list<ParameterInfo> arguments, std::function<ObjectPtr(Object*, const std::vector<ObjectPtr>&)> func, std::initializer_list<Attribute*> attributes)
        : MethodBase(owner, name, rettype, arguments, attributes)
        , func(func)
    {
    }

    template <typename Host, typename FUNC, typename RET, typename... Args>
    static MethodInfo* RegisterImpl(const std::string& name, FUNC f, std::initializer_list<Attribute*> attributes)
    {
        static_assert(std::is_member_function_pointer_v<FUNC>);

        std::function<ObjectPtr(Object*, const std::vector<ObjectPtr>&)> func = [=](Object* target, const std::vector<ObjectPtr>& args) -> ObjectPtr
        {
            if (args.size() != sizeof...(Args))
            {
                // Log(LogLevel::Error, "args.size() != "s + sizeof...(Args));
                return nullptr;
            }

            Host* self = nullptr;
            if constexpr (is_object<Host>)
                self = static_cast<Host*>(target);
            else
                self = Unbox<Host*>(target);

            auto args_tuple = MakeArgs<std::vector<ObjectPtr>, Args...>(args);
            if constexpr (std::is_void_v<RET>)
            {
                std::apply([=](Args... a)
                           { (self->*f)(std::forward<Args>(a)...); },
                           args_tuple);
                return nullptr;
            }
            else
            {
                return std::apply([=](Args... a)
                                  { return Box((self->*f)(std::forward<Args>(a)...)); },
                                  args_tuple);
            }
        };

        Type* rettype;
        if constexpr (std::is_void_v<RET>)
        {
            rettype = nullptr;
        }
        else
        {
            rettype = typeof(RET);
        }

        return new MethodInfo(typeof(Host), name, rettype, {GetParameterInfo<Args>()...}, func, attributes);
    }

public:
    ObjectPtr Invoke(Object* target, const std::vector<ObjectPtr>& args)
    {
        return func(target, args);
    }

    ObjectPtr Invoke(const ObjectPtr& target, const std::vector<ObjectPtr>& args)
    {
        return func(target.get(), args);
    }

    template <typename... Args>
    ObjectPtr Invoke(Object* target, Args... args)
    {
        return func(target, {Box(args)...});
    }

    template <typename... Args>
    ObjectPtr Invoke(const ObjectPtr& target, Args... args)
    {
        return func(target.get(), {Box(args)...});
    }

    template <typename Host, typename RET, typename... Args>
    static MethodInfo* Register(const std::string& name, RET (Host::*FUNC)(Args...), std::initializer_list<Attribute*> attributes)
    {
        return RegisterImpl<Host, decltype(FUNC), RET, Args...>(name, FUNC, attributes);
    }

    template <typename Host, typename RET, typename... Args>
    static MethodInfo* Register(const std::string& name, RET (Host::*FUNC)(Args...) const, std::initializer_list<Attribute*> attributes)
    {
        return RegisterImpl<Host, decltype(FUNC), RET, Args...>(name, FUNC, attributes);
    }
};

#define METHOD(name, ...) MethodInfo::Register(#name##s, &HOST::name, {__VA_ARGS__})

template <class T, class... Args>
ObjectPtr constructor(Args... args)
{
    if constexpr (is_object<T>)
        return std::make_shared<T>(std::forward<Args>(args)...);
    else
        return Box(T(std::forward<Args>(args)...));
}

#define CONSTRUCTOR(...) ConstructorInfo::Register<HOST, __VA_ARGS__>(&constructor<HOST, __VA_ARGS__>, {})
} // namespace Albert