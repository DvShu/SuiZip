// FileManagerStyle.h

#ifndef ZIP7_INC_FILE_MANAGER_STYLE_H
#define ZIP7_INC_FILE_MANAGER_STYLE_H

namespace NFileManagerStyle {

inline int GetDpi(HWND window)
{
  typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
  static const GetDpiForWindowFunc getDpiForWindow = Z7_GET_PROC_ADDRESS(
      GetDpiForWindowFunc, ::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
  if (getDpiForWindow && window)
  {
    const UINT dpi = getDpiForWindow(window);
    if (dpi)
      return (int)dpi;
  }

  HDC dc = ::GetDC(window);
  if (!dc)
    return USER_DEFAULT_SCREEN_DPI;
  const int dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
  ::ReleaseDC(window, dc);
  return dpi > 0 ? dpi : USER_DEFAULT_SCREEN_DPI;
}

inline int Scale(HWND window, int pixels)
{
  return ::MulDiv(pixels, GetDpi(window), USER_DEFAULT_SCREEN_DPI);
}

inline COLORREF BlendColors(COLORREF background, COLORREF foreground, unsigned strength)
{
  const unsigned base = 100 - strength;
  return RGB((GetRValue(background) * base + GetRValue(foreground) * strength) / 100,
             (GetGValue(background) * base + GetGValue(foreground) * strength) / 100,
             (GetBValue(background) * base + GetBValue(foreground) * strength) / 100);
}

inline COLORREF TintWithAccent(COLORREF background, unsigned strength)
{
  return BlendColors(background, ::GetSysColor(COLOR_HIGHLIGHT), strength);
}

}

#endif
