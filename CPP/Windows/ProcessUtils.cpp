// ProcessUtils.cpp

#include "StdAfx.h"

#include "../Common/StringConvert.h"

#include "ProcessUtils.h"


namespace NWindows {

static UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2.Add_Char('\"');
  return s2;
}

WRes CProcess::Create(LPCWSTR imageName, const UString &params, LPCWSTR curDir)
{
  /*
  OutputDebugStringW(L"CProcess::Create");
  OutputDebugStringW(imageName);
  if (params)
  {
    OutputDebugStringW(L"params:");
    OutputDebugStringW(params);
  }
  if (curDir)
  {
    OutputDebugStringW(L"cur dir:");
    OutputDebugStringW(curDir);
  }
  */

  Close();
  const UString params2 =
      GetQuotedString(imageName) + L' ' +
      params;
  imageName = NULL;
  PROCESS_INFORMATION pi;
  BOOL result;
  {
    STARTUPINFOW si;
    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;
    
    result = CreateProcessW(imageName, params2.Ptr_non_const(),
        NULL, NULL, FALSE, 0, NULL, curDir, &si, &pi);
  }
  if (result == 0)
    return ::GetLastError();
  ::CloseHandle(pi.hThread);
  _handle = pi.hProcess;
  return 0;
}

WRes MyCreateProcess(LPCWSTR imageName, const UString &params)
{
  CProcess process;
  return process.Create(imageName, params, NULL);
}

}
