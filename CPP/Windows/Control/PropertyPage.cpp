// Windows/Control/PropertyPage.cpp
// Modified 2026-11-17 for the fork: hide the property sheet's title-bar Help button.

#include "StdAfx.h"


#include "PropertyPage.h"

extern HINSTANCE g_hInstance;

namespace NWindows {
namespace NControl {

static
  INT_PTR
APIENTRY MyProperyPageProcedure(HWND dialogHWND, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(dialogHWND);
  if (message == WM_INITDIALOG)
    tempDialog.SetUserDataLongPtr(((PROPSHEETPAGE *)lParam)->lParam);
  CDialog *dialog = (CDialog *)(tempDialog.GetUserDataLongPtr());
  if (dialog == NULL)
    return FALSE;
  if (message == WM_INITDIALOG)
    dialog->Attach(dialogHWND);
  try { return BoolToBOOL(dialog->OnMessage(message, wParam, lParam)); }
  catch(...) { return TRUE; }
}

bool CPropertyPage::OnNotify(UINT /* controlID */, LPNMHDR lParam)
{
  switch (lParam->code)
  {
    case PSN_APPLY: SetMsgResult(OnApply2(LPPSHNOTIFY(lParam))); break;
    case PSN_KILLACTIVE: SetMsgResult(BoolToBOOL(OnKillActive2(LPPSHNOTIFY(lParam)))); break;
    case PSN_SETACTIVE: SetMsgResult(OnSetActive2(LPPSHNOTIFY(lParam))); break;
    case PSN_RESET: OnReset2(LPPSHNOTIFY(lParam)); break;
    case PSN_HELP: OnNotifyHelp2(LPPSHNOTIFY(lParam)); break;
    default: return false;
  }
  return true;
}

INT_PTR MyPropertySheet(const CObjectVector<CPageInfo> &pagesInfo, HWND hwndParent, const UString &title)
{
  const DWORD flags = PSH_PROPSHEETPAGE
    #ifdef PSH_NOCONTEXTHELP
    | PSH_NOCONTEXTHELP
    #endif
    ;
  unsigned i;
  CRecordVector<PROPSHEETPAGEW> pagesW;

  for (i = 0; i < pagesInfo.Size(); i++)
  {
    const CPageInfo &pageInfo = pagesInfo[i];
    {
      PROPSHEETPAGEW page;
      memset(&page, 0, sizeof(page));
      page.dwSize = sizeof(page);
      page.dwFlags = PSP_HASHELP;
      page.hInstance = g_hInstance;
      page.pszTemplate = MAKEINTRESOURCEW(pageInfo.ID);
      // page.pszIcon = NULL;
      page.pfnDlgProc = NWindows::NControl::MyProperyPageProcedure;
      
      if (!pageInfo.Title.IsEmpty())
      {
        page.pszTitle = pageInfo.Title;
        page.dwFlags |= PSP_USETITLE;
      }
      // else page.pszTitle = NULL;
      page.lParam = (LPARAM)pageInfo.Page;
      // page.pfnCallback = NULL;
      pagesW.Add(page);
    }
  }

  {
    PROPSHEETHEADERW sheet;
    sheet.dwSize = sizeof(sheet);
    sheet.dwFlags = flags;
    sheet.hwndParent = hwndParent;
    sheet.hInstance = g_hInstance;
    sheet.pszCaption = title;
    sheet.nPages = pagesW.Size();
    sheet.nStartPage = 0;
    sheet.ppsp = pagesW.ConstData();
    sheet.pfnCallback = NULL;
    return ::PropertySheetW(&sheet);
  }
}

}}
