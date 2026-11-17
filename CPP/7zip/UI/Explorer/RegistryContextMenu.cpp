// RegistryContextMenu.cpp
// Modified 2026-11-16 for the fork: use the product name for shell registration.

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../../Windows/Registry.h"

#include "../../MyVersion.h"
#include "RegistryContextMenu.h"

using namespace NWindows;
using namespace NRegistry;


#define k_Clsid_A "{23170F69-40C1-278A-1000-000100020000}"

static LPCTSTR const k_Clsid = TEXT(k_Clsid_A);
static LPCTSTR const k_ShellExtName = TEXT(MY_PRODUCT_NAME) TEXT(" Shell Extension");

static LPCTSTR const k_Approved = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved");
static LPCTSTR const k_Inproc = TEXT("InprocServer32");

static LPCSTR const k_KeyPostfix_ContextMenu = "\\shellex\\ContextMenuHandlers\\7-Zip";
static LPCSTR const k_KeyPostfix_DragDrop    = "\\shellex\\DragDropHandlers\\7-Zip";

static LPCSTR const k_KeyName_File      = "*";
static LPCSTR const k_KeyName_Folder    = "Folder";
static LPCSTR const k_KeyName_Directory = "Directory";
static LPCSTR const k_KeyName_Drive     = "Drive";

static LPCSTR const k_shellex_Prefixes[] =
{
  k_KeyName_File,
  k_KeyName_Folder,
  k_KeyName_Directory,
  k_KeyName_Drive
};

static const bool k_shellex_Statuses[2][4] =
{
  { true, true, true, false },
  { false, false, true, true }
};


// static NSynchronization::CCriticalSection g_CS;

static AString Get_ContextMenuHandler_KeyName(LPCSTR keyName)
  { return (AString)keyName + k_KeyPostfix_ContextMenu; }

/*
static CSysString Get_DragDropHandler_KeyName(LPCTSTR keyName)
  { return (AString)keyName + k_KeyPostfix_DragDrop); }
*/

static bool CheckHandlerCommon(const AString &keyName)
{
  CKey key;
  if (key.Open(HKEY_CLASSES_ROOT, (CSysString)keyName, KEY_READ) != ERROR_SUCCESS)
    return false;
  CSysString value;
  if (key.QueryValue(NULL, value) != ERROR_SUCCESS)
    return false;
  return StringsAreEqualNoCase_Ascii(value, k_Clsid_A);
}

bool CheckContextMenuHandler(const UString &path)
{
  // NSynchronization::CCriticalSectionLock lock(g_CS);

  CSysString s ("CLSID\\");
  s += k_Clsid_A;
  s += "\\InprocServer32";
  
  {
    NRegistry::CKey key;
    if (key.Open(HKEY_CLASSES_ROOT, s, KEY_READ) != ERROR_SUCCESS)
      return false;
    UString regPath;
    if (key.QueryValue(NULL, regPath) != ERROR_SUCCESS)
      return false;
    if (!path.IsEqualTo_NoCase(regPath))
      return false;
  }
  
  return
       CheckHandlerCommon(Get_ContextMenuHandler_KeyName(k_KeyName_File));
}


LONG SetContextMenuHandler(bool setMode, const UString &path)
{
  // NSynchronization::CCriticalSectionLock lock(g_CS);

  LONG res;

  {
  CSysString s ("CLSID\\");
  s += k_Clsid_A;

  if (setMode)
  {
    {
      CKey key;
      res = key.Create(HKEY_CLASSES_ROOT, s);
      if (res == ERROR_SUCCESS)
      {
        key.SetValue(NULL, k_ShellExtName);
        CKey keyInproc;
        res = keyInproc.Create(key, k_Inproc);
        if (res == ERROR_SUCCESS)
        {
          res = keyInproc.SetValue(NULL, path);
          keyInproc.SetValue(TEXT("ThreadingModel"), TEXT("Apartment"));
        }
      }
    }
    
    {
      CKey key;
      if (key.Create(HKEY_LOCAL_MACHINE, k_Approved) == ERROR_SUCCESS)
        key.SetValue(k_Clsid, k_ShellExtName);
    }
  }
  else
  {
    CSysString s2 (s);
    s2 += "\\InprocServer32";

    RegDeleteKey(HKEY_CLASSES_ROOT, s2);
    res = RegDeleteKey(HKEY_CLASSES_ROOT, s);
  }
  }

  // The CLSID controls activation. Leave shared handler keys on disable.
  if (setMode)
  for (unsigned i = 0; i < 2; i++)
  {
    for (unsigned k = 0; k < Z7_ARRAY_SIZE(k_shellex_Prefixes); k++)
    {
      CSysString s (k_shellex_Prefixes[k]);
      s += (i == 0 ? k_KeyPostfix_ContextMenu : k_KeyPostfix_DragDrop);
      if (k_shellex_Statuses[i][k])
      {
        CKey key;
        key.Create(HKEY_CLASSES_ROOT, s);
        key.SetValue(NULL, k_Clsid);
      }
      else
        RegDeleteKey(HKEY_CLASSES_ROOT, s);
    }
  }

  return res;
}
