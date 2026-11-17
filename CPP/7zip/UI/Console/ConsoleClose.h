// ConsoleClose.h

#ifndef ZIP7_INC_CONSOLE_CLOSE_H
#define ZIP7_INC_CONSOLE_CLOSE_H

namespace NConsoleClose {

// class CCtrlBreakException {};


extern unsigned g_BreakCounter;

inline bool TestBreakSignal()
{
  return (g_BreakCounter != 0);
}

class CCtrlHandlerSetter Z7_final
{
  #ifndef _WIN32
  void (*memo_sig_int)(int);
  void (*memo_sig_term)(int);
  #endif
public:
  CCtrlHandlerSetter();
  ~CCtrlHandlerSetter();
};


}

#endif
