#pragma once

// 自定义 Object 基类
#ifndef RTTI_OBJECT_DEFINE
#define RTTI_OBJECT_DEFINE class Object : public std::enable_shared_from_this<Object>
#endif

// 自定义智能指针
#ifndef RTTI_PTR
#define RTTI_PTR(T) std::shared_ptr<T>
#define RTTI_MAKE_PTR(T, ...) std::make_shared<T>(__VA_ARGS__)
#define RTTI_PTR_CAST(T, p) std::static_pointer_cast<T>(p)
#define RTTI_PTR_FROM_RAW(p) p->shared_from_this()
#define RTTI_RAW_FROM_PTR(p) p.get()
#endif

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