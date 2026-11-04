#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Attributable.h"
#include "Error.h"
#include "Object.h"
#include "ObjectBox.h"
#include "Reflection.h"
#include "System.h"

namespace rtti
{

namespace detail
{

template <typename>
inline constexpr bool always_false_v = false;

// Detect captureless (stateless) lambdas: only those are implicitly convertible
// to a function pointer. Lambdas with captures SFINAE out.
template <typename T, typename = void>
struct is_stateless_lambda : std::false_type
{
};

template <typename T>
struct is_stateless_lambda<T, std::void_t<decltype(+std::declval<T>())>> : std::true_type
{
};

template <typename FUNC>
constexpr bool is_allowed_method_callable_v =
    std::is_function_v<std::remove_pointer_t<FUNC>> || std::is_member_function_pointer_v<FUNC> ||
    std::is_member_object_pointer_v<FUNC> || is_stateless_lambda<FUNC>::value;

template <typename T>
ParameterInfo ParameterInfoOf()
{
    return { type_of<T>(), std::is_lvalue_reference_v<T>, std::is_const_v<std::remove_reference_t<T>> };
}

template <typename Arg>
struct StoredArgument
{
    using type = Arg;
};

template <typename Arg>
struct StoredArgument<const Arg&>
{
    using type = Arg;
};

template <typename Arg>
using StoredArgumentT = typename StoredArgument<Arg>::type;

template <typename Arg>
Arg CastArgument(const ObjectPtr& object, bool& ok);

template <typename... Args, typename Iterator, std::size_t... I>
std::optional<std::tuple<StoredArgumentT<Args>...>> MakeArgsImpl(const Iterator& p, std::index_sequence<I...>);

template <typename... Args, typename Iterator>
std::optional<std::tuple<StoredArgumentT<Args>...>> MakeArgs(const Iterator& p);

template <typename R>
bool StoreReturn(R&& value, ObjectPtr& out)
{
    using V = remove_const_ref<R>;
    if constexpr (is_ptr_v<V>)
    {
        out = Box(value);
        return true;
    }
    else if constexpr (is_object<V>)
    {
        if constexpr (std::is_pointer_v<V>)
        {
            out = Box(PtrFromRaw(value));
            return true;
        }
        else
        {
            out = Box(PtrFromRaw(const_cast<V*>(std::addressof(value))));
            return true;
        }
    }
    else if constexpr (std::is_pointer_v<V>)
    {
        out = Box(value);
        return true;
    }
    else
    {
        out = Box(std::forward<R>(value));
        return true;
    }
}

inline bool CheckArgumentCount(const char* owner, const char* name, size_t expected, size_t actual)
{
    if (expected == actual)
        return true;
    return Fail(ErrorCode::ArgumentCount, std::string(owner ? owner : "?") + "::" + (name ? name : "?") +
                                               " expects " + std::to_string(expected) + " arguments, got " +
                                               std::to_string(actual));
}

} // namespace detail

class MethodBase : public Attributable
{
protected:
    MethodBase(Type* owner,
               const std::string& name,
               Type* returnType,
               std::initializer_list<ParameterInfo> parameters,
               const std::map<size_t, std::any>& attributes)
        : Attributable(attributes)
        , m_owner(owner)
        , m_name(name)
        , m_returnType(returnType)
    {
        m_parameters.assign(parameters.begin(), parameters.end());
    }

public:
    virtual ~MethodBase() = default;

    const std::string& GetName() const { return m_name; }

    Type* GetOwnerType() const { return m_owner; }

    Type* GetReturnType() const { return m_returnType; }

    const std::vector<ParameterInfo>& GetParameters() const { return m_parameters; }

    // Internal registration hook used by DynamicType.
    void SetOwner(Type* owner) { m_owner = owner; }

protected:
    Type* m_owner = nullptr;
    std::string m_name;
    Type* m_returnType = nullptr;
    std::vector<ParameterInfo> m_parameters;
};

class ConstructorInfo : public MethodBase
{
public:
    using Callable = std::function<bool(const std::vector<ObjectPtr>&, ObjectPtr&)>;

protected:
    ConstructorInfo(Type* owner,
                    std::initializer_list<ParameterInfo> parameters,
                    Callable function,
                    const std::map<size_t, std::any>& attributes)
        : MethodBase(owner, ".ctor", nullptr, parameters, attributes)
        , m_function(std::move(function))
    {
    }

public:
    bool TryInvoke(const std::vector<ObjectPtr>& arguments, ObjectPtr& result) noexcept
    {
#if RTTI_HAS_EXCEPTIONS
        try
        {
            return m_function(arguments, result);
        }
        catch (const Error&)
        {
            return false;
        }
        catch (...)
        {
            return Fail(ErrorCode::TargetInvocation, "constructor threw an exception");
        }
#else
        return m_function(arguments, result);
#endif
    }

    ObjectPtr Invoke(const std::vector<ObjectPtr>& arguments)
    {
        ObjectPtr result;
#if RTTI_HAS_EXCEPTIONS
        if (m_function(arguments, result))
            return result;
        RethrowLastError();
#else
        if (m_function(arguments, result))
            return result;
        return nullptr;
#endif
    }

    template <typename... Args>
        requires((!std::is_same_v<remove_const_ref<Args>, std::vector<ObjectPtr>>) && ...)
    ObjectPtr Invoke(Args&&... arguments)
    {
        return Invoke({ Box(std::forward<Args>(arguments))... });
    }

    template <typename... Args>
    static std::unique_ptr<ConstructorInfo> Register(Type* host,
                                                     ObjectPtr (*function)(Args...),
                                                     const std::map<size_t, std::any>& attributes = {})
    {
        Callable callable = [=](const std::vector<ObjectPtr>& arguments, ObjectPtr& result) -> bool
        {
            if (!detail::CheckArgumentCount("constructor", ".ctor", sizeof...(Args), arguments.size()))
                return false;

            auto tuple = detail::MakeArgs<Args...>(arguments);
            if (!tuple)
            {
                if (GetLastError().code == ErrorCode::None)
                    Fail(ErrorCode::ArgumentType, "constructor argument conversion failed");
                return false;
            }

#if RTTI_HAS_EXCEPTIONS
            try
            {
#endif
                if constexpr (std::is_void_v<decltype(function(std::declval<Args>()...))>)
                {
                    std::apply(function, *tuple);
                    result = nullptr;
                }
                else
                {
                    decltype(auto) value = std::apply(function, *tuple);
                    return detail::StoreReturn<decltype(value)>(std::forward<decltype(value)>(value), result);
                }
#if RTTI_HAS_EXCEPTIONS
            }
            catch (const Error&)
            {
                throw;
            }
            catch (...)
            {
                ThrowTargetInvocation(std::current_exception());
            }
#endif
            return true;
        };

        return std::unique_ptr<ConstructorInfo>(
            new ConstructorInfo(host, { detail::ParameterInfoOf<Args>()... }, std::move(callable), attributes));
    }

    template <typename Host, typename... Args>
    static std::unique_ptr<ConstructorInfo> Register(ObjectPtr (*function)(Args...),
                                                     const std::map<size_t, std::any>& attributes = {})
    {
        return Register(type_of<Host>(), function, attributes);
    }

private:
    Callable m_function;
};

class MethodInfo : public MethodBase
{
public:
    using Callable = std::function<bool(const ObjectPtr&, const std::vector<ObjectPtr>&, ObjectPtr&)>;

protected:
    MethodInfo(Type* owner,
               const std::string& name,
               bool isStatic,
               Type* returnType,
               std::initializer_list<ParameterInfo> parameters,
               Callable function,
               const std::map<size_t, std::any>& attributes)
        : MethodBase(owner, name, returnType, parameters, attributes)
        , m_isStatic(isStatic)
        , m_function(std::move(function))
    {
    }

public:
    bool IsStatic() const noexcept { return m_isStatic; }

    bool TryInvoke(const ObjectPtr& target, const std::vector<ObjectPtr>& arguments, ObjectPtr& result) noexcept
    {
#if RTTI_HAS_EXCEPTIONS
        try
        {
            return m_function(target, arguments, result);
        }
        catch (const Error&)
        {
            return false;
        }
        catch (...)
        {
            return Fail(ErrorCode::TargetInvocation, "reflected method threw an exception");
        }
#else
        return m_function(target, arguments, result);
#endif
    }

    ObjectPtr Invoke(const ObjectPtr& target, const std::vector<ObjectPtr>& arguments)
    {
        ObjectPtr result;
#if RTTI_HAS_EXCEPTIONS
        if (m_function(target, arguments, result))
            return result;
        RethrowLastError();
#else
        if (m_function(target, arguments, result))
            return result;
        return nullptr;
#endif
    }

    template <typename... Args>
        requires((!std::is_same_v<remove_const_ref<Args>, std::vector<ObjectPtr>>) && ...)
    ObjectPtr Invoke(const ObjectPtr& target, Args&&... arguments)
    {
        return Invoke(target, { Box(std::forward<Args>(arguments))... });
    }

    template <typename Host, typename FUNC, typename RET, typename... Args>
    static std::unique_ptr<MethodInfo> Register(const std::string& name,
                                                FUNC function,
                                                const std::map<size_t, std::any>& attributes = {})
    {
        static_assert(detail::is_allowed_method_callable_v<FUNC>,
                      "Method callable must be a function pointer, member pointer, or a captureless lambda. "
                      "Lambdas with captures (especially reference captures) would dangle when invoked later.");

        using SelfType = std::remove_pointer_t<Host>;
        constexpr bool isMember = std::is_member_function_pointer_v<FUNC> || std::is_member_object_pointer_v<FUNC>;
        constexpr bool isMemberObject = std::is_member_object_pointer_v<FUNC>;

        Callable callable = [=](const ObjectPtr& target, const std::vector<ObjectPtr>& arguments,
                                ObjectPtr& result) -> bool
        {
            if (!detail::CheckArgumentCount(type_of<Host>()->GetName().c_str(), name.c_str(), sizeof...(Args),
                                            arguments.size()))
                return false;

            SelfType* self = nullptr;
            if constexpr (isMember)
            {
                if (!target)
                    return Fail(ErrorCode::NullTarget, "target is null for member '" + name + "'");

                if constexpr (is_object<SelfType>)
                {
                    if (!type_of<SelfType>()->IsAssignableFrom(target))
                        return Fail(ErrorCode::TypeMismatch, "target type mismatch for member '" + name + "'");

                    Object* native = target->GetNativeObject(type_of<SelfType>());
                    if (native == nullptr)
                        return Fail(ErrorCode::TypeMismatch, "no physical base object for member '" + name + "'");
                    self = static_cast<SelfType*>(native);
                }
                else
                {
                    Type* targetType = target->GetRttiType();
                    if (targetType != type_of<SelfType>() && targetType != type_of<SelfType*>())
                        return Fail(ErrorCode::TypeMismatch, "target type mismatch for value member '" + name + "'");
                    self = Unbox<SelfType*>(target);
                }
            }

            auto tuple = detail::MakeArgs<Args...>(arguments);
            if (!tuple)
            {
                if (GetLastError().code == ErrorCode::None)
                    Fail(ErrorCode::ArgumentType, "argument conversion failed for '" + name + "'");
                return false;
            }

#if RTTI_HAS_EXCEPTIONS
            try
            {
#endif
                if constexpr (isMemberObject && std::is_void_v<RET>)
                {
                    static_assert(sizeof...(Args) == 1, "a field setter takes exactly one argument");
                    (self->*function) = std::get<0>(*tuple);
                    result = nullptr;
                    return true;
                }
                else if constexpr (isMemberObject)
                {
                    static_assert(sizeof...(Args) == 0, "a field getter takes no arguments");
                    decltype(auto) value = (self->*function);
                    return detail::StoreReturn<decltype(value)>(std::forward<decltype(value)>(value), result);
                }
                else
                {
                    auto invoke = [&](auto&&... a) -> decltype(auto)
                    {
                        if constexpr (std::is_member_function_pointer_v<FUNC>)
                            return std::invoke(function, self, std::forward<decltype(a)>(a)...);
                        else
                            return std::invoke(function, std::forward<decltype(a)>(a)...);
                    };

                    using ResultType = std::invoke_result_t<decltype(invoke), Args...>;
                    if constexpr (std::is_void_v<ResultType>)
                    {
                        std::apply(
                            [&](Args... a) { invoke(std::forward<Args>(a)...); }, *tuple);
                        result = nullptr;
                        return true;
                    }
                    else
                    {
                        decltype(auto) value =
                            std::apply([&](Args... a) -> decltype(auto)
                                       { return invoke(std::forward<Args>(a)...); },
                                       *tuple);
                        return detail::StoreReturn<decltype(value)>(std::forward<decltype(value)>(value), result);
                    }
                }
#if RTTI_HAS_EXCEPTIONS
            }
            catch (const Error&)
            {
                throw;
            }
            catch (...)
            {
                ThrowTargetInvocation(std::current_exception());
            }
#endif
        };

        const bool isStatic = !isMember;
        Type* returnType = nullptr;
        if constexpr (!std::is_void_v<RET>)
            returnType = type_of<RET>();

        return std::unique_ptr<MethodInfo>(new MethodInfo(type_of<Host>(), name, isStatic, returnType,
                                                          { detail::ParameterInfoOf<Args>()... },
                                                          std::move(callable), attributes));
    }

private:
    bool m_isStatic = false;
    Callable m_function;
};

} // namespace rtti
