#pragma once

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// ============================================================================
// Ptr<T> — the smart pointer type alias used throughout the library.
//
// Default: std::shared_ptr<T>. Define RTTI_PTR_ALIAS before including any RTTI
// header to use a different smart pointer. All translation units must agree.
// ============================================================================

#ifndef RTTI_PTR_ALIAS
#define RTTI_PTR_ALIAS(T) std::shared_ptr<T>
#endif

namespace rtti
{
template <typename T>
using Ptr = RTTI_PTR_ALIAS(T);
} // namespace rtti

namespace rtti::detail
{

#ifndef RTTI_CUSTOM_POINTER_OPS

template <typename T>
struct pointer_ops
{
    template <typename U = T, typename... Args>
    static Ptr<U> make(Args&&... args)
    {
        return std::make_shared<U>(std::forward<Args>(args)...);
    }

    template <typename U>
    static Ptr<U> ptr_cast(const Ptr<T>& p)
    {
        return std::static_pointer_cast<U>(p);
    }

    static T* to_raw(const Ptr<T>& p) noexcept { return p.get(); }

    static Ptr<T> from_raw(T* raw)
    {
        if (raw == nullptr)
            return Ptr<T>();
        return std::static_pointer_cast<T>(raw->shared_from_this());
    }
};

#endif // RTTI_CUSTOM_POINTER_OPS

} // namespace rtti::detail

// ============================================================================
// Object root base customization.
//
// Default: std::enable_shared_from_this<Object>. Define RTTI_OBJECT_BASE
// before including any RTTI header to replace it.
// ============================================================================

#ifndef RTTI_OBJECT_BASE
#define RTTI_OBJECT_BASE std::enable_shared_from_this<Object>
#endif

// ============================================================================
// Exception support
// ============================================================================

#if !defined(RTTI_HAS_EXCEPTIONS)
#if defined(RTTI_NO_EXCEPTIONS)
#define RTTI_HAS_EXCEPTIONS 0
#elif defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define RTTI_HAS_EXCEPTIONS 1
#else
#define RTTI_HAS_EXCEPTIONS 0
#endif
#endif

// ============================================================================
// Logging
//
// The library never writes to stdout/stderr by itself. Install a handler with
// SetLogHandler(); when RTTI_ENABLE_LOG is defined a stderr handler is
// installed by default.
// ============================================================================

namespace rtti
{

enum class LogLevel
{
    Debug,
    Warning,
    Error
};

using LogHandler = void (*)(LogLevel, const char*);

#if defined(RTTI_ENABLE_LOG) && RTTI_ENABLE_LOG
inline void DefaultLogHandler(LogLevel level, const char* message) noexcept
{
    const char* name = level == LogLevel::Debug ? "debug" : (level == LogLevel::Warning ? "warning" : "error");
    std::fprintf(stderr, "[rtti][%s] %s\n", name, message ? message : "");
}
#endif

inline LogHandler& LogHandlerStorage() noexcept
{
#if defined(RTTI_ENABLE_LOG) && RTTI_ENABLE_LOG
    static LogHandler handler = &DefaultLogHandler;
#else
    static LogHandler handler = nullptr;
#endif
    return handler;
}

inline void SetLogHandler(LogHandler handler) noexcept { LogHandlerStorage() = handler; }

inline LogHandler GetLogHandler() noexcept { return LogHandlerStorage(); }

inline bool HasLogHandler() noexcept { return LogHandlerStorage() != nullptr; }

inline void Log(LogLevel level, std::string_view message) noexcept
{
    if (LogHandler handler = GetLogHandler())
        handler(level, std::string(message).c_str());
}

} // namespace rtti

#define RTTI_LOG(level, msg)                                                     \
    do                                                                           \
    {                                                                            \
        if (::rtti::HasLogHandler())                                             \
            ::rtti::Log(::rtti::LogLevel::level, (msg));                         \
    } while (0)

#define RTTI_DEBUG(msg) RTTI_LOG(Debug, msg)
#define RTTI_WARNING(msg) RTTI_LOG(Warning, msg)
#define RTTI_ERROR(msg) RTTI_LOG(Error, msg)
