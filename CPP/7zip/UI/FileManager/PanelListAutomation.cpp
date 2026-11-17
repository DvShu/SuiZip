// PanelListAutomation.cpp

#include "StdAfx.h"

#include <new>
#include <UIAutomation.h>

#include "Panel.h"

namespace {

static bool GetRowBounds(HWND list, int row, RECT &bounds)
{
  RECT viewport;
  if (!::GetClientRect(list, &viewport) ||
      !ListView_GetItemRect(list, row, &bounds, LVIR_BOUNDS))
    return false;

  const HWND header = ListView_GetHeader(list);
  if (header && ::IsWindowVisible(header))
  {
    RECT headerBounds;
    if (::GetWindowRect(header, &headerBounds))
    {
      ::MapWindowPoints(NULL, list, (POINT *)&headerBounds, 2);
      viewport.top = MyMax(viewport.top, headerBounds.bottom);
    }
  }
  return ::IntersectRect(&bounds, &bounds, &viewport) != FALSE;
}

static int NextVisibleRow(HWND list, int row)
{
  if ((::GetWindowLongPtr(list, GWL_STYLE) & LVS_TYPEMASK) == LVS_REPORT)
  {
    const int top = ListView_GetTopIndex(list);
    const int limit = MyMin(ListView_GetItemCount(list), top + ListView_GetCountPerPage(list) + 1);
    for (row = MyMax(row + 1, top); row < limit; row++)
    {
      RECT bounds;
      if (GetRowBounds(list, row, bounds))
        return row;
    }
    return -1;
  }

  // The icon and list views can have nonconsecutive items in their viewport.
  for (;;)
  {
    const int next = ListView_GetNextItem(list, row, LVNI_VISIBLEONLY);
    if (next <= row)
      return -1;
    row = next;
    RECT bounds;
    if (GetRowBounds(list, row, bounds))
      return row;
  }
}

static int PreviousVisibleRow(HWND list, int before)
{
  int previous = -1;
  for (int row = NextVisibleRow(list, -1); row >= 0 && row < before;
      row = NextVisibleRow(list, row))
    previous = row;
  return previous;
}

static void SetBounds(HWND list, RECT &bounds, UiaRect *result)
{
  ::MapWindowPoints(list, NULL, (POINT *)&bounds, 2);
  result->left = bounds.left;
  result->top = bounds.top;
  result->width = bounds.right - bounds.left;
  result->height = bounds.bottom - bounds.top;
}

static void SetBool(VARIANT *value, bool enabled)
{
  value->vt = VT_BOOL;
  value->boolVal = enabled ? VARIANT_TRUE : VARIANT_FALSE;
}

static HRESULT GetCellName(HWND list, int row, int column, VARIANT *value)
{
  UString name;
  unsigned capacity = 512;
  for (;;)
  {
    LVITEMW item = {};
    item.iSubItem = column;
    item.pszText = name.GetBuf(capacity);
    item.cchTextMax = (int)capacity;
    const unsigned length = (unsigned)::SendMessageW(list, LVM_GETITEMTEXTW, (WPARAM)row, (LPARAM)&item);
    name.ReleaseBuf_SetLen(length);
    if (length + 1 < capacity || capacity >= (1 << 16))
      break;
    capacity *= 2;
  }
  value->bstrVal = ::SysAllocStringLen(name, name.Len());
  if (!value->bstrVal)
    return E_OUTOFMEMORY;
  value->vt = VT_BSTR;
  return S_OK;
}

// UI Automation calls these providers on the list window's COM apartment.
class CListAutomationNode:
    public IRawElementProviderSimple,
    public IRawElementProviderFragment
{
  LONG _references = 1;

protected:
  virtual ~CListAutomationNode() {}
  virtual void *GetExtraInterface(REFIID iid) = 0;

public:
  STDMETHODIMP QueryInterface(REFIID iid, void **result) override
  {
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IRawElementProviderSimple))
      *result = static_cast<IRawElementProviderSimple *>(this);
    else if (iid == __uuidof(IRawElementProviderFragment))
      *result = static_cast<IRawElementProviderFragment *>(this);
    else
      *result = GetExtraInterface(iid);
    if (!*result)
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)::InterlockedIncrement(&_references); }
  STDMETHODIMP_(ULONG) Release() override
  {
    const ULONG remaining = (ULONG)::InterlockedDecrement(&_references);
    if (!remaining)
      delete this;
    return remaining;
  }

  STDMETHODIMP get_ProviderOptions(ProviderOptions *options) override
  {
    *options = (ProviderOptions)(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
  }

  STDMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY **roots) override
  {
    *roots = NULL;
    return S_OK;
  }
};

}

class CFileListAutomation Z7_final:
    public CListAutomationNode,
    public IRawElementProviderFragmentRoot,
    public IItemContainerProvider,
    public ISelectionProvider
{
  HWND _list;
  LONG _generation = 0;
  int _visibleFirst = -1;
  int _visibleLast = -1;
  int _visibleCount = 0;

  void *GetExtraInterface(REFIID iid) override
  {
    if (iid == __uuidof(IRawElementProviderFragmentRoot))
      return static_cast<IRawElementProviderFragmentRoot *>(this);
    if (iid == __uuidof(IItemContainerProvider))
      return static_cast<IItemContainerProvider *>(this);
    if (iid == __uuidof(ISelectionProvider))
      return static_cast<ISelectionProvider *>(this);
    return NULL;
  }

public:
  explicit CFileListAutomation(HWND list): _list(list) {}
  STDMETHODIMP QueryInterface(REFIID iid, void **result) override
    { return CListAutomationNode::QueryInterface(iid, result); }
  STDMETHODIMP_(ULONG) AddRef() override { return CListAutomationNode::AddRef(); }
  STDMETHODIMP_(ULONG) Release() override { return CListAutomationNode::Release(); }
  HWND List() const { return _list; }
  LONG Generation() const { return _generation; }
  void Detach() { _list = NULL; }
  void ResetRows() { _generation++; }

  HRESULT NewItem(int index, int column, REFIID iid, void **result);
  void ViewportChanged(bool force);
  void ItemChanged(const NMLISTVIEW &item);

  STDMETHODIMP GetPatternProvider(PATTERNID pattern, IUnknown **result) override
  {
    *result = NULL;
    if (pattern == UIA_ItemContainerPatternId)
      return QueryInterface(__uuidof(IItemContainerProvider), (void **)result);
    if (pattern == UIA_SelectionPatternId)
      return QueryInterface(__uuidof(ISelectionProvider), (void **)result);
    return S_OK;
  }

  STDMETHODIMP GetPropertyValue(PROPERTYID prop, VARIANT *result) override
  {
    ::VariantInit(result);
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    if (prop == UIA_ControlTypePropertyId)
    {
      result->vt = VT_I4;
      result->lVal = UIA_ListControlTypeId;
    }
    else if (prop == UIA_IsControlElementPropertyId || prop == UIA_IsContentElementPropertyId)
      SetBool(result, true);
    else if (prop == UIA_HasKeyboardFocusPropertyId)
      SetBool(result, ::GetFocus() == _list && ListView_GetNextItem(_list, -1, LVNI_FOCUSED) < 0);
    return S_OK;
  }

  STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple **result) override
  {
    *result = NULL;
    return _list ? ::UiaHostProviderFromHwnd(_list, result) : UIA_E_ELEMENTNOTAVAILABLE;
  }

  STDMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment **result) override
  {
    *result = NULL;
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    int row = -1;
    if (direction == NavigateDirection_FirstChild)
      row = NextVisibleRow(_list, -1);
    else if (direction == NavigateDirection_LastChild)
      row = PreviousVisibleRow(_list, ListView_GetItemCount(_list));
    return NewItem(row, -1, __uuidof(IRawElementProviderFragment), (void **)result);
  }

  STDMETHODIMP GetRuntimeId(SAFEARRAY **result) override
  {
    // UI Automation supplies the runtime ID of the host HWND.
    *result = NULL;
    return S_OK;
  }

  STDMETHODIMP get_BoundingRectangle(UiaRect *result) override
  {
    *result = {};
    RECT bounds;
    if (!_list || !::GetClientRect(_list, &bounds))
      return UIA_E_ELEMENTNOTAVAILABLE;
    SetBounds(_list, bounds, result);
    return S_OK;
  }

  STDMETHODIMP SetFocus() override
  {
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    ::SetFocus(_list);
    return S_OK;
  }

  STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot **result) override
  {
    return QueryInterface(__uuidof(IRawElementProviderFragmentRoot), (void **)result);
  }

  STDMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment **result) override
  {
    *result = NULL;
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    LVHITTESTINFO hit = {};
    hit.pt.x = (LONG)x;
    hit.pt.y = (LONG)y;
    ::ScreenToClient(_list, &hit.pt);
    const int row = ListView_HitTest(_list, &hit);
    if (row >= 0)
      return NewItem(row, -1, __uuidof(IRawElementProviderFragment), (void **)result);
    return QueryInterface(__uuidof(IRawElementProviderFragment), (void **)result);
  }

  STDMETHODIMP GetFocus(IRawElementProviderFragment **result) override
  {
    *result = NULL;
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    const HWND focus = ::GetFocus();
    if (focus != _list)
      return S_OK;
    const int row = ListView_GetNextItem(_list, -1, LVNI_FOCUSED);
    if (row < 0)
      return QueryInterface(__uuidof(IRawElementProviderFragment), (void **)result);
    return NewItem(row, -1, __uuidof(IRawElementProviderFragment), (void **)result);
  }

  STDMETHODIMP FindItemByProperty(IRawElementProviderSimple *startAfter, PROPERTYID prop,
      VARIANT value, IRawElementProviderSimple **result) override;
  STDMETHODIMP GetSelection(SAFEARRAY **result) override;
  STDMETHODIMP get_CanSelectMultiple(BOOL *result) override
  {
    if (!_list)
      return UIA_E_ELEMENTNOTAVAILABLE;
    *result = (::GetWindowLongPtr(_list, GWL_STYLE) & LVS_SINGLESEL) == 0;
    return S_OK;
  }
  STDMETHODIMP get_IsSelectionRequired(BOOL *result) override
  {
    *result = FALSE;
    return S_OK;
  }
};

namespace {

class CFileListItem Z7_final:
    public CListAutomationNode,
    public IVirtualizedItemProvider,
    public IScrollItemProvider,
    public ISelectionItemProvider,
    public IInvokeProvider
{
  CMyComPtr<CFileListAutomation> _root;
  const LONG _generation;
  const int _id;
  const int _column;

  HWND List() const { return _generation == _root->Generation() ? _root->List() : NULL; }
  int Row(HWND list) const { return list ? (int)ListView_MapIDToIndex(list, _id) : -1; }

  int ColumnCount(HWND list) const
  {
    if ((::GetWindowLongPtr(list, GWL_STYLE) & LVS_TYPEMASK) != LVS_REPORT)
      return 0;
    const HWND header = ListView_GetHeader(list);
    return header ? Header_GetItemCount(header) : 0;
  }

  void *GetExtraInterface(REFIID iid) override
  {
    if (_column >= 0)
      return NULL;
    if (iid == __uuidof(IVirtualizedItemProvider))
      return static_cast<IVirtualizedItemProvider *>(this);
    if (iid == __uuidof(IScrollItemProvider))
      return static_cast<IScrollItemProvider *>(this);
    if (iid == __uuidof(ISelectionItemProvider))
      return static_cast<ISelectionItemProvider *>(this);
    if (iid == __uuidof(IInvokeProvider))
      return static_cast<IInvokeProvider *>(this);
    return NULL;
  }

public:
  CFileListItem(CFileListAutomation *root, int id, int column):
      _root(root), _generation(root->Generation()), _id(id), _column(column) {}
  STDMETHODIMP QueryInterface(REFIID iid, void **result) override
    { return CListAutomationNode::QueryInterface(iid, result); }
  STDMETHODIMP_(ULONG) AddRef() override { return CListAutomationNode::AddRef(); }
  STDMETHODIMP_(ULONG) Release() override { return CListAutomationNode::Release(); }

  STDMETHODIMP GetPatternProvider(PATTERNID pattern, IUnknown **result) override
  {
    *result = NULL;
    if (_column >= 0)
      return S_OK;
    if (pattern == UIA_VirtualizedItemPatternId)
      return QueryInterface(__uuidof(IVirtualizedItemProvider), (void **)result);
    if (pattern == UIA_ScrollItemPatternId)
      return QueryInterface(__uuidof(IScrollItemProvider), (void **)result);
    if (pattern == UIA_SelectionItemPatternId)
      return QueryInterface(__uuidof(ISelectionItemProvider), (void **)result);
    if (pattern == UIA_InvokePatternId)
      return QueryInterface(__uuidof(IInvokeProvider), (void **)result);
    return S_OK;
  }

  STDMETHODIMP GetPropertyValue(PROPERTYID prop, VARIANT *result) override
  {
    ::VariantInit(result);
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    switch (prop)
    {
      case UIA_ControlTypePropertyId:
        result->vt = VT_I4;
        result->lVal = _column < 0 ? UIA_ListItemControlTypeId : UIA_TextControlTypeId;
        break;
      case UIA_NamePropertyId:
        try { return GetCellName(list, row, MyMax(0, _column), result); }
        catch (...) { return E_OUTOFMEMORY; }
      case UIA_IsControlElementPropertyId:
      case UIA_IsContentElementPropertyId:
        SetBool(result, true);
        break;
      case UIA_IsEnabledPropertyId:
        SetBool(result, ::IsWindowEnabled(list) != FALSE);
        break;
      case UIA_IsKeyboardFocusablePropertyId:
        SetBool(result, _column < 0);
        break;
      case UIA_HasKeyboardFocusPropertyId:
        SetBool(result, _column < 0 && ::GetFocus() == list &&
            ListView_GetItemState(list, row, LVIS_FOCUSED) != 0);
        break;
      case UIA_IsOffscreenPropertyId:
      {
        UiaRect bounds;
        get_BoundingRectangle(&bounds);
        SetBool(result, bounds.width <= 0 || bounds.height <= 0);
        break;
      }
    }
    return S_OK;
  }

  STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple **result) override
  {
    *result = NULL;
    return S_OK;
  }

  STDMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment **result) override
  {
    *result = NULL;
    const HWND list = List();
    int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    int column = _column;
    if (column < 0)
    {
      if (direction == NavigateDirection_Parent)
        return _root->QueryInterface(__uuidof(IRawElementProviderFragment), (void **)result);
      if (direction == NavigateDirection_NextSibling)
        row = NextVisibleRow(list, row);
      else if (direction == NavigateDirection_PreviousSibling)
        row = PreviousVisibleRow(list, row);
      else
      {
        RECT bounds;
        const int count = ColumnCount(list);
        if (count <= 0 || !GetRowBounds(list, row, bounds))
          return S_OK;
        column = direction == NavigateDirection_FirstChild ? 0 : count - 1;
      }
    }
    else
    {
      if (direction == NavigateDirection_Parent)
        column = -1;
      else if (direction == NavigateDirection_NextSibling && column + 1 < ColumnCount(list))
        column++;
      else if (direction == NavigateDirection_PreviousSibling && column > 0)
        column--;
      else
        return S_OK;
    }
    return _root->NewItem(row, column, __uuidof(IRawElementProviderFragment), (void **)result);
  }

  STDMETHODIMP GetRuntimeId(SAFEARRAY **result) override
  {
    *result = NULL;
    if (Row(List()) < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    const LONG parts[] = { UiaAppendRuntimeId, _generation, _id, _column };
    SAFEARRAY *ids = ::SafeArrayCreateVector(VT_I4, 0, Z7_ARRAY_SIZE(parts));
    if (!ids)
      return E_OUTOFMEMORY;
    for (LONG i = 0; i < (LONG)Z7_ARRAY_SIZE(parts); i++)
    {
      LONG part = parts[i];
      ::SafeArrayPutElement(ids, &i, &part);
    }
    *result = ids;
    return S_OK;
  }

  STDMETHODIMP get_BoundingRectangle(UiaRect *result) override
  {
    *result = {};
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    RECT bounds;
    if (!GetRowBounds(list, row, bounds))
      return S_OK;
    if (_column >= 0)
    {
      RECT cell;
      if (!ListView_GetSubItemRect(list, row, _column, LVIR_LABEL, &cell) ||
          !::IntersectRect(&bounds, &bounds, &cell))
        return S_OK;
    }
    SetBounds(list, bounds, result);
    return S_OK;
  }

  STDMETHODIMP SetFocus() override
  {
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    if (!::IsWindowEnabled(list))
      return UIA_E_ELEMENTNOTENABLED;
    ListView_EnsureVisible(list, row, FALSE);
    ListView_SetItemState(list, row, LVIS_FOCUSED, LVIS_FOCUSED);
    ::SetFocus(list);
    return S_OK;
  }

  STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot **result) override
  {
    return _root->get_FragmentRoot(result);
  }

  STDMETHODIMP Realize() override { return ScrollIntoView(); }
  STDMETHODIMP ScrollIntoView() override
  {
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    return ListView_EnsureVisible(list, row, FALSE) ? S_OK : E_FAIL;
  }

  STDMETHODIMP Invoke() override
  {
    const HWND list = List();
    if (Row(list) < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    if (!::IsWindowEnabled(list))
      return UIA_E_ELEMENTNOTENABLED;
    if (!::PostMessageW(list, kFileListAutomationInvoke, (WPARAM)_generation, _id))
      return HRESULT_FROM_WIN32(::GetLastError());
    return S_OK;
  }

  STDMETHODIMP Select() override { return SetSelection(true, true); }
  STDMETHODIMP AddToSelection() override { return SetSelection(true, false); }
  STDMETHODIMP RemoveFromSelection() override { return SetSelection(false, false); }

  HRESULT SetSelection(bool select, bool clearOther)
  {
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    if (!::IsWindowEnabled(list))
      return UIA_E_ELEMENTNOTENABLED;
    if (select && !clearOther && (::GetWindowLongPtr(list, GWL_STYLE) & LVS_SINGLESEL) &&
        ListView_GetSelectedCount(list) != 0 && !ListView_GetItemState(list, row, LVIS_SELECTED))
      return UIA_E_INVALIDOPERATION;
    if (clearOther)
      ListView_SetItemState(list, -1, 0, LVIS_SELECTED);
    ListView_SetItemState(list, row, select ? LVIS_SELECTED : 0, LVIS_SELECTED);
    return S_OK;
  }

  STDMETHODIMP get_IsSelected(BOOL *result) override
  {
    *result = FALSE;
    const HWND list = List();
    const int row = Row(list);
    if (row < 0)
      return UIA_E_ELEMENTNOTAVAILABLE;
    *result = ListView_GetItemState(list, row, LVIS_SELECTED) != 0;
    return S_OK;
  }

  STDMETHODIMP get_SelectionContainer(IRawElementProviderSimple **result) override
  {
    return _root->QueryInterface(__uuidof(IRawElementProviderSimple), (void **)result);
  }
};

}

HRESULT CFileListAutomation::NewItem(int row, int column, REFIID iid, void **result)
{
  *result = NULL;
  if (row < 0 || !_list)
    return S_OK;
  const int id = (int)ListView_MapIndexToID(_list, row);
  if (id == -1)
    return S_OK;
  CFileListItem *item = new (std::nothrow) CFileListItem(this, id, column);
  if (!item)
    return E_OUTOFMEMORY;
  const HRESULT res = item->QueryInterface(iid, result);
  item->Release();
  return res;
}

STDMETHODIMP CFileListAutomation::FindItemByProperty(IRawElementProviderSimple *startAfter,
    PROPERTYID prop, VARIANT value, IRawElementProviderSimple **result)
{
  *result = NULL;
  if (!_list)
    return UIA_E_ELEMENTNOTAVAILABLE;
  int start = -1;
  if (startAfter)
  {
    CMyComPtr<IRawElementProviderFragment> fragment;
    CMyComPtr<IRawElementProviderFragmentRoot> root;
    if (FAILED(startAfter->QueryInterface(__uuidof(IRawElementProviderFragment), (void **)&fragment)) ||
        FAILED(fragment->get_FragmentRoot(&root)) ||
        root.Interface() != static_cast<IRawElementProviderFragmentRoot *>(this))
      return E_INVALIDARG;
    SAFEARRAY *runtimeId = NULL;
    if (FAILED(fragment->GetRuntimeId(&runtimeId)) || !runtimeId)
      return E_INVALIDARG;
    LONG id = -1;
    LONG position = 2;
    const HRESULT res = ::SafeArrayGetElement(runtimeId, &position, &id);
    ::SafeArrayDestroy(runtimeId);
    if (FAILED(res))
      return E_INVALIDARG;
    start = (int)ListView_MapIDToIndex(_list, id);
    if (start < 0)
      return E_INVALIDARG;
  }

  int row = -1;
  switch (prop)
  {
    case 0:
      if (start + 1 < ListView_GetItemCount(_list))
        row = start + 1;
      break;
    case UIA_NamePropertyId:
    {
      if (value.vt != VT_BSTR || !value.bstrVal)
        return E_INVALIDARG;
      LVFINDINFOW find = {};
      find.flags = LVFI_STRING;
      find.psz = value.bstrVal;
      row = (int)::SendMessageW(_list, LVM_FINDITEMW, (WPARAM)start, (LPARAM)&find);
      break;
    }
    case UIA_SelectionItemIsSelectedPropertyId:
      if (value.vt != VT_BOOL)
        return E_INVALIDARG;
      for (int i = start + 1, count = ListView_GetItemCount(_list); i < count; i++)
        if ((ListView_GetItemState(_list, i, LVIS_SELECTED) != 0) == (value.boolVal != VARIANT_FALSE))
        {
          row = i;
          break;
        }
      break;
    default:
      return E_INVALIDARG;
  }
  return NewItem(row, -1, __uuidof(IRawElementProviderSimple), (void **)result);
}

STDMETHODIMP CFileListAutomation::GetSelection(SAFEARRAY **result)
{
  *result = NULL;
  if (!_list)
    return UIA_E_ELEMENTNOTAVAILABLE;
  // Like tree traversal, selection reports realized rows. ItemContainer can find
  // selected rows outside the viewport.
  CRecordVector<int> rows;
  try
  {
    for (int row = NextVisibleRow(_list, -1); row >= 0; row = NextVisibleRow(_list, row))
      if (ListView_GetItemState(_list, row, LVIS_SELECTED))
        rows.Add(row);
  }
  catch (...) { return E_OUTOFMEMORY; }
  SAFEARRAY *selection = ::SafeArrayCreateVector(VT_UNKNOWN, 0, rows.Size());
  if (!selection)
    return E_OUTOFMEMORY;
  for (LONG i = 0; i < (LONG)rows.Size(); i++)
  {
    CMyComPtr<IRawElementProviderSimple> item;
    const HRESULT res = NewItem(rows[(unsigned)i], -1, __uuidof(IRawElementProviderSimple), (void **)&item);
    if (FAILED(res) || FAILED(::SafeArrayPutElement(selection, &i, item.Interface())))
    {
      ::SafeArrayDestroy(selection);
      return FAILED(res) ? res : E_OUTOFMEMORY;
    }
  }
  *result = selection;
  return S_OK;
}

void CFileListAutomation::ViewportChanged(bool force)
{
  if (!_list || !::UiaClientsAreListening())
    return;
  const int count = ListView_GetItemCount(_list);
  const int first = NextVisibleRow(_list, -1);
  const int last = PreviousVisibleRow(_list, count);
  if (force || count != _visibleCount || first != _visibleFirst || last != _visibleLast)
  {
    _visibleCount = count;
    _visibleFirst = first;
    _visibleLast = last;
    ::UiaRaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
  }
}

void CFileListAutomation::ItemChanged(const NMLISTVIEW &change)
{
  if (!(change.uChanged & LVIF_STATE) || !::UiaClientsAreListening())
    return;
  const UINT delta = change.uNewState ^ change.uOldState;
  if (change.iItem < 0)
  {
    if (delta & LVIS_SELECTED)
      ::UiaRaiseAutomationEvent(this, UIA_Selection_InvalidatedEventId);
    return;
  }
  const bool focused = (delta & change.uNewState & LVIS_FOCUSED) != 0;
  RECT bounds;
  const bool selectionChanged = (delta & LVIS_SELECTED) != 0 &&
      GetRowBounds(_list, change.iItem, bounds);
  if (!focused && !selectionChanged)
    return;
  CMyComPtr<IRawElementProviderSimple> item;
  if (FAILED(NewItem(change.iItem, -1, __uuidof(IRawElementProviderSimple), (void **)&item)) || !item)
    return;
  if (focused)
    ::UiaRaiseAutomationEvent(item, UIA_AutomationFocusChangedEventId);
  if (selectionChanged)
  {
    EVENTID event = UIA_SelectionItem_ElementRemovedFromSelectionEventId;
    if (change.uNewState & LVIS_SELECTED)
      event = ListView_GetSelectedCount(_list) == 1 ? UIA_SelectionItem_ElementSelectedEventId
          : UIA_SelectionItem_ElementAddedToSelectionEventId;
    ::UiaRaiseAutomationEvent(item, event);
  }
}

bool CMyListView::GetAutomationObject(WPARAM wParam, LPARAM lParam, LRESULT &result)
{
  if ((LONG)lParam != UiaRootObjectId)
    return false;
  if (!_automation)
    _automation = new (std::nothrow) CFileListAutomation(*this);
  if (!_automation)
    return false;
  result = ::UiaReturnRawElementProvider(*this, wParam, lParam, _automation);
  return true;
}

void CMyListView::CloseAutomation()
{
  if (_automation)
  {
    _automation->Detach();
    ::UiaReturnRawElementProvider(*this, 0, 0, NULL);
    _automation->Release();
    _automation = NULL;
  }
}

void CMyListView::InvokeAutomationItem(WPARAM generation, LPARAM itemId)
{
  if (!_automation || generation != (WPARAM)_automation->Generation() || !::IsWindowEnabled(*this))
    return;
  const int row = (int)ListView_MapIDToIndex(*this, (int)itemId);
  LPARAM index;
  if (row < 0 || !GetItemParam((unsigned)row, index))
    return;
  CMyComPtr<IRawElementProviderSimple> item;
  if (::UiaClientsAreListening() &&
      SUCCEEDED(_automation->NewItem(row, -1, __uuidof(IRawElementProviderSimple), (void **)&item)) && item)
    ::UiaRaiseAutomationEvent(item, UIA_Invoke_InvokedEventId);
  const unsigned realIndex = (unsigned)index;
  if (_panel->IsItem_Folder(realIndex))
    _panel->OpenFolder(realIndex);
  else
    _panel->OpenItem(realIndex, true, true);
}

void CMyListView::NotifyAutomationViewport(bool force, bool reset)
{
  if (_automation)
  {
    if (reset)
      _automation->ResetRows();
    _automation->ViewportChanged(force);
  }
}

void CMyListView::NotifyAutomationItem(const NMLISTVIEW &item)
{
  if (_automation)
    _automation->ItemChanged(item);
}
