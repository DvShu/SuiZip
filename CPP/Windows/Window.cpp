// Windows/Window.cpp

#include "StdAfx.h"

#include "Window.h"


namespace NWindows {



bool CWindow::GetText(CSysString &s) const
{
  s.Empty();
  unsigned len = (unsigned)GetTextLength();
  if (len == 0)
    return (::GetLastError() == ERROR_SUCCESS);
  TCHAR *p = s.GetBuf(len);
  {
    const unsigned len2 = (unsigned)GetText(p, (int)(len + 1));
    if (len > len2)
      len = len2;
  }
  s.ReleaseBuf_CalcLen(len);
  if (len == 0)
    return (::GetLastError() == ERROR_SUCCESS);
  return true;
}


 
/*
bool CWindow::ModifyStyleBase(int styleOffset,
  DWORD remove, DWORD add, UINT flags)
{
  DWORD style = GetWindowLong(styleOffset);
  DWORD newStyle = (style & ~remove) | add;
  if (style == newStyle)
    return false; // it is not good

  SetWindowLong(styleOffset, newStyle);
  if (flags != 0)
  {
    ::SetWindowPos(_window, NULL, 0, 0, 0, 0,
      SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | flags);
  }
  return TRUE;
}
*/

}
