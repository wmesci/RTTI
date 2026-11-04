#pragma once

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "Config.h"

namespace rtti
{

enum class ErrorCode
{
    None = 0,
    TypeMismatch,
    NotConvertible,
    ArgumentCount,
    ArgumentType,
    NullTarget,
    ReadOnlyProperty,
    TypeNotFound,
    DuplicateTypeName,
    RegistryFrozen,
    InvalidDynamicType,
    NoMatchingConstructor,
    TargetInvocation,
    InvalidOperation,
    Internal
};

inline const char* GetErrorCodeName(ErrorCode code) noexcept
{
    switch (code)
    {
        case ErrorCode::None: return "None";
        case ErrorCode::TypeMismatch: return "TypeMismatch";
        case ErrorCode::NotConvertible: return "NotConvertible";
        case ErrorCode::ArgumentCount: return "ArgumentCount";
        case ErrorCode::ArgumentType: return "ArgumentType";
        case ErrorCode::NullTarget: return "NullTarget";
        case ErrorCode::ReadOnlyProperty: return "ReadOnlyProperty";
        case ErrorCode::TypeNotFound: return "TypeNotFound";
        case ErrorCode::DuplicateTypeName: return "DuplicateTypeName";
        case ErrorCode::RegistryFrozen: return "RegistryFrozen";
        case ErrorCode::InvalidDynamicType: return "InvalidDynamicType";
        case ErrorCode::NoMatchingConstructor: return "NoMatchingConstructor";
        case ErrorCode::TargetInvocation: return "TargetInvocation";
        case ErrorCode::InvalidOperation: return "InvalidOperation";
        case ErrorCode::Internal: return "Internal";
    }
    return "Unknown";
}

struct ErrorInfo
{
    ErrorCode code = ErrorCode::None;
    std::string message;
};

inline ErrorInfo& LastErrorStorage() noexcept
{
    thread_local ErrorInfo error;
    return error;
}

inline const ErrorInfo& GetLastError() noexcept { return LastErrorStorage(); }

inline void ClearLastError() noexcept { LastErrorStorage() = ErrorInfo{}; }

inline bool Fail(ErrorCode code, std::string message) noexcept
{
    ErrorInfo& error = LastErrorStorage();
    error.code = code;
    error.message = std::move(message);
    return false;
}

#if RTTI_HAS_EXCEPTIONS

class Error : public std::runtime_error
{
public:
    Error(ErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }

    ErrorCode Code() const noexcept { return m_code; }

private:
    ErrorCode m_code;
};

class TargetInvocationError : public Error
{
public:
    TargetInvocationError(std::exception_ptr inner, const std::string& message = "target invocation failed")
        : Error(ErrorCode::TargetInvocation, message)
        , m_inner(std::move(inner))
    {
    }

    const std::exception_ptr& Inner() const noexcept { return m_inner; }

private:
    std::exception_ptr m_inner;
};

[[noreturn]] inline void ThrowError(ErrorCode code, std::string message)
{
    Fail(code, message);
    throw Error(code, message);
}

[[noreturn]] inline void ThrowTargetInvocation(std::exception_ptr inner)
{
    Fail(ErrorCode::TargetInvocation, "target invocation failed");
    throw TargetInvocationError(std::move(inner));
}

inline void ReportError(ErrorCode code, std::string message)
{
    ThrowError(code, std::move(message));
}

[[noreturn]] inline void RethrowLastError()
{
    const ErrorInfo error = GetLastError();
    throw Error(error.code == ErrorCode::None ? ErrorCode::Internal : error.code, error.message);
}

#else // RTTI_HAS_EXCEPTIONS

inline void ReportError(ErrorCode code, std::string message) { (void)Fail(code, std::move(message)); }

[[noreturn]] inline void RethrowLastError() noexcept { std::abort(); }

#endif // RTTI_HAS_EXCEPTIONS

// Programming/invariant errors abort even when exceptions are enabled: they
// indicate a bug in the library or in the registration code, not bad input.
[[noreturn]] inline void FatalError(ErrorCode code, const std::string& message) noexcept
{
    Fail(code, message);
    RTTI_ERROR(message.c_str());
    std::abort();
}

#if !RTTI_HAS_EXCEPTIONS
[[noreturn]] inline void ThrowError(ErrorCode code, const std::string& message)
{
    FatalError(code, message);
}
#endif

} // namespace rtti
