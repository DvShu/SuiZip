/* DllSecur.c -- DLL loading security
: Igor Pavlov : Public domain */

#include "Precomp.h"

#ifdef _WIN32

#include "7zWindows.h"

#include "DllSecur.h"


#define DELIM "\0"

static const char * const g_Dlls =
         "userenv"
  DELIM  "setupapi"
  DELIM  "apphelp"
  DELIM  "propsys"
  DELIM  "dwmapi"
  DELIM  "cryptbase"
  DELIM  "oleacc"
  DELIM  "clbcatq"
  DELIM  "version"
  #ifndef _CONSOLE
  DELIM  "uxtheme"
  #endif
  DELIM;


static BOOL SetSafeDllDirectories(void)
{
  return SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
}

void My_SetDefaultDllDirectories(void)
{
  SetSafeDllDirectories();
}


void LoadSecurityDlls(void)
{
  if (SetSafeDllDirectories())
    return;
  {
    WCHAR buf[MAX_PATH + 100];
    const char *dll;
    unsigned pos = GetSystemDirectoryW(buf, MAX_PATH + 2);
    if (pos == 0 || pos > MAX_PATH)
      return;
    if (buf[pos - 1] != '\\')
      buf[pos++] = '\\';
    for (dll = g_Dlls; *dll != 0;)
    {
      WCHAR *dest = &buf[pos];
      for (;;)
      {
        const char c = *dll++;
        if (c == 0)
          break;
        *dest++ = (Byte)c;
      }
      dest[0] = '.';
      dest[1] = 'd';
      dest[2] = 'l';
      dest[3] = 'l';
      dest[4] = 0;
      // lstrcatW(buf, L".dll");
      LoadLibraryExW(buf, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
  }
}

#endif // _WIN32
