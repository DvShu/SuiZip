// RegistryAssociations.cpp
// Modified 2026-11-17 for the fork: update association values and propagate write errors.

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/Registry.h"

#include "RegistryAssociations.h"

using namespace NWindows;
using namespace NRegistry;

namespace NRegistryAssoc {
  
// static NSynchronization::CCriticalSection g_CriticalSection;

static const TCHAR * const kClasses = TEXT("Software\\Classes\\");
// static const TCHAR * const kShellNewKeyName = TEXT("ShellNew");
// static const TCHAR * const kShellNewDataValueName = TEXT("Data");
static const TCHAR * const kDefaultIconKeyName = TEXT("DefaultIcon");
static const TCHAR * const kShellKeyName = TEXT("shell");
static const TCHAR * const kOpenKeyName = TEXT("open");
static const TCHAR * const kCommandKeyName = TEXT("command");
static const char * const k7zipPrefix = "7-Zip.";

static CSysString GetExtProgramKeyName(const CSysString &ext)
{
  return CSysString(k7zipPrefix) + ext;
}

static CSysString GetFullKeyPath(HKEY hkey, const CSysString &name)
{
  CSysString s;
  if (hkey != HKEY_CLASSES_ROOT)
    s = kClasses;
  return s + name;
}

static CSysString GetExtKeyPath(HKEY hkey, const CSysString &ext)
{
  return GetFullKeyPath(hkey, (TEXT(".")) + ext);
}

bool CShellExtInfo::ReadFromRegistry(HKEY hkey, const CSysString &ext)
{
  ProgramKey.Empty();
  IconPath.Empty();
  IconIndex = -1;
  // NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
  {
    CKey extKey;
    if (extKey.Open(hkey, GetExtKeyPath(hkey, ext), KEY_READ) != ERROR_SUCCESS)
      return false;
    if (extKey.QueryValue(NULL, ProgramKey) != ERROR_SUCCESS)
      return false;
  }
  {
    CKey iconKey;
 
    if (iconKey.Open(hkey, GetFullKeyPath(hkey, ProgramKey + CSysString(CHAR_PATH_SEPARATOR) + kDefaultIconKeyName), KEY_READ) == ERROR_SUCCESS)
    {
      UString value;
      if (iconKey.QueryValue(NULL, value) == ERROR_SUCCESS)
      {
        const int pos = value.ReverseFind(L',');
        IconPath = value;
        if (pos >= 0)
        {
          const wchar_t *end;
          const Int32 index = ConvertStringToInt32((const wchar_t *)value + pos + 1, &end);
          if (*end == 0)
          {
            // 9.31: if there is no icon index, we use -1. Is it OK?
            if (pos != (int)value.Len() - 1)
              IconIndex = (int)index;
            IconPath.SetFrom(value, (unsigned)pos);
          }
        }
      }
    }
  }
  return true;
}

bool CShellExtInfo::IsIt7Zip() const
{
  return ProgramKey.IsPrefixedBy_Ascii_NoCase(k7zipPrefix);
}

LONG DeleteShellExtensionInfo(HKEY hkey, const CSysString &ext)
{
  CKey extKey;
  LONG res = extKey.Open(hkey, GetExtKeyPath(hkey, ext), KEY_SET_VALUE);
  if (res == ERROR_SUCCESS)
    res = extKey.DeleteValue((LPCTSTR)NULL);
  return res == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : res;
}

LONG AddShellExtensionInfo(HKEY hkey,
    const CSysString &ext,
    const UString &programTitle,
    const UString &programOpenCommand,
    const UString &iconPath, int iconIndex
    // , const void *shellNewData, int shellNewDataSize
    )
{
  const CSysString programKeyName = GetExtProgramKeyName(iconIndex < 0 ? CSysString("*") : ext);
  CKey programKey;
  RINOK(programKey.Create(hkey, GetFullKeyPath(hkey, programKeyName)))
  RINOK(programKey.SetValue(NULL, programTitle))

  CKey iconKey;
  UString iconPathFull = iconPath;
  iconPathFull.Add_Char(',');
  iconPathFull.Add_UInt32((UInt32)MyMax(iconIndex, 0));
  RINOK(iconKey.Create(programKey, kDefaultIconKeyName))
  RINOK(iconKey.SetValue(NULL, iconPathFull))

  CKey shellKey;
  RINOK(shellKey.Create(programKey, kShellKeyName))
  RINOK(shellKey.SetValue(NULL, TEXT("")))

  CKey openKey;
  RINOK(openKey.Create(shellKey, kOpenKeyName))
  RINOK(openKey.SetValue(NULL, TEXT("")))

  CKey commandKey;
  RINOK(commandKey.Create(openKey, kCommandKeyName))
  RINOK(commandKey.SetValue(NULL, programOpenCommand))

  return SetValue(hkey, GetExtKeyPath(hkey, ext), NULL, programKeyName);
}

}
