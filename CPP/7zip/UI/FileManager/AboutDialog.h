// AboutDialog.h
// Modified 2026-11-17 for the fork: release the About logo when the dialog closes.
 
#ifndef ZIP7_INC_ABOUT_DIALOG_H
#define ZIP7_INC_ABOUT_DIALOG_H

#include "../../../Windows/Control/Dialog.h"

#include "AboutDialogRes.h"

class CAboutDialog: public NWindows::NControl::CModalDialog
{
public:
  virtual bool OnInit() Z7_override;
  virtual bool OnDestroy() Z7_override;
  virtual void OnHelp() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  INT_PTR Create(HWND wndParent = NULL) { return CModalDialog::Create(IDD_ABOUT, wndParent); }
};

#endif
