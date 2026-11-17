/* Precomp.h -- precompilation file
: Igor Pavlov : Public domain */

#ifndef ZIP7_INC_PRECOMP_H
#define ZIP7_INC_PRECOMP_H

/*
  this file must be included before another *.h files and before <windows.h>.
  this file is included from the following files:
    C\*.c
    C\Util\*\Precomp.h   <-  C\Util\*\*.c
    CPP\Common\Common.h  <-  *\StdAfx.h    <-  *\*.cpp

  this file can set the following macros:
    Z7_LARGE_PAGES 1
    Z7_LONG_PATH 1
    Z7_WIN32_WINNT_MIN  0x0A00 (Windows 10)
    _WIN32_WINNT        0x0A00 (or higher)
    WINVER  _WIN32_WINNT
    UNICODE 1
    _UNICODE 1
*/

#include "Compiler.h"

#if (defined(_WIN32) && !defined(_WIN64)) || \
    (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ < 8)
  #error SuiZip requires a 64-bit target.
#endif

#ifdef _MSC_VER
// #pragma warning(disable : 4206) // nonstandard extension used : translation unit is empty
#if _MSC_VER >= 1912
// #pragma warning(disable : 5039) // pointer or reference to potentially throwing function passed to 'extern "C"' function under - EHc.Undefined behavior may occur if this function throws an exception.
#endif
#endif

#ifndef Z7_LARGE_PAGES
#ifndef Z7_NO_LARGE_PAGES
#define Z7_LARGE_PAGES 1
#endif
#endif

#ifdef _WIN32

#if defined(UNDER_CE) || defined(Z7_OLD_WIN_SDK) || defined(Z7_NO_UNICODE)
  #error SuiZip requires the Unicode Windows 10 SDK target or newer.
#endif

#define Z7_WIN32_WINNT_MIN 0x0A00

#if (defined(_WIN32_WINNT) && _WIN32_WINNT < Z7_WIN32_WINNT_MIN) || \
    (defined(WINVER) && WINVER < Z7_WIN32_WINNT_MIN) || \
    (defined(NTDDI_VERSION) && NTDDI_VERSION < 0x0A000000)
  #error SuiZip requires Windows 10 or newer.
#endif

#ifndef Z7_LONG_PATH
#ifndef Z7_NO_LONG_PATH
#define Z7_LONG_PATH 1
#endif
#endif

#ifndef Z7_DEVICE_FILE
#ifndef Z7_NO_DEVICE_FILE
// #define Z7_DEVICE_FILE 1
#endif
#endif

#ifndef _WIN32_WINNT
Z7_DIAGNOSTIC_IGNORE_BEGIN_RESERVED_MACRO_IDENTIFIER
#define _WIN32_WINNT Z7_WIN32_WINNT_MIN
Z7_DIAGNOSTIC_IGNORE_END_RESERVED_MACRO_IDENTIFIER
#endif

#ifndef WINVER
  #define WINVER  _WIN32_WINNT
#endif

#ifndef UNICODE
#define UNICODE 1
#endif

#ifndef _UNICODE
Z7_DIAGNOSTIC_IGNORE_BEGIN_RESERVED_MACRO_IDENTIFIER
#define _UNICODE 1
Z7_DIAGNOSTIC_IGNORE_END_RESERVED_MACRO_IDENTIFIER
#endif

#endif // _WIN32

#endif
