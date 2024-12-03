#pragma once

#ifndef _WIN32_WINNT

// windows xp
//#define _WIN32_WINNT 0x0501

// windows 7
#define _WIN32_WINNT 0x0601

#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// glfw3.h defines APIENTRY, redefinition in minwindef.h is found, it is annoying
// #pragma warning( disable : 4005 )

// #include <winsock2.h>
// #include <ws2tcpip.h>

#include <windows.h>

#include <intrin.h>
// Used for ConvertArguments in Win32.cpp
#include <shellapi.h>

// #pragma warning( default : 4005 )