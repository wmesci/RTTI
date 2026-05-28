#pragma once

#include <memory>

// ============================================================================
// Ptr<T> — The smart pointer type alias used throughout the library.
//
// Default: std::shared_ptr<T>.
//
// To use a different smart pointer globally, define RTTI_PTR_ALIAS before
// including any RTTI header.  Make sure ALL translation units see the same
// definition.
//
// Example:
//   #define RTTI_PTR_ALIAS(T) my::IntrusivePtr<T>
//   #include <RTTI/RTTI.h>
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

// ============================================================================
// pointer_ops<T> — Customization point for smart pointer operations.
//
// Default: operations for std::shared_ptr<T>.
//
// When using a custom smart pointer via RTTI_PTR_ALIAS, you MUST also
// specialize pointer_ops<T> for your types BEFORE including any RTTI header.
//
// Example for a custom intrusive pointer:
//
//   namespace rtti::detail {
//   template <typename T>
//   struct pointer_ops<T> {
//       template <typename U = T, typename... Args>
//       static rtti::Ptr<U> make(Args&&... args) {
//           return my::MakeIntrusive<U>(std::forward<Args>(args)...);
//       }
//
//       template <typename U>
//       static rtti::Ptr<U> ptr_cast(const rtti::Ptr<T>& p) {
//           return my::static_pointer_cast<U>(p);
//       }
//
//       static T* to_raw(const rtti::Ptr<T>& p) noexcept { return p.get(); }
//       static rtti::Ptr<T> from_raw(T* raw) { return rtti::Ptr<T>(raw); }
//   };
//   } // namespace rtti::detail
// ============================================================================

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

    static Ptr<T> from_raw(T* raw) { return raw->shared_from_this(); }
};

// ============================================================================
// object_base<T> — Customization point for the Object root class base.
//
// Default: std::enable_shared_from_this<T>.
//
// Specialize this (typically for rtti::Object) BEFORE including any RTTI
// header to replace the base of the Object class.
// ============================================================================

template <typename T>
struct object_base
{
    using type = std::enable_shared_from_this<T>;
};

} // namespace rtti::detail

// ============================================================================
// Logging macros
// ============================================================================

#define RTTI_LOG(x, msg) printf("[" #x "]: %s\n\t\t in %s[%s:%d]\n", (msg), __PRETTY_FUNCTION__, __FILE__, __LINE__)

#if !defined(RTTI_DEBUG)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_DEBUG(msg) RTTI_LOG(DEBUG, msg)
#else
#define RTTI_DEBUG(msg)
#endif
#endif

#if !defined(RTTI_WARNING)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_WARNING(msg) RTTI_LOG(WARNING, msg)
#else
#define RTTI_WARNING(msg)
#endif
#endif

#if !defined(RTTI_ERROR)
#if defined(RTTI_ENABLE_LOG)
#define RTTI_ERROR(msg) RTTI_LOG(ERROR, msg)
#else
#define RTTI_ERROR(msg)
#endif
#endif
