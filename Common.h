#pragma once

#if defined (_DEBUG)
#if defined (_DEVBUILD)
#pragma error "_DEBUG and _DEVBUILD cannot be defined at the same time!"
#endif

#define DEBUG 1
#define DEVBUILD 0
#define RELEASE 0
#elif defined (_DEVBUILD)
#define DEBUG 0
#define DEVBUILD 1
#define RELEASE 0
#else
#define DEBUG 0
#define DEVBUILD 0
#define RELEASE 1
#endif

#if defined (_WINDOWS)
#define WINDOWS 1
#define PS4 0
#define XBOXONE 0
#elif defined (_PS4)
#define WINDOWS 0
#define PS4 1
#define XBOXONE 0
#elif defined (_XBOXONE)
#define WINDOWS 0
#define PS4 0
#define XBOXONE 1
#else
#pragma error "No platform defined"
#endif

#if defined (_CONSOLE)
#define CONSOLE 1
#else
#define CONSOLE 0
#endif

#define STEAM 0

#define DEVFUNCTIONS (DEBUG || DEVBUILD)

#define UNUSED_VAR(b) (void)(b)