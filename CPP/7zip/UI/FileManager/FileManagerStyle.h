// FileManagerStyle.h

#ifndef ZIP7_INC_FILE_MANAGER_STYLE_H
#define ZIP7_INC_FILE_MANAGER_STYLE_H

namespace NFileManagerStyle {

inline int Scale(HWND window, int pixels)
{
  HDC dc = ::GetDC(window);
  if (!dc)
    return pixels;
  const int dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
  ::ReleaseDC(window, dc);
  return ::MulDiv(pixels, dpi, 96);
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
