// SystemPage.cpp
// Modified 2026-11-17 for the fork: use its name for file association labels.
// Modified 2026-11-17 for the fork: apply all-users associations with Windows elevation.

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <shlobj.h>
#else
#include <ShlObj.h>
#endif

#include "../../../Common/Defs.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/Handle.h"
#include "../../../Windows/Registry.h"

#include "../../MyVersion.h"
#include "../Agent/Agent.h"

#include "HelpUtils.h"
#include "IFolder.h"
#include "LangUtils.h"
#include "PropertyNameRes.h"
#include "SystemPage.h"
#include "SystemPageRes.h"

using namespace NWindows;


#define kSystemTopic "FM/options.htm#system"

CSysString CModifiedExtInfo::GetString() const
{
  const char *s;
  if (State == kExtState_7Zip)
    s = MY_PRODUCT_NAME;
  else if (State == kExtState_Clear)
    s = "";
  else if (Other7Zip)
    s = "[7-Zip]";
  else
    return ProgramKey;
  return CSysString (s);
}


int CSystemPage::AddIcon(const UString &iconPath, int iconIndex)
{
  if (iconPath.IsEmpty())
    return -1;
  if (iconIndex == -1)
    iconIndex = 0;
  
  HICON hicon;
  
  // we expand path from REG_EXPAND_SZ registry item.
  UString path;
  const DWORD size = MAX_PATH + 10;
  const DWORD needLen = ::ExpandEnvironmentStringsW(iconPath, path.GetBuf(size + 2), size);
  path.ReleaseBuf_CalcLen(size);
  if (needLen == 0 || needLen >= size)
    path = iconPath;
  const UINT num = ExtractIconExW(path, iconIndex, NULL, &hicon, 1);
  if (num != 1 || !hicon)
    return -1;
  
  _imageList.AddIcon(hicon);
  DestroyIcon(hicon);
  return (int)(_numIcons++);
}


void CSystemPage::RefreshListItem(unsigned group, unsigned listIndex)
{
  const CAssoc &assoc = _items[GetRealIndex(listIndex)];
  _listView.SetSubItem(listIndex, group + 1, assoc.Pair[group].GetString());
  LVITEMW newItem;
  memset(&newItem, 0, sizeof(newItem));
  newItem.iItem = (int)listIndex;
  newItem.mask = LVIF_IMAGE;
  newItem.iImage = assoc.GetIconIndex();
  _listView.SetItem(&newItem);
}


void CSystemPage::ChangeState(unsigned group, const CUIntVector &indices)
{
  if (indices.IsEmpty())
    return;

  bool thereAreClearItems = false;
  unsigned counters[3] = { 0, 0, 0 };
  
  unsigned i;
  for (i = 0; i < indices.Size(); i++)
  {
    const CModifiedExtInfo &mi = _items[GetRealIndex(indices[i])].Pair[group];
    int state = kExtState_7Zip;
    if (mi.State == kExtState_7Zip)
      state = kExtState_Clear;
    else if (mi.State == kExtState_Clear)
    {
      thereAreClearItems = true;
      if (mi.Other)
        state = kExtState_Other;
    }
    counters[state]++;
  }

  int state = kExtState_Clear;
  if (counters[kExtState_Other] != 0)
    state = kExtState_Other;
  else if (counters[kExtState_7Zip] != 0)
    state = kExtState_7Zip;
  
  for (i = 0; i < indices.Size(); i++)
  {
    unsigned listIndex = indices[i];
    CAssoc &assoc = _items[GetRealIndex(listIndex)];
    CModifiedExtInfo &mi = assoc.Pair[group];
    bool change = false;
    
    switch (state)
    {
      case kExtState_Clear: change = true; break;
      case kExtState_Other: change = mi.Other; break;
      default: change = !(mi.Other && thereAreClearItems); break;
    }
    
    if (change)
    {
      mi.State = state;
      RefreshListItem(group, listIndex);
    }
  }
  
  _needSave = true;
  Changed();
}


bool CSystemPage::OnInit()
{
  _needSave = false;

  UString label;
#ifdef Z7_LANG
  LangSetDlgItems(*this, NULL, 0);
  LangString_OnlyFromLangFile(IDT_SYSTEM_ASSOCIATE, label);
  if (g_LangID.IsEqualTo_Ascii_NoCase("zh-cn"))
  {
    SetItemText(IDT_SYSTEM_TOGGLE_HELP, L"切换选中的文件类型。未选中时切换整列。\n应用所有用户的更改需要管理员权限。");
    SetItemText(IDB_SYSTEM_CURRENT, L"切换当前用户关联");
    SetItemText(IDB_SYSTEM_ALL, L"切换所有用户关联");
  }
#endif
  if (label.IsEmpty())
    label = "Associate " MY_PRODUCT_NAME " with:";
  else
    label.Replace(L"7-Zip", UString(MY_PRODUCT_NAME));
  SetItemText(IDT_SYSTEM_ASSOCIATE, label);

  _listView.Attach(GetItem(IDL_SYSTEM_ASSOCIATE));
  _listView.SetUnicodeFormat();
  DWORD newFlags = LVS_EX_FULLROWSELECT;
  _listView.SetExtendedListViewStyle(newFlags, newFlags);

  _numIcons = 0;
  _imageList.Create(16, 16, ILC_MASK | ILC_COLOR32, 0, 0);

  _listView.SetImageList(_imageList, LVSIL_SMALL);

  _listView.InsertColumn(0, LangString(IDS_PROP_FILE_TYPE), 80);

  UString s;

  #if NUM_EXT_GROUPS == 1
    s = "Program";
  #else
      const unsigned kSize = 256;
      BOOL res;

      DWORD size = kSize;

      {
        res = GetUserNameW(s.GetBuf(size), &size);
        s.ReleaseBuf_CalcLen(MyMin((unsigned)size, kSize));
      }
    
      if (!res)
        s = "Current User";
  #endif

  LV_COLUMNW ci;
  ci.mask = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
  ci.cx = 152;
  ci.fmt = LVCFMT_CENTER;
  ci.pszText = s.Ptr_non_const();
  ci.iSubItem = 1;
  _listView.InsertColumn(1, &ci);

  #if NUM_EXT_GROUPS > 1
  {
    LangString(IDS_SYSTEM_ALL_USERS, s);
    ci.pszText = s.Ptr_non_const();
    ci.iSubItem = 2;
    _listView.InsertColumn(2, &ci);
  }
  #endif

  _extDB.Read();
  _items.Clear();

  FOR_VECTOR (i, _extDB.Exts)
  {
    const CExtPlugins &extInfo = _extDB.Exts[i];

    LVITEMW item;
    item.iItem = (int)i;
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    item.lParam = (LPARAM)i;
    item.iSubItem = 0;
    // ListView always uses internal iImage that is 0 by default?
    // so we always use LVIF_IMAGE.
    item.iImage = -1;
    item.pszText = extInfo.Ext.Ptr_non_const();

    CAssoc assoc;
    const CPluginToIcon &plug = extInfo.Plugins[0];
    assoc.SevenZipImageIndex = AddIcon(plug.IconPath, plug.IconIndex);

    CSysString texts[NUM_EXT_GROUPS];
    unsigned g;
    for (g = 0; g < NUM_EXT_GROUPS; g++)
    {
      CModifiedExtInfo &mi = assoc.Pair[g];
      mi.ReadFromRegistry(GetHKey(g), GetSystemString(extInfo.Ext));
      mi.SetState(plug.IconPath);
      mi.ImageIndex = AddIcon(mi.IconPath, mi.IconIndex);
      texts[g] = mi.GetString();
    }
    item.iImage = assoc.GetIconIndex();
    const int itemIndex = _listView.InsertItem(&item);
    for (g = 0; g < NUM_EXT_GROUPS; g++)
      _listView.SetSubItem((unsigned)itemIndex, 1 + g, texts[g]);
    _items.Add(assoc);
  }
  
  if (_listView.GetItemCount() > 0)
    _listView.SetItemState(0, LVIS_FOCUSED, LVIS_FOCUSED);

  return CPropertyPage::OnInit();
}


static UString GetProgramCommand(const UString &programPath)
{
  UString command ('\"');
  command += programPath;
  command += L"\" \"%1\"";
  return command;
}


static LONG SetAssociation(HKEY key, const UString &ext, bool associate,
    const UString &command, const UString &iconPath, int iconIndex)
{
  const CSysString systemExt = GetSystemString(ext);
  if (!associate)
    return NRegistryAssoc::DeleteShellExtensionInfo(key, systemExt);

  UString title = ext;
  title += " Archive";
  return NRegistryAssoc::AddShellExtensionInfo(key, systemExt, title, command, iconPath, iconIndex);
}



extern HINSTANCE g_hInstance;

const wchar_t kApplyAllUsersAssociations[] = L"--apply-all-users-associations";

static bool CanWriteAllUsersAssociations()
{
  NRegistry::CKey key;
  return key.Open(HKEY_LOCAL_MACHINE, TEXT("Software\\Classes"),
      KEY_SET_VALUE | KEY_CREATE_SUB_KEY) == ERROR_SUCCESS;
}

struct CAssociationChange
{
  UString Ext;
  bool Associate;
  UString IconPath;
  int IconIndex;
};

LONG ApplyAllUsersAssociations(const UString &arguments)
{
  UStringVector requests;
  SplitString(arguments, requests);
  if (requests.IsEmpty())
    return ERROR_INVALID_PARAMETER;

  FString programFile;
  if (!NDLL::MyGetModuleFileName(programFile))
    return ERROR_FILE_NOT_FOUND;
  const UString programPath = fs2us(programFile);

  CCodecIcons programIcons;
  programIcons.LoadIcons(g_hInstance);
  CCodecIcons archiveIcons;
  FString archiveFile = NDLL::GetModuleDirPrefix();
  archiveFile += FTEXT("7z.dll");
  {
    // Load the bundled icons as data. The elevated command does not load codecs.
    NDLL::CLibrary archiveResources;
    if (archiveResources.LoadEx(archiveFile))
      archiveIcons.LoadIcons(archiveResources.Get_HMODULE());
  }

  CObjectVector<CAssociationChange> changes;
  FOR_VECTOR (i, requests)
  {
    const UString &request = requests[i];
    if (request.Len() < 2 || (request[0] != L'+' && request[0] != L'-'))
      return ERROR_INVALID_PARAMETER;
    CAssociationChange &change = changes.AddNew();
    change.Ext = request.Ptr(1);
    change.Associate = request[0] == L'+';
    for (unsigned j = 0; j < change.Ext.Len(); j++)
    {
      const wchar_t c = change.Ext[j];
      if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
        return ERROR_INVALID_PARAMETER;
    }
    if (archiveIcons.FindIconIndex(change.Ext, change.IconIndex))
      change.IconPath = fs2us(archiveFile);
    else if (programIcons.FindIconIndex(change.Ext, change.IconIndex))
      change.IconPath = programPath;
    else
      return ERROR_INVALID_PARAMETER;
  }

  if (!CanWriteAllUsersAssociations())
    return ERROR_ACCESS_DENIED;

  const UString command = GetProgramCommand(programPath);
  LONG result = ERROR_SUCCESS;
  FOR_VECTOR (i, changes)
  {
    const CAssociationChange &change = changes[i];
    result = SetAssociation(HKEY_LOCAL_MACHINE, change.Ext, change.Associate, command,
        change.IconPath, change.IconIndex);
    if (result != ERROR_SUCCESS)
      break;
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  return result;
}

static LONG RunElevatedAssociationCommand(HWND owner, const UString &programPath,
    const UString &changes)
{
  UString parameters = kApplyAllUsersAssociations;
  parameters += changes;
  const UString directory = fs2us(NDLL::GetModuleDirPrefix());
  SHELLEXECUTEINFOW launch;
  memset(&launch, 0, sizeof(launch));
  launch.cbSize = sizeof(launch);
  launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
  launch.hwnd = owner;
  launch.lpVerb = L"runas";
  launch.lpFile = programPath;
  launch.lpParameters = parameters;
  launch.lpDirectory = directory;
  launch.nShow = SW_HIDE;
  if (!::ShellExecuteExW(&launch))
    return (LONG)::GetLastError();
  if (!launch.hProcess)
    return ERROR_INVALID_HANDLE;

  CHandle process;
  process.Attach(launch.hProcess);
  if (::WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0)
    return (LONG)::GetLastError();
  DWORD result;
  if (!::GetExitCodeProcess(process, &result))
    return (LONG)::GetLastError();
  return (LONG)result;
}



LONG CSystemPage::OnApply()
{
  if (!_needSave)
    return PSNRET_NOERROR;

  FString programFile;
  if (!NDLL::MyGetModuleFileName(programFile))
  {
    MessageBoxW(*this, NError::MyFormatMessage(ERROR_FILE_NOT_FOUND), UString(MY_PRODUCT_NAME), MB_ICONERROR);
    return PSNRET_INVALID_NOCHANGEPAGE;
  }
  const UString programPath = fs2us(programFile);
  const UString command = GetProgramCommand(programPath);

  UString allUsersChanges;
  FOR_VECTOR (i, _items)
  {
    const CModifiedExtInfo &pending = _items[i].Pair[1];
    if (pending.HasChange())
    {
      allUsersChanges.Add_Space();
      allUsersChanges.Add_Char(pending.State == kExtState_7Zip ? L'+' : L'-');
      allUsersChanges += _extDB.Exts[i].Ext;
    }
  }
  const bool elevated = !allUsersChanges.IsEmpty() && !CanWriteAllUsersAssociations();
  LONG elevatedResult = ERROR_SUCCESS;
  if (elevated)
  {
    elevatedResult = RunElevatedAssociationCommand(GetParent(), programPath, allUsersChanges);
    if (elevatedResult == ERROR_CANCELLED)
      return PSNRET_INVALID_NOCHANGEPAGE;
  }

  LONG result = ERROR_SUCCESS;
  bool applied = false;
  _needSave = false;
  FOR_VECTOR (listIndex, _extDB.Exts)
  {
    const unsigned realIndex = GetRealIndex(listIndex);
    const CExtPlugins &extInfo = _extDB.Exts[realIndex];
    const CPluginToIcon &plug = extInfo.Plugins[0];
    CAssoc &assoc = _items[realIndex];
    for (unsigned g = 0; g < NUM_EXT_GROUPS; g++)
    {
      CModifiedExtInfo &mi = assoc.Pair[g];
      if (!mi.HasChange())
        continue;
      const HKEY key = GetHKey(g);
      const CSysString ext = GetSystemString(extInfo.Ext);
      LONG itemResult;
      if (g == 1 && elevated)
      {
        itemResult = elevatedResult;
        if (itemResult != ERROR_SUCCESS)
        {
          // Some writes may already have completed. Keep only unapplied edits pending.
          CModifiedExtInfo actual;
          actual.ReadFromRegistry(key, ext);
          actual.SetState(plug.IconPath);
          if (actual.State == mi.State)
            itemResult = ERROR_SUCCESS;
        }
      }
      else
        itemResult = SetAssociation(key, extInfo.Ext, mi.State == kExtState_7Zip,
            command, plug.IconPath, plug.IconIndex);

      if (itemResult != ERROR_SUCCESS)
      {
        if (result == ERROR_SUCCESS)
          result = itemResult;
        _needSave = true;
        continue;
      }

      mi.ReadFromRegistry(key, ext);
      mi.SetState(plug.IconPath);
      mi.ImageIndex = AddIcon(mi.IconPath, mi.IconIndex);
      RefreshListItem(g, listIndex);
      applied = true;
    }
  }

  if (applied)
  {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    WasChanged = true;
  }

  if (result != ERROR_SUCCESS)
  {
    MessageBoxW(*this, NError::MyFormatMessage(result), UString(MY_PRODUCT_NAME), MB_ICONERROR);
    return PSNRET_INVALID_NOCHANGEPAGE;
  }
  return PSNRET_NOERROR;
}


void CSystemPage::OnNotifyHelp()
{
  ShowHelpWindow(kSystemTopic);
}


bool CSystemPage::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    /*
    case IDC_SYSTEM_SELECT_ALL:
      _listView.SelectAll();
      return true;
    */
    case IDB_SYSTEM_CURRENT:
    case IDB_SYSTEM_ALL:
      ChangeState(buttonID == IDB_SYSTEM_CURRENT ? 0 : 1);
      return true;
  }
  return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
}


bool CSystemPage::OnNotify(UINT controlID, LPNMHDR lParam)
{
  if (lParam->hwndFrom == HWND(_listView))
  {
    switch (lParam->code)
    {
      case NM_RETURN:
      {
        ChangeState(0);
        return true;
      }

      case NM_CLICK:
      {
        NMITEMACTIVATE *item = (NMITEMACTIVATE *)lParam;
        if (item->uKeyFlags == 0)
        {
          if (item->iItem >= 0)
          {
            // unsigned realIndex = GetRealIndex(item->iItem);
            if (item->iSubItem >= 1 && item->iSubItem <= 2)
            {
              CUIntVector indices;
              indices.Add((unsigned)item->iItem);
              ChangeState(item->iSubItem < 2 ? 0 : 1, indices);
            }
          }
        }
        break;
      }
      
      case LVN_KEYDOWN:
      {
        if (OnListKeyDown(LPNMLVKEYDOWN(lParam)))
          return true;
        break;
      }
      
      /*
      case NM_RCLICK:
      case NM_DBLCLK:
      case LVN_BEGINRDRAG:
        // PostMessage(kRefreshpluginsListMessage, 0);
        PostMessage(kUpdateDatabase, 0);
        break;
      */
    }
  }
  return CPropertyPage::OnNotify(controlID, lParam);
}


void CSystemPage::ChangeState(unsigned group)
{
  CUIntVector indices;
  
  int itemIndex = -1;
  while ((itemIndex = _listView.GetNextSelectedItem(itemIndex)) != -1)
    indices.Add((unsigned)itemIndex);
  
  if (indices.IsEmpty())
    FOR_VECTOR (i, _items)
      indices.Add(i);
  
  ChangeState(group, indices);
}


bool CSystemPage::OnListKeyDown(LPNMLVKEYDOWN keyDownInfo)
{
  bool ctrl = IsKeyDown(VK_CONTROL);
  bool alt = IsKeyDown(VK_MENU);

  if (alt)
    return false;

  if ((ctrl && keyDownInfo->wVKey == 'A')
      || (!ctrl && keyDownInfo->wVKey == VK_MULTIPLY))
  {
    _listView.SelectAll();
    return true;
  }

  switch (keyDownInfo->wVKey)
  {
    case VK_SPACE:
    case VK_ADD:
    case VK_SUBTRACT:
    case VK_SEPARATOR:
    case VK_DIVIDE:

    case VK_OEM_PLUS:
    case VK_OEM_MINUS:

      if (!ctrl)
      {
        ChangeState(keyDownInfo->wVKey == VK_SPACE ? 0 : 1);
        return true;
      }
      break;
  }

  return false;
}
