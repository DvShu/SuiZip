#define MY_VER_MAJOR 26
#define MY_VER_MINOR 6
#define MY_VER_BUILD 0
#define MY_VERSION_NUMBERS "26.06"
#define MY_VERSION MY_VERSION_NUMBERS
// Modified 2026-11-16 for the fork: added its product name and independent version.
#include "SuiZipVersion.h"
#define MY_PRODUCT_NAME "SuiZip"

#ifdef MY_CPU_NAME
  #define MY_VERSION_CPU MY_VERSION " (" MY_CPU_NAME ")"
  #define MY_PRODUCT_VERSION_CPU MY_PRODUCT_VERSION " (" MY_CPU_NAME ")"
#else
  #define MY_VERSION_CPU MY_VERSION
  #define MY_PRODUCT_VERSION_CPU MY_PRODUCT_VERSION
#endif

#define MY_DATE "2026-11-06"
#undef MY_COPYRIGHT
#undef MY_VERSION_COPYRIGHT_DATE
#define MY_AUTHOR_NAME "Igor Pavlov"
#define MY_COPYRIGHT_PD "Igor Pavlov : Public domain"
#define MY_COPYRIGHT_CR "Copyright (c) 1999-2026 Igor Pavlov"

#ifdef USE_COPYRIGHT_CR
  #define MY_COPYRIGHT MY_COPYRIGHT_CR
#else
  #define MY_COPYRIGHT MY_COPYRIGHT_PD
#endif

#define MY_COPYRIGHT_DATE MY_COPYRIGHT " : " MY_DATE
#define MY_VERSION_COPYRIGHT_DATE MY_VERSION_CPU " : " MY_COPYRIGHT " : " MY_DATE
