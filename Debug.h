#pragma once

#include "Common.h"
#include "MacroHelper.h"

#ifdef ASSERT
#undef ASSERT
#endif
#ifdef VERIFY
#undef VERIFY
#endif
#ifdef TRACE
#undef TRACE
#endif

#define ASSERTS_ENABLED (DEVFUNCTIONS)
#define DASSERTS_ENABLED (DEBUG)

#if WINDOWS
namespace ddbg
{
	enum class DialogResult
	{
		Ignore,
		IgnoreAlways,
		Break
	};
	DialogResult AssertTaskDialog(const wchar_t * title, const char * file, int line, bool allButtons, const char * message);
	void Trace(const char * message);
}

#define ERROR_DIALOG(title, file, line, allButtons, condition, ...)    \
        ddbg::AssertTaskDialog(title, file, line, allButtons, #condition)

#if ASSERTS_ENABLED        
#define ASSERT(x, ...)                                                                          \
        {                                                                                               \
            static bool ___ignoreAlways = false;                                                        \
            if(!___ignoreAlways && !(x))                                                                \
            {                                                                                           \
                switch(ERROR_DIALOG(L"Assertion Failed", __FILE__, __LINE__, true, x, __VA_ARGS__))  \
                {                                                                                       \
                case ddbg::DialogResult::Ignore:                                                        \
                    break;                                                                              \
                case ddbg::DialogResult::IgnoreAlways:                                                  \
                    ___ignoreAlways = true;                                                             \
                    break;                                                                              \
                default:                                                                                \
                    __debugbreak();                                                                     \
                    break;                                                                              \
                }                                                                                       \
            }                                                                                           \
        }
#define TRACE(condition, str, ...) { auto trace_condition = (condition); if (trace_condition) { ddbg::Trace(Str::Format(str, __VA_ARGS__).CharPtr()); } }
#define IF_ASSERTS_ENABLED(...) __VA_ARGS__
#else 
#define ASSERT(...)
#define TRACE(...)
#define IF_ASSERTS_ENABLED(...)
#endif

#else
#error "Need to define ASSERT and TRACE for this platform!"
#endif

// Debug ASSERT and VERIFY
#if DASSERTS_ENABLED
#define DASSERT(...) ASSERT(##__VA_ARGS__)
#define IF_DASSERTS_ENABLED(...) IF_ASSERTS_ENABLED(__VA_ARGS__)
#define DTRACE(...) TRACE(__VA_ARGS__)
#else
#define DASSERT(...)
#define IF_DASSERTS_ENABLED(...)
#define DTRACE(...)
#endif

// Checks for memory leaks
#if DEVFUNCTIONS
#if WINDOWS
#ifndef NO_OVERRIDE_NEW
#define _CRTDBG_MAP_ALLOC
#pragma warning (push)
#pragma warning (disable: 4548 4668)
#pragma warning (disable: 4987 4623 4626 5027)

#ifdef _malloca
#undef _malloca
#endif

#include <crtdbg.h>
#pragma warning (pop)
#define new new(_NORMAL_BLOCK,__FILE__, __LINE__)
void InitDebugger();
#endif
#else
#define InitDebugger()
#endif
#else
#define InitDebugger()
#endif
