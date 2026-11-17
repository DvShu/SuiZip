// Windows/Control/ComboBox.cpp

#include "StdAfx.h"


#include "ComboBox.h"


namespace NWindows {
namespace NControl {

LRESULT CComboBox::GetLBText(int index, CSysString &s)
{
  s.Empty();
  LRESULT len = GetLBTextLen(index); // length, excluding the terminating null character
  if (len == CB_ERR)
    return len;
  LRESULT len2 = GetLBText(index, s.GetBuf((unsigned)len));
  if (len2 == CB_ERR)
    return len;
  if (len > len2)
    len = len2;
  s.ReleaseBuf_CalcLen((unsigned)len);
  return len;
}


LRESULT CComboBox::AddString_SetItemData(LPCWSTR s, LPARAM lParam)
{
  const LRESULT index = AddString(s);
  // NOTE: SetItemData((int)-1, lParam) works as unexpected.
  if (index >= 0) // optional check, because (index < 0) is not expected for normal inputs
    SetItemData((int)index, lParam);
  return index;
}

}}
