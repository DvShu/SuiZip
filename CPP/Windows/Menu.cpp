// Windows/Menu.cpp

#include "StdAfx.h"

#include "Menu.h"


namespace NWindows {

#define COPY_MENUITEM_field(d, s, name) \
  d.name = s.name;

#define COPY_MENUITEM_fields(d, s)  \
  COPY_MENUITEM_field(d, s, fMask)  \
  COPY_MENUITEM_field(d, s, fType)  \
  COPY_MENUITEM_field(d, s, fState)  \
  COPY_MENUITEM_field(d, s, wID)  \
  COPY_MENUITEM_field(d, s, hSubMenu)  \
  COPY_MENUITEM_field(d, s, hbmpChecked)  \
  COPY_MENUITEM_field(d, s, hbmpUnchecked)  \
  COPY_MENUITEM_field(d, s, dwItemData)  \

static void ConvertItemToSysForm(const CMenuItem &item, MENUITEMINFOW &si)
{
  ZeroMemory(&si, sizeof(si));
  si.cbSize = sizeof(si);
  COPY_MENUITEM_fields(si, item)
}


static void ConvertItemToMyForm(const MENUITEMINFOW &si, CMenuItem &item)
{
  COPY_MENUITEM_fields(item, si)
}



bool CMenu::GetItem(UINT itemIndex, bool byPosition, CMenuItem &item) const
{
  item.StringValue.Empty();
  const unsigned kMaxSize = 512;
  {
    wchar_t s[kMaxSize + 1];
    s[0] = 0;
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    const bool isString = item.IsString();
    unsigned bufSize = kMaxSize;
    if (isString)
    {
      si.cch = bufSize;
      si.dwTypeData = s;
    }
    bool res = GetItemInfo(itemIndex, byPosition, &si);
    if (!res)
      return false;
    if (isString)
    {
      s[Z7_ARRAY_SIZE(s) - 1] = 0;
      item.StringValue = s;
      if (si.cch >= bufSize - 1)
      {
        si.dwTypeData = NULL;
        res = GetItemInfo(itemIndex, byPosition, &si);
        if (!res)
          return false;
        si.cch++;
        bufSize = si.cch;
        si.dwTypeData = item.StringValue.GetBuf(bufSize);
        res = GetItemInfo(itemIndex, byPosition, &si);
        item.StringValue.ReleaseBuf_CalcLen(bufSize);
        if (!res)
          return false;
      }
      // if (item.StringValue.Len() != si.cch) throw 123; // for debug
    }
    ConvertItemToMyForm(si, item);
    return true;
  }
}


bool CMenu::SetItem(UINT itemIndex, bool byPosition, const CMenuItem &item)
{
  {
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
      si.dwTypeData = item.StringValue.Ptr_non_const();
    return SetItemInfo(itemIndex, byPosition, &si);
  }
}


bool CMenu::InsertItem(UINT itemIndex, bool byPosition, const CMenuItem &item)
{
  {
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
      si.dwTypeData = item.StringValue.Ptr_non_const();
    return InsertItem(itemIndex, byPosition, &si);
  }
}


}
