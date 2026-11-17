// Windows/CommonDialog.cpp

#include "StdAfx.h"

#include "../Common/MyBuffer.h"



#include "CommonDialog.h"
#include "WinDefs.h"
// #include "FileDir.h"


namespace NWindows {

/*
  GetSaveFileName()
  GetOpenFileName()
  OPENFILENAME

(lpstrInitialDir) : the initial directory.
DOCs: the algorithm for selecting the initial directory varies on different platforms:
{
  Win2000/XP/Vista:
    1. If lpstrFile contains a path, that path is the initial directory.
    2. Otherwise, lpstrInitialDir specifies the initial directory.

  Win7:
    If lpstrInitialDir has the same value as was passed the first time
    the application used an Open or Save As dialog box, the path
    most recently selected by the user is used as the initial directory.
}

Win10:
 in:
  function supports (lpstrInitialDir) path with super prefix "\\\\?\\"
  function supports (lpstrInitialDir) path with long path
  function doesn't support absolute (lpstrFile) path with super prefix "\\\\?\\"
  function doesn't support absolute (lpstrFile) path with long path
 out: the path with super prefix "\\\\?\\" will be returned, if selected path is long

WinXP-64 and Win10: if no filters, the system shows all files.
    but DOCs say: If all three members are zero or NULL,
        the system does not use any filters and does not
        show any files in the file list control of the dialog box.

in Win7+: GetOpenFileName() and GetSaveFileName()
    do not support pstrCustomFilter feature anymore
*/



bool CCommonDialogInfo::CommonDlg_BrowseForFile(LPCWSTR lpstrInitialDir, const UStringVector &filters)
{
  /* GetSaveFileName() and GetOpenFileName() could change current dir,
     if OFN_NOCHANGEDIR is not used.
     We can restore current dir manually, if it's required.
     22.02: we use OFN_NOCHANGEDIR. So we don't need to restore current dir manually. */
  // NFile::NDir::CCurrentDirRestorer curDirRestorer;

  {
    UString tempPath;
    unsigned size = filters.Size() + 1;
    unsigned i;
    for (i = 0; i < filters.Size(); i++)
      size += filters[i].Len();
    CObjArray<wchar_t> filterBuf(size);
    // memset(filterBuf, 0, size * sizeof(wchar_t));
    {
      wchar_t *dest = filterBuf;
      for (i = 0; i < filters.Size(); i++)
      {
        const UString &s = filters[i];
        MyStringCopy(dest, s);
        dest += s.Len() + 1;
      }
      *dest = 0;
      // if ((unsigned)(dest + 1 - filterBuf) != size) return false;
    }
    OPENFILENAMEW p;
    memset(&p, 0, sizeof(p));
    p.lStructSize = sizeof(p);
    p.hwndOwner = hwndOwner;
    if (size > 1)
    {
      p.lpstrFilter = filterBuf;
      p.nFilterIndex = (DWORD)(FilterIndex + 1);
    }
    unsigned bufSize = MAX_PATH * 8 + FilePath.Len();
    if (lpstrInitialDir)
    {
      p.lpstrInitialDir = lpstrInitialDir;
      bufSize += MyStringLen(lpstrInitialDir);
    }
    p.nMaxFile = bufSize;
    p.lpstrFile = tempPath.GetBuf(bufSize);
    MyStringCopy(p.lpstrFile, FilePath);
    p.lpstrTitle = lpstrTitle;
    p.Flags =
          OFN_EXPLORER
        | OFN_HIDEREADONLY
        | OFN_NOCHANGEDIR
        // | OFN_FORCESHOWHIDDEN // Win10 shows hidden items even without this flag
        // | OFN_PATHMUSTEXIST
        ;
    const BOOL b = SaveMode ?
        ::GetSaveFileNameW((LPOPENFILENAMEW)(void *)&p) :
        ::GetOpenFileNameW((LPOPENFILENAMEW)(void *)&p);
    /* DOCs: lpstrFile :
        if the buffer is too small, then:
        - the function returns FALSE
        - the CommDlgExtendedError() returns FNERR_BUFFERTOOSMALL
        - the first two bytes of the lpstrFile buffer contain the
          required size, in bytes or characters. */
    if (!b)
      return false;
    {
      tempPath.ReleaseBuf_CalcLen(bufSize);
      FilePath = tempPath;
      FilterIndex = (int)p.nFilterIndex - 1;
      return true;
    }
  }
}

}
