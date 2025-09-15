#pragma once

#ifndef RTTI_BASE_OBJECT
#define RTTI_BASE_OBJECT rtti::Object
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