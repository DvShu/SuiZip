// Panel.cpp
// Modified 2026-11-16 for the fork: revise the file pane layout.

#include "StdAfx.h"

#include <windowsx.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/Thread.h"

#include "../../PropID.h"

#include "resource.h"
#include "../GUI/ExtractRes.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ZipRegistry.h"

#include "../Agent/IFolderArchive.h"

#include "App.h"
#include "ExtractCallback.h"
#include "FSFolder.h"
#include "FileManagerStyle.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "Panel.h"
#include "RootFolder.h"

#include "PropertyNameRes.h"

using namespace NWindows;
using namespace NControl;


static const UINT_PTR kTimerID = 1;
static const UINT kTimerElapse = 1000;

static DWORD kStyles[4] = { LVS_ICON, LVS_SMALLICON, LVS_LIST, LVS_REPORT };

// static const int kCreateFolderID = 101;

extern HINSTANCE g_hInstance;

void CPanel::ReleasePanel()
{
  Disable_Processing_Timer_Notify_StatusBar();
  // It's for unloading COM dll's: don't change it.
  CloseOpenFolders();
  _sevenZipContextMenu.Release();
  _systemContextMenu.Release();
}

CPanel::~CPanel()
{
  CloseOpenFolders();
}

HWND CPanel::GetParent() const
{
  const HWND h = CWindow2::GetParent();
  return h ? h : _mainWindow;
}

#define kClassName L"7-Zip::Panel"


HRESULT CPanel::Create(HWND mainWindow, HWND parentWindow, UINT id,
    const UString &currentFolderPrefix,
    const UString &arcFormat,
    CPanelCallback *panelCallback, CAppState *appState,
    bool needOpenArc,
    COpenResult &openRes)
{
  _mainWindow = mainWindow;
  _processTimer = true;
  _processNotify = true;
  _processStatusBar = true;

  _panelCallback = panelCallback;
  _appState = appState;
  // _index = index;
  _baseID = id;
  _comboBoxID = _baseID + 3;
  _statusBarID = _comboBoxID + 1;

  UString cfp = currentFolderPrefix;

  if (!currentFolderPrefix.IsEmpty())
    if (currentFolderPrefix[0] == L'.')
    {
      FString cfpF;
      if (NFile::NDir::MyGetFullPathName(us2fs(currentFolderPrefix), cfpF))
        cfp = fs2us(cfpF);
    }

  RINOK(BindToPath(cfp, arcFormat, openRes))

  if (needOpenArc && !openRes.ArchiveIsOpened)
    return S_OK;

  if (!CreateEx(0, kClassName, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
      0, 0, _xSize, 260,
      parentWindow, (HMENU)(UINT_PTR)id, g_hInstance))
    return E_FAIL;
  PanelCreated = true;

  return S_OK;
}

// extern UInt32 g_NumMessages;

static const int kPaneFrameWidth = 1;

static int GetAddressBarTop(int inset)
{
  return (g_App.ShowArchiveToolbar || g_App.ShowStandardToolbar) ? kPaneFrameWidth : inset;
}

static void StyleStatusBar(CStatusBar &statusBar)
{
  const HMODULE theme = ::GetModuleHandle(TEXT("uxtheme.dll"));
  if (theme)
  {
    typedef HRESULT (WINAPI *SetWindowThemeFunc)(HWND, LPCWSTR, LPCWSTR);
    const SetWindowThemeFunc setWindowTheme = Z7_GET_PROC_ADDRESS(SetWindowThemeFunc, theme, "SetWindowTheme");
    if (setWindowTheme)
      setWindowTheme(statusBar, L"", L"");
  }
  statusBar.SendMsg(SB_SETBKCOLOR, 0, ::GetSysColor(COLOR_WINDOW));
}

static void ClipControlEdges(HWND control, int top, int right, int bottom)
{
  RECT rect;
  if (!::GetWindowRect(control, &rect))
    return;
  const int width = RECT_SIZE_X(rect);
  const int height = RECT_SIZE_Y(rect);
  if (width <= right || height <= top + bottom)
    return;

  HRGN region = ::CreateRectRgn(0, top, width - right, height - bottom);
  if (region && !::SetWindowRgn(control, region, TRUE))
    ::DeleteObject(region);
}

static void DrawControlFrame(HWND panel, HWND control, HDC dc, int clippedTop = 0)
{
  RECT rect;
  if (!::GetWindowRect(control, &rect) || rect.left >= rect.right || rect.top >= rect.bottom)
    return;
  ::MapWindowPoints(NULL, panel, (POINT *)&rect, 2);
  // Place the frame outside the visible control, including any clipped edge.
  rect.top += clippedTop;
  ::InflateRect(&rect, kPaneFrameWidth, kPaneFrameWidth);
  ::FrameRect(dc, &rect, ::GetSysColorBrush(COLOR_3DSHADOW));
}

LRESULT CPanel::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // g_NumMessages++;
  switch (message)
  {
    case WM_PAINT:
    {
      PAINTSTRUCT paint;
      HDC dc = ::BeginPaint(*this, &paint);
      DrawControlFrame(*this, _headerReBar, dc);
      DrawControlFrame(*this, _listView, dc);
      DrawControlFrame(*this, _statusBar, dc, kPaneFrameWidth);
      ::EndPaint(*this, &paint);
      return 0;
    }
    case kShiftSelectMessage:
      OnShiftSelectMessage();
      return 0;
    case kReLoadMessage:
      RefreshListCtrl(_selectedState);
      return 0;
    case kSetFocusToListView:
      _listView.SetFocus();
      return 0;
    case kOpenItemChanged:
      return OnOpenItemChanged(lParam);
    case kRefresh_StatusBar:
      if (_processStatusBar)
        Refresh_StatusBar();
      return 0;
    case WM_TIMER:
      OnTimer();
      return 0;
    case WM_CONTEXTMENU:
      if (OnContextMenu(HANDLE(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
      break;
    /*
    case WM_DROPFILES:
      CompressDropFiles(HDROP(wParam));
      return 0;
    */
  }
  return CWindow2::OnMessage(message, wParam, lParam);
}

LRESULT CMyListView::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_GETOBJECT)
  {
    LRESULT result;
    if (GetAutomationObject(wParam, lParam, result))
      return result;
  }
  if (message == WM_NCDESTROY)
    CloseAutomation();
  if (message == kFileListAutomationInvoke)
  {
    InvokeAutomationItem(wParam, lParam);
    return 0;
  }

  if (message == WM_CHAR)
  {
    UINT scanCode = (UINT)((lParam >> 16) & 0xFF);
    bool extended = ((lParam & 0x1000000) != 0);
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_MULTIPLY || virtualKey == VK_ADD ||
        virtualKey == VK_SUBTRACT)
      return 0;
    if ((wParam == '/' && extended)
        || wParam == '\\' || wParam == '/')
    {
      _panel->OpenDrivesFolder();
      return 0;
    }
  }
  else if (message == WM_SYSCHAR)
  {
    // For Alt+Enter Beep disabling
    UINT scanCode = (UINT)(lParam >> 16) & 0xFF;
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_RETURN || virtualKey == VK_MULTIPLY ||
        virtualKey == VK_ADD || virtualKey == VK_SUBTRACT)
      return 0;
  }
  /*
  else if (message == WM_SYSKEYDOWN)
  {
    // return 0;
  }
  */
  else if (message == WM_KEYDOWN)
  {
    bool alt = IsKeyDown(VK_MENU);
    bool ctrl = IsKeyDown(VK_CONTROL);
    bool shift = IsKeyDown(VK_SHIFT);
    switch (wParam)
    {
      /*
      case VK_RETURN:
      {
        if (shift && !alt && !ctrl)
        {
          _panel->OpenSelectedItems(false);
          return 0;
        }
        break;
      }
      */
      case VK_NEXT:
      {
        if (ctrl && !alt && !shift)
        {
          _panel->OpenFocusedItemAsInternal();
          return 0;
        }
        break;
      }
      case VK_PRIOR:
      if (ctrl && !alt && !shift)
      {
        _panel->OpenParentFolder();
        return 0;
      }
    }
  }
  else if (message == WM_SETFOCUS)
  {
    _panel->_lastFocusedIsList = true;
    _panel->_panelCallback->PanelWasFocused();
  }
  const LRESULT result = CListView2::OnMessage(message, wParam, lParam);
  if (message == LVM_DELETEALLITEMS)
    NotifyAutomationViewport(true, true);
  else if (message == LVM_SORTITEMS || message == LVM_SORTITEMSEX)
    NotifyAutomationViewport(true);
  else if (message == WM_PAINT || message == WM_SIZE)
    NotifyAutomationViewport();
  return result;
}

/*
static LRESULT APIENTRY ComboBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBox *w = (CMyComboBox *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

LRESULT CMyComboBox::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}
*/


static LRESULT APIENTRY ComboBoxEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBoxEdit *w = (CMyComboBoxEdit *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}


LRESULT CMyComboBoxEdit::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // See MSDN / Subclassing a Combo Box / Creating a Combo-box Toolbar
  switch (message)
  {
    case WM_SYSKEYDOWN:
      switch (wParam)
      {
        case VK_F1:
        case VK_F2:
        {
          // check ALT
          if ((lParam & (1<<29)) == 0)
            break;
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (alt && !ctrl && !shift)
          {
            _panel->_panelCallback->SetFocusToPath(wParam == VK_F1 ? 0 : 1);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_KEYDOWN:
      switch (wParam)
      {
        case VK_TAB:
          // SendMessage(hwndMain, WM_ENTER, 0, 0);
          _panel->SetFocusToList();
          return 0;
        case VK_F9:
        {
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (!alt && !ctrl && !shift)
          {
            g_App.SwitchOnOffOnePanel();
            return 0;
          }
          break;
        }
        case 'W':
        {
          bool ctrl = IsKeyDown(VK_CONTROL);
          if (ctrl)
          {
            PostMessage(g_HWND, WM_COMMAND, IDCLOSE, 0);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_CHAR:
      switch (wParam)
      {
        case VK_TAB:
        case VK_ESCAPE:
          return 0;
      }
  }
    return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}


bool CPanel::OnCreate(CREATESTRUCT * /* createStruct */)
{
  // _virtualMode = false;
  // _sortIndex = 0;
  _sortID = kpidName;
  _ascending = true;
  _lastFocusedIsList = true;

  DWORD style = WS_CHILD | WS_VISIBLE; //  | WS_BORDER ; // | LVS_SHAREIMAGELISTS; //  | LVS_SHOWSELALWAYS;

  style |= LVS_SHAREIMAGELISTS;
  // style  |= LVS_AUTOARRANGE;
  style |= WS_CLIPCHILDREN;
  style |= WS_CLIPSIBLINGS;

  const UInt32 kNumListModes = Z7_ARRAY_SIZE(kStyles);
  if (_listViewMode >= kNumListModes)
    _listViewMode = kNumListModes - 1;

  style |= kStyles[_listViewMode]
    | WS_TABSTOP
    | LVS_EDITLABELS;
  if (_mySelectMode)
    style |= LVS_SINGLESEL;

  /*
  if (_virtualMode)
    style |= LVS_OWNERDATA;
  */

  if (!_listView.CreateEx(0, style, 0, 0, 116, 260,
      *this, (HMENU)(UINT_PTR)(_baseID + 1), g_hInstance, NULL))
    return false;

  _listView.SetUnicodeFormat();
  _listView._panel = this;
  _listView.SetWindowProc();

  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(true), LVSIL_SMALL);
  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(false), LVSIL_NORMAL);

  // _exStyle |= LVS_EX_HEADERDRAGDROP;
  // DWORD extendedStyle = _listView.GetExtendedListViewStyle();
  // extendedStyle |= _exStyle;
  //  _listView.SetExtendedListViewStyle(extendedStyle);
  SetExtendedStyle();

  _listView.Show(SW_SHOW);
  _listView.InvalidateRect(NULL, true);
  _listView.Update();
  
  // Ensure that the common control DLL is loaded.
  INITCOMMONCONTROLSEX icex;

  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  const TBBUTTON tbb[] =
  {
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    {VIEW_PARENTFOLDER, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, { 0, 0 }, 0, 0 },
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    // {VIEW_NEWFOLDER, kCreateFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
  };

  {
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // if there is no CCS_NOPARENTALIGN, there is space of some pixels after rebar (Incorrect GetWindowRect ?)

    _headerReBar.Attach(::CreateWindowEx(WS_EX_TOOLWINDOW,
      REBARCLASSNAME,
      NULL, WS_VISIBLE | WS_CHILD |
      WS_CLIPCHILDREN | WS_CLIPSIBLINGS
      | CCS_NODIVIDER
      | CCS_NOPARENTALIGN
      | CCS_TOP
      | RBS_VARHEIGHT
      ,0,0,0,0, *this, NULL, g_hInstance, NULL));
    _headerReBar.SendMsg(RB_SETBKCOLOR, 0, ::GetSysColor(COLOR_WINDOW));
  }

  DWORD toolbarStyle =  WS_CHILD | WS_VISIBLE ;
  if (_headerReBar)
  {
    toolbarStyle |= 0
      // | WS_CLIPCHILDREN
      // | WS_CLIPSIBLINGS

      | TBSTYLE_TOOLTIPS
      | CCS_NODIVIDER
      | CCS_NORESIZE
      | TBSTYLE_FLAT
      | TBSTYLE_TRANSPARENT
      ;
  }

  _headerToolBar.Attach(::CreateToolbarEx ((*this), toolbarStyle,
      _baseID + 2, 11,
      (HINSTANCE)HINST_COMMCTRL,
      IDB_VIEW_SMALL_COLOR,
      (LPCTBBUTTON)&tbb, Z7_ARRAY_SIZE(tbb),
      0, 0, 0, 0, sizeof (TBBUTTON)));

  // Load ComboBoxEx class
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_USEREX_CLASSES;
  InitCommonControlsEx(&icex);
  
  _headerComboBox.CreateEx(0,
      WC_COMBOBOXEXW
      , NULL,
    WS_VISIBLE | WS_CHILD | CBS_DROPDOWN | CBS_AUTOHSCROLL,
      0, 0, 100, 620,
      (_headerReBar ? _headerToolBar : (HWND)*this),
      (HMENU)(UINT_PTR)(_comboBoxID),
      g_hInstance, NULL);

  _headerComboBox.SetUnicodeFormat(true);
  _headerComboBox.SetImageList(Shell_Get_SysImageList_smallIcons(true));
  _headerComboBox.SetExtendedStyle(CBES_EX_PATHWORDBREAKPROC, CBES_EX_PATHWORDBREAKPROC);
  /*
  _headerComboBox.SetUserDataLongPtr(LONG_PTR(&_headerComboBox));
  _headerComboBox._panel = this;
  _headerComboBox._origWindowProc =
      (WNDPROC)_headerComboBox.SetLongPtr(GWLP_WNDPROC,
      LONG_PTR(ComboBoxSubclassProc));
  */
  _comboBoxEdit.Attach(_headerComboBox.GetEditControl());
  // _comboBoxEdit.SendMessage(CCM_SETUNICODEFORMAT, (WPARAM)(BOOL)TRUE, 0);
  _comboBoxEdit.SetUserDataLongPtr(LONG_PTR(&_comboBoxEdit));
  _comboBoxEdit._panel = this;
     _comboBoxEdit._origWindowProc =
      (WNDPROC)_comboBoxEdit.SetLongPtr(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));

  if (_headerReBar)
  {
    REBARINFO     rbi;
    rbi.cbSize = sizeof(REBARINFO);  // Required when using this struct.
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    _headerReBar.SetBarInfo(&rbi);
    
    // Send the TB_BUTTONSTRUCTSIZE message, which is required for
    // backward compatibility.
    // _headerToolBar.SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SIZE size;
    _headerToolBar.GetMaxSize(&size);
    
    REBARBANDINFO rbBand;
    memset(&rbBand, 0, sizeof(rbBand));
    rbBand.cbSize = sizeof(rbBand);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    rbBand.fStyle = RBBS_NOGRIPPER;
    rbBand.cxMinChild = (UINT)size.cx;
    rbBand.cyMinChild = (UINT)size.cy;
    rbBand.cyChild = (UINT)size.cy;
    rbBand.cx = rbBand.cxMinChild;
    rbBand.hwndChild  = _headerToolBar;
    _headerReBar.InsertBand(-1, &rbBand);

    RECT rc;
    ::GetWindowRect(_headerComboBox, &rc);
    rbBand.cxMinChild = 30;
    rbBand.cyMinChild = (UINT)(rc.bottom - rc.top);
    rbBand.cx = 1000;
    rbBand.hwndChild  = _headerComboBox;
    _headerReBar.InsertBand(-1, &rbBand);
    // _headerReBar.MaximizeBand(1, false);
  }

  _statusBar.Create(WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN
      | SBARS_TOOLTIPS
      , L"", (*this), _statusBarID);
  StyleStatusBar(_statusBar);
  UpdateDpi(NFileManagerStyle::GetDpi(*this), g_App.GetControlFont());

  RECT rect;
  GetClientRect(&rect);
  ChangeWindowSize(RECT_SIZE_X(rect), RECT_SIZE_Y(rect));

  SetTimer(kTimerID, kTimerElapse);

  // InitListCtrl();
  RefreshListCtrl();
  
  return true;
}

void CPanel::OnDestroy()
{
  SaveListViewInfo();
  CWindow2::OnDestroy();
}

void CPanel::UpdateDpi(int dpi, HFONT font)
{
  if (dpi != _columnDpi)
  {
    FOR_VECTOR (i, _visibleColumns)
    {
      LVCOLUMNW column = {};
      column.mask = LVCF_WIDTH;
      if (_listView.GetColumn(i, &column))
      {
        column.cx = ::MulDiv(column.cx, dpi, _columnDpi);
        ListView_SetColumn((HWND)_listView, i, &column);
      }
    }
    _columnDpi = dpi;
  }

  if (font)
  {
    _listView.SendMsg(WM_SETFONT, (WPARAM)font, FALSE);
    const HWND header = ListView_GetHeader((HWND)_listView);
    if (header)
      ::SendMessageW(header, WM_SETFONT, (WPARAM)font, FALSE);
    _headerComboBox.SendMsg(WM_SETFONT, (WPARAM)font, FALSE);
    _statusBar.SendMsg(WM_SETFONT, (WPARAM)font, FALSE);
    _statusBar.SendMsg(WM_SIZE, 0, 0);
  }

  if (_headerReBar)
  {
    SIZE size = {};
    if (_headerToolBar.GetMaxSize(&size))
    {
      REBARBANDINFO band = {};
      band.cbSize = sizeof(band);
      band.fMask = RBBIM_CHILDSIZE;
      band.cxMinChild = (UINT)size.cx;
      band.cyMinChild = (UINT)size.cy;
      _headerReBar.SetBandInfo(0, &band);

      band.cxMinChild = (UINT)::MulDiv(30, dpi, USER_DEFAULT_SCREEN_DPI);
      band.cyMinChild = (UINT)::MulDiv(24, dpi, USER_DEFAULT_SCREEN_DPI);
      _headerReBar.SetBandInfo(1, &band);
    }
  }
  if (_thereAre_ListView_Items)
    Refresh_StatusBar();
}

void CPanel::ChangeWindowSize(int xSize, int ySize)
{
  if (!(HWND)*this || !_listView || !_statusBar || !_headerToolBar)
    return;

  const int inset = NFileManagerStyle::Scale(*this, 10);
  const int gap = NFileManagerStyle::Scale(*this, 8);
  const int addressTop = GetAddressBarTop(inset);
  const int width = MyMax(xSize - 2 * inset, 0);
  RECT rect = {};
  if (_headerReBar)
    _headerReBar.GetWindowRect(&rect);
  else
    _headerToolBar.GetWindowRect(&rect);
  const int listTop = addressTop + RECT_SIZE_Y(rect) + gap;

  _statusBar.GetWindowRect(&rect);
  const int statusHeight = RECT_SIZE_Y(rect);
  const int listHeight = MyMax(ySize - listTop - statusHeight - gap - inset, 0);

  if (!_headerReBar)
  {
    const int pathOffset = NFileManagerStyle::Scale(*this, 32);
    _headerToolBar.Move(inset, addressTop, width, 0);
    _headerComboBox.Move(inset + pathOffset, addressTop + 2, MyMax(width - pathOffset, 0), 0);
  }
  else
  {
    // Keep the combo's left edge as a divider inside the rebar's frame.
    const int edgeY = ::GetSystemMetrics(SM_CYBORDER);
    ClipControlEdges(_headerComboBox, edgeY, ::GetSystemMetrics(SM_CXBORDER), edgeY);
  }

  _listView.Move(inset, listTop, width, listHeight);
  _statusBar.Move(inset, listTop + listHeight + gap, width, statusHeight);
  // Replace the native top line with the pane's single-pixel frame.
  ClipControlEdges(_statusBar, kPaneFrameWidth, 0, 0);
  LayoutStatusBar(width);
  InvalidateRect(NULL);
}

bool CPanel::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  if (!(HWND)*this)
    return true;
  if (_headerReBar)
  {
    const int inset = NFileManagerStyle::Scale(*this, 10);
    _headerReBar.Move(inset, GetAddressBarTop(inset), MyMax(xSize - 2 * inset, 0), 0);
  }
  ChangeWindowSize(xSize, ySize);
  return true;
}

bool CPanel::OnNotifyReBar(LPNMHDR header, LRESULT & /* result */)
{
  switch (header->code)
  {
    case RBN_HEIGHTCHANGE:
    {
      RECT rect;
      GetWindowRect(&rect);
      ChangeWindowSize(RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
      return false;
    }
  }
  return false;
}

/*
UInt32 g_OnNotify = 0;
UInt32 g_LVIF_TEXT = 0;
UInt32 g_Time = 0;

void Print_OnNotify(const char *name)
{
  char s[256];
  DWORD tim = GetTickCount();
  sprintf(s,
      "Time = %7u ms, Notify = %9u, TEXT = %9u, %s",
      tim - g_Time,
      g_OnNotify,
      g_LVIF_TEXT,
      name);
  g_Time = tim;
  OutputDebugStringA(s);
  g_OnNotify = 0;
  g_LVIF_TEXT = 0;
}
*/

bool CPanel::OnNotify(UINT /* controlID */, LPNMHDR header, LRESULT &result)
{
  /*
  g_OnNotify++;

  if (header->hwndFrom == _listView)
  {
    if (header->code == LVN_GETDISPINFOW)
    {
      LV_DISPINFOW *dispInfo = (LV_DISPINFOW *)header;
        if ((dispInfo->item.mask & LVIF_TEXT))
          g_LVIF_TEXT++;
    }
  }
  */

  if (!_processNotify)
    return false;

  if (header->hwndFrom == _headerComboBox)
    return OnNotifyComboBox(header, result);
  else if (header->hwndFrom == _headerReBar)
    return OnNotifyReBar(header, result);
  else if (header->hwndFrom == _listView)
    return OnNotifyList(header, result);
  else if (::GetParent(header->hwndFrom) == _listView)
  {
    // NMHDR:code is UINT
    // NM_RCLICK is unsigned in windows sdk
    // NM_RCLICK is int      in MinGW
    if (header->code == (UINT)NM_RCLICK)
      return OnRightClick((MY_NMLISTVIEW_NMITEMACTIVATE *)header, result);
  }
  return false;
}

bool CPanel::OnCommand(unsigned code, unsigned itemID, LPARAM lParam, LRESULT &result)
{
  if (itemID == kParentFolderID)
  {
    OpenParentFolder();
    result = 0;
    return true;
  }
  /*
  if (itemID == kCreateFolderID)
  {
    CreateFolder();
    result = 0;
    return true;
  }
  */
  if (itemID == _comboBoxID)
  {
    if (OnComboBoxCommand(code, lParam, result))
      return true;
  }
  return CWindow2::OnCommand(code, itemID, lParam, result);
}



/*
void CPanel::MessageBox_Info(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK); }
void CPanel::MessageBox_Warning(LPCWSTR message) const
  { ::MessageBoxW((HWND)*this, message, L"7-Zip", MB_OK | MB_ICONWARNING); }
*/

void CPanel::MessageBox_Error_Caption(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK | MB_ICONSTOP); }

void CPanel::MessageBox_Error(LPCWSTR message) const
  { MessageBox_Error_Caption(message, L"7-Zip"); }

static UString ErrorHResult_To_Message(HRESULT errorCode)
{
  if (errorCode == 0)
    errorCode = E_FAIL;
  return HResultToMessage(errorCode);
}

void CPanel::MessageBox_Error_HRESULT_Caption(HRESULT errorCode, LPCWSTR caption) const
{
  MessageBox_Error_Caption(ErrorHResult_To_Message(errorCode), caption);
}

void CPanel::MessageBox_Error_HRESULT(HRESULT errorCode) const
  { MessageBox_Error_HRESULT_Caption(errorCode, L"7-Zip"); }

void CPanel::MessageBox_Error_2Lines_Message_HRESULT(LPCWSTR message, HRESULT errorCode) const
{
  UString m = message;
  m.Add_LF();
  m += ErrorHResult_To_Message(errorCode);
  MessageBox_Error(m);
}

void CPanel::MessageBox_LastError(LPCWSTR caption) const
  { MessageBox_Error_HRESULT_Caption(GetLastError_noZero_HRESULT(), caption); }

void CPanel::MessageBox_LastError() const
  { MessageBox_LastError(L"7-Zip"); }

void CPanel::MessageBox_Error_LangID(UINT resourceID) const
  { MessageBox_Error(LangString(resourceID)); }

void CPanel::MessageBox_Error_UnsupportOperation() const
  { MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED); }




void CPanel::SetFocusToList()
{
  _listView.SetFocus();
  // SetCurrentPathText();
}

void CPanel::SetFocusToLastRememberedItem()
{
  if (_lastFocusedIsList)
    SetFocusToList();
  else
    _headerComboBox.SetFocus();
}

UString CPanel::GetFolderTypeID() const
{
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidType, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        return (const wchar_t *)prop.bstrVal;
  }
  return UString();
}

bool CPanel::IsFolderTypeEqTo(const char *s) const
{
  return StringsAreEqual_Ascii(GetFolderTypeID(), s);
}

bool CPanel::IsRootFolder() const { return IsFolderTypeEqTo("RootFolder"); }
bool CPanel::IsFSFolder() const { return IsFolderTypeEqTo("FSFolder"); }
bool CPanel::IsFSDrivesFolder() const { return IsFolderTypeEqTo("FSDrives"); }
bool CPanel::IsAltStreamsFolder() const { return IsFolderTypeEqTo("AltStreamsFolder"); }
bool CPanel::IsArcFolder() const
{
  return GetFolderTypeID().IsPrefixedBy_Ascii_NoCase("7-Zip");
}

bool CPanel::IsHashFolder() const
{
  if (_folder)
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidIsHash, &prop) == S_OK)
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
  }
  return false;
}

UString CPanel::GetFsPath() const
{
  if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix() && !IsSuperDrivesPrefix())
    return UString();
  return _currentFolderPrefix;
}

UString CPanel::GetDriveOrNetworkPrefix() const
{
  if (!IsFSFolder())
    return UString();
  UString drive = GetFsPath();
  drive.DeleteFrom(NFile::NName::GetRootPrefixSize(drive));
  return drive;
}

void CPanel::SetListViewMode(UInt32 index)
{
  if (index >= 4)
    return;
  _listViewMode = index;
  const LONG_PTR oldStyle = _listView.GetStyle();
  const DWORD newStyle = kStyles[index];

  // DWORD tickCount1 = GetTickCount();
  if ((oldStyle & LVS_TYPEMASK) != (LONG_PTR)newStyle)
    _listView.SetStyle((oldStyle & ~(LONG_PTR)(DWORD)LVS_TYPEMASK) | (LONG_PTR)newStyle);
  // RefreshListCtrlSaveFocused();
  /*
  DWORD tickCount2 = GetTickCount();
  char s[256];
  sprintf(s, "SetStyle = %5d",
      tickCount2 - tickCount1
      );
  OutputDebugStringA(s);
  */

}

void CPanel::ChangeFlatMode()
{
  _flatMode = !_flatMode;
  if (!_parentFolders.IsEmpty())
    _flatModeForArc = _flatMode;
  else
    _flatModeForDisk = _flatMode;
  RefreshListCtrl_SaveFocused();
}

/*
void CPanel::Change_ShowNtfsStrems_Mode()
{
  _showNtfsStrems_Mode = !_showNtfsStrems_Mode;
  if (!_parentFolders.IsEmpty())
    _showNtfsStrems_ModeForArc = _showNtfsStrems_Mode;
  else
    _showNtfsStrems_ModeForDisk = _showNtfsStrems_Mode;
  RefreshListCtrlSaveFocused();
}
*/

void CPanel::Post_Refresh_StatusBar()
{
  if (_processStatusBar)
    PostMsg(kRefresh_StatusBar);
}

void CPanel::AddToArchive()
{
  if (!Is_IO_FS_Folder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() == 0)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UString destCurDirPrefix = GetFsPath();
  if (IsFSDrivesFolder())
    destCurDirPrefix = ROOT_FS_FOLDER;
  UStringVector names;
  GetFilePaths(indices, names);
  UString baseName;
  const UString arcName = CreateArchiveName(names,
      false, // isHash
      NULL,  // CFileInfo *fi
      baseName);
  const HRESULT res = CompressFiles(destCurDirPrefix, arcName, L"",
      true,   // addExtension
      names,
      false,  // email
      true,   // showDialog
      false); // waitFinish
  if (res != S_OK)
  {
    if (destCurDirPrefix.Len() >= MAX_PATH)
      MessageBox_Error_LangID(IDS_MESSAGE_UNSUPPORTED_OPERATION_FOR_LONG_PATH_FOLDER);
  }
  // KillSelection();
}

// function from ContextMenu.cpp
UString GetSubFolderNameForExtract(const UString &arcPath);

static UString GetSubFolderNameForExtract2(const UString &arcPath)
{
  int slashPos = arcPath.ReverseFind_PathSepar();
  UString s;
  UString name = arcPath;
  if (slashPos >= 0)
  {
    s = arcPath.Left((unsigned)(slashPos + 1));
    name = arcPath.Ptr((unsigned)(slashPos + 1));
  }
  s += GetSubFolderNameForExtract(name);
  return s;
}


int CPanel::FindDir_InOperatedList(const CRecordVector<UInt32> &operatedIndices) const
{
  const bool *isDirVector = _isDirVector.ConstData();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  for (unsigned i = 0; i < numItems; i++)
    if (isDirVector[indices[i]])
      return (int)i;
  return -1;
}


void CPanel::GetFilePaths(const CRecordVector<UInt32> &operatedIndices, UStringVector &paths) const
{
  paths.ClearAndReserve(operatedIndices.Size());
  UString path = GetFsPath();
  const unsigned prefixLen = path.Len();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  // for (unsigned y = 0; y < 10000; y++, paths.Clear())
  for (unsigned i = 0; i < numItems; i++)
  {
    path.DeleteFrom(prefixLen);
    Add_ItemRelPath2_To_String(indices[i], path);
    // ODS_U(path)
    paths.AddInReserved(path);
  }
}


void CPanel::ExtractArchives()
{
  if (!_parentFolders.IsEmpty())
  {
    _panelCallback->OnCopy(false, false);
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty() || FindDir_InOperatedList(indices) != -1)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  UString outFolder = GetFsPath();
  if (indices.Size() == 1)
    outFolder += GetSubFolderNameForExtract2(GetItemRelPath(indices[0]));
  else
    outFolder.Add_Char('*');
  outFolder.Add_PathSepar();
  
  CContextMenuInfo ci;
  ci.Load();

  ::ExtractArchives(paths, outFolder
      , true   // showDialog
      , false  // elimDup
      , ci.WriteZone
      );
}

/*
static void AddValuePair(UINT resourceID, UInt64 value, UString &s)
{
  AddLangString(s, resourceID);
  char sz[32];
  s += ": ";
  ConvertUInt64ToString(value, sz);
  s += sz;
  s.Add_LF();
}

// now we don't need CThreadTest, since now we call CopyTo for "test command

class CThreadTest: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  CRecordVector<UInt32> Indices;
  CExtractCallbackImp *ExtractCallbackSpec;
  CMyComPtr<IFolderArchiveExtractCallback> ExtractCallback;
  CMyComPtr<IArchiveFolder> ArchiveFolder;
};

HRESULT CThreadTest::ProcessVirt()
{
  RINOK(ArchiveFolder->Extract(&Indices[0], Indices.Size(),
      true, // includeAltStreams
      false, // replaceAltStreamColon
      NExtract::NPathMode::kFullPathnames,
      NExtract::NOverwriteMode::kAskBefore,
      NULL, // path
      BoolToInt(true), // testMode
      ExtractCallback));
  if (ExtractCallbackSpec->IsOK())
  {
    UString s;
    AddValuePair(IDS_PROP_FOLDERS, ExtractCallbackSpec->NumFolders, s);
    AddValuePair(IDS_PROP_FILES, ExtractCallbackSpec->NumFiles, s);
    // AddValuePair(IDS_PROP_SIZE, ExtractCallbackSpec->UnpackSize, s);
    // AddSizePair(IDS_COMPRESSED_COLON, Stat.PackSize, s);
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
    FinalMessage.OkMessage.Message = s;
  }
  return S_OK;
}

static void AddSizePair(UInt32 langID, UInt64 value, UString &s)
{
  char sz[32];
  AddLangString(s, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  ConvertUInt64ToString(value >> 20, sz);
  s += " (";
  s += sz;
  s += " MB)";
  s.Add_LF();
}
*/

void CPanel::TestArchives()
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_OperSmart(indices);
  CMyComPtr<IArchiveFolder> archiveFolder;
  _folder.QueryInterface(IID_IArchiveFolder, &archiveFolder);
  if (archiveFolder)
  {
    CCopyToOptions options;
    options.streamMode = true;
    options.showErrorMessages = true;
    options.testMode = true;

    UStringVector messages;
    HRESULT res = CopyTo(options, indices, &messages);
    if (res != S_OK)
    {
      if (res != E_ABORT)
        MessageBox_Error_HRESULT(res);
    }
    return;

    /*
    {
    CThreadTest extracter;

    extracter.ArchiveFolder = archiveFolder;
    extracter.ExtractCallbackSpec = new CExtractCallbackImp;
    extracter.ExtractCallback = extracter.ExtractCallbackSpec;
    extracter.ExtractCallbackSpec->ProgressDialog = &extracter.ProgressDialog;
    if (!_parentFolders.IsEmpty())
    {
      const CFolderLink &fl = _parentFolders.Back();
      extracter.ExtractCallbackSpec->PasswordIsDefined = fl.UsePassword;
      extracter.ExtractCallbackSpec->Password = fl.Password;
    }

    if (indices.IsEmpty())
      return;

    extracter.Indices = indices;
    
    const UString title = LangString(IDS_PROGRESS_TESTING);
    
    extracter.ProgressDialog.CompressingMode = false;
    extracter.ProgressDialog.MainWindow = GetParent();
    extracter.ProgressDialog.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
    extracter.ProgressDialog.MainAddTitle = title + L' ';
    
    extracter.ExtractCallbackSpec->OverwriteMode = NExtract::NOverwriteMode::kAskBefore;
    extracter.ExtractCallbackSpec->Init();
    
    if (extracter.Create(title, GetParent()) != S_OK)
      return;
    
    }
    RefreshTitleAlways();
    return;
    */
  }

  if (!IsFSFolder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  if (paths.IsEmpty())
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  ::TestArchives(paths);
}
