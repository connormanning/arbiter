#pragma once

#if defined(_WIN32) || defined(WIN32) || defined(_MSC_VER)
#define ARBITER_WINDOWS
#endif

#ifndef ARBITER_DLL
#if defined(ARBITER_WINDOWS)
#if defined(ARBITER_DLL_EXPORT)
#   define ARBITER_DLL   __declspec(dllexport)
#elif defined(PDAL_DLL_IMPORT)
#   define ARBITER_DLL   __declspec(dllimport)
#else
#   define ARBITER_DLL
#endif
#else
#    define ARBITER_DLL     __attribute__ ((visibility("default")))
#endif
#endif

#ifdef _WIN32
#pragma warning(disable:4251)// [templated class] needs to have dll-interface...
#endif

