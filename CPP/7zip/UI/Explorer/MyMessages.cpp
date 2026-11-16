// MyMessages.cpp
// Modified 2026-11-16 for the fork: name Explorer error dialogs after the product.

#include "StdAfx.h"

#include "MyMessages.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/ResourceString.h"

#include "../../MyVersion.h"
#include "../FileManager/LangUtils.h"

using namespace NWindows;

extern bool g_DisableUserQuestions;

void ShowErrorMessage(HWND window, LPCWSTR message)
{
  if (!g_DisableUserQuestions)
    ::MessageBoxW(window, message, UString(MY_PRODUCT_NAME), MB_OK | MB_ICONSTOP);
}

void ShowErrorMessageHwndRes(HWND window, UInt32 resID)
{
  UString s = LangString(resID);
  if (s.IsEmpty())
    s.Add_UInt32(resID);
  ShowErrorMessage(window, s);
}

void ShowErrorMessageRes(UInt32 resID)
{
  ShowErrorMessageHwndRes(NULL, resID);
}

static void ShowErrorMessageDWORD(HWND window, DWORD errorCode)
{
  ShowErrorMessage(window, NError::MyFormatMessage(errorCode));
}

void ShowLastErrorMessage(HWND window)
{
  ShowErrorMessageDWORD(window, ::GetLastError());
}
