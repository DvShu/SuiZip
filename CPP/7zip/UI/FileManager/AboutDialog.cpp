// AboutDialog.cpp
// Modified 2026-11-16 for the fork: show its name and version in About.

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../MyVersion.h"

#include "../Common/LoadCodecs.h"

#include "AboutDialog.h"
#include "PropertyNameRes.h"

#include "HelpUtils.h"
#include "LangUtils.h"

#define kHomePageURL TEXT("https://www.7-zip.org/")
#define kHelpTopic "start.htm"

#define LLL_(quote) L##quote
#define LLL(quote) LLL_(quote)

extern CCodecs *g_CodecsObj;

bool CAboutDialog::OnInit()
{
  #ifdef Z7_EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    UString s;
    g_CodecsObj->GetCodecsErrorMessage(s);
    if (!s.IsEmpty())
      MessageBoxW(GetParent(), s, LLL(MY_PRODUCT_NAME), MB_ICONERROR);
  }
  #endif

  UString title;
  #ifdef Z7_LANG
  LangString_OnlyFromLangFile(IDD_ABOUT, title);
  LangSetDlgItems(*this, NULL, 0);
  #endif
  if (title.IsEmpty())
    title = "About " MY_PRODUCT_NAME;
  else
    title.Replace(L"7-Zip", LLL(MY_PRODUCT_NAME));
  NWindows::MySetWindowText(*this, title);
  SetItemText(IDT_ABOUT_VERSION, UString(MY_PRODUCT_NAME " " MY_PRODUCT_VERSION_CPU));
  SetItemText(IDT_ABOUT_DATE, UString("7-Zip source date: " MY_DATE));
  SetItemText(IDT_ABOUT_INFO, UString("Based on 7-Zip " MY_VERSION));
  
  NormalizePosition();
  return CModalDialog::OnInit();
}

void CAboutDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}

bool CAboutDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  LPCTSTR url;
  switch (buttonID)
  {
    case IDB_ABOUT_HOMEPAGE: url = kHomePageURL; break;
    default:
      return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
  }

  #ifdef UNDER_CE
  SHELLEXECUTEINFO s;
  memset(&s, 0, sizeof(s));
  s.cbSize = sizeof(s);
  s.lpFile = url;
  ::ShellExecuteEx(&s);
  #else
  ::ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
  #endif

  return true;
}
