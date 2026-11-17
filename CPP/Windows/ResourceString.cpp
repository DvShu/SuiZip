// Windows/ResourceString.cpp

#include "StdAfx.h"


#include "ResourceString.h"

extern HINSTANCE g_hInstance;

namespace NWindows {


static const int kStartSize = 256;

static void MyLoadString2(HINSTANCE hInstance, UINT resourceID, UString &s)
{
  int size = kStartSize;
  int len;
  do
  {
    size <<= 1;
    len = ::LoadStringW(hInstance, resourceID, s.GetBuf((unsigned)size - 1), size);
  }
  while (size - len <= 1);
  s.ReleaseBuf_CalcLen((unsigned)len);
}

// NT4 doesn't support LoadStringW(,,, 0) to get pointer to resource string. So we don't use it.

UString MyLoadString(UINT resourceID)
{
  {
    {
      wchar_t s[kStartSize];
      s[0] = 0;
      int len = ::LoadStringW(g_hInstance, resourceID, s, kStartSize);
      if (kStartSize - len > 1)
        return s;
    }
    UString dest;
    MyLoadString2(g_hInstance, resourceID, dest);
    return dest;
  }
}

void MyLoadString(HINSTANCE hInstance, UINT resourceID, UString &dest)
{
  dest.Empty();
  {
    {
      wchar_t s[kStartSize];
      s[0] = 0;
      int len = ::LoadStringW(hInstance, resourceID, s, kStartSize);
      if (kStartSize - len > 1)
      {
        dest = s;
        return;
      }
    }
    MyLoadString2(hInstance, resourceID, dest);
  }
}

void MyLoadString(UINT resourceID, UString &dest)
{
  MyLoadString(g_hInstance, resourceID, dest);
}

}
