/* Removal of the context menu component shipped by older SuiZip releases. */

#ifndef ZIP7_INC_LEGACY_CONTEXT_MENU_H
#define ZIP7_INC_LEGACY_CONTEXT_MENU_H

#define k_7zip_CLSID L"{23170F69-40C1-278A-1000-000100020000}"

static LONG RemoveLegacyContextMenuRegistration(HKEY hive, LPCWSTR dllPath)
{
  static const WCHAR clsidKey[] = L"Software\\Classes\\CLSID\\" k_7zip_CLSID;
  static const WCHAR inprocKey[] = L"Software\\Classes\\CLSID\\" k_7zip_CLSID L"\\InprocServer32";
  WCHAR registeredPath[MAX_PATH * 2 + 60];
  DWORD size = sizeof(registeredPath) - sizeof(registeredPath[0]);
  DWORD type = 0;
  HKEY key;
  LONG res = RegOpenKeyExW(hive, inprocKey, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key);
  if (res == ERROR_FILE_NOT_FOUND || res == ERROR_PATH_NOT_FOUND)
    return ERROR_SUCCESS;
  if (res != ERROR_SUCCESS)
    return res;

  res = RegQueryValueExW(key, NULL, NULL, &type, (BYTE *)registeredPath, &size);
  RegCloseKey(key);
  if (res == ERROR_FILE_NOT_FOUND || res == ERROR_MORE_DATA)
    return ERROR_SUCCESS;
  if (res != ERROR_SUCCESS)
    return res;
  registeredPath[size / sizeof(registeredPath[0])] = 0;
  if (type != REG_SZ || lstrcmpiW(registeredPath, dllPath) != 0)
    return ERROR_SUCCESS;

  if (hive == HKEY_LOCAL_MACHINE)
  {
    res = RegOpenKeyExW(hive, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        0, KEY_SET_VALUE | KEY_WOW64_32KEY, &key);
    if (res == ERROR_SUCCESS)
    {
      res = RegDeleteValueW(key, k_7zip_CLSID);
      RegCloseKey(key);
    }
    if (res != ERROR_SUCCESS && res != ERROR_FILE_NOT_FOUND && res != ERROR_PATH_NOT_FOUND)
      return res;
  }

  // Only remove the alternate COM registration. Handler keys are shared with Explorer.
  res = RegDeleteKeyExW(hive, inprocKey, KEY_WOW64_32KEY, 0);
  if (res != ERROR_SUCCESS && res != ERROR_FILE_NOT_FOUND && res != ERROR_PATH_NOT_FOUND)
    return res;
  RegDeleteKeyExW(hive, clsidKey, KEY_WOW64_32KEY, 0);
  return ERROR_SUCCESS;
}

static DWORD RemoveLegacyContextMenu(LPCWSTR installPath, BOOL *restartRequired)
{
  static const WCHAR fileName[] = L"7-zip32.dll";
  WCHAR dllPath[MAX_PATH * 2 + 60];
  const size_t prefixLen = (size_t)lstrlenW(installPath);
  LONG res;
  DWORD attributes;

  if (prefixLen + Z7_ARRAY_SIZE(fileName) > Z7_ARRAY_SIZE(dllPath))
    return ERROR_FILENAME_EXCED_RANGE;
  lstrcpyW(dllPath, installPath);
  lstrcpyW(dllPath + prefixLen, fileName);

  res = RemoveLegacyContextMenuRegistration(HKEY_CURRENT_USER, dllPath);
  if (res != ERROR_SUCCESS)
    return (DWORD)res;
  res = RemoveLegacyContextMenuRegistration(HKEY_LOCAL_MACHINE, dllPath);
  if (res != ERROR_SUCCESS)
    return (DWORD)res;

  attributes = GetFileAttributesW(dllPath);
  if (attributes == INVALID_FILE_ATTRIBUTES)
  {
    const DWORD error = GetLastError();
    return (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) ? ERROR_SUCCESS : error;
  }
  if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    return ERROR_SUCCESS;
  if (attributes & FILE_ATTRIBUTE_READONLY)
    SetFileAttributesW(dllPath, attributes & ~(DWORD)FILE_ATTRIBUTE_READONLY);
  if (DeleteFileW(dllPath))
    return ERROR_SUCCESS;
  if (!MoveFileExW(dllPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT))
    return GetLastError();
  if (restartRequired)
    *restartRequired = TRUE;
  return ERROR_SUCCESS;
}

#endif
