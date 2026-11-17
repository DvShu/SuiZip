!IF "$(PLATFORM)" == "x86" || "$(PLATFORM)" == "arm" || "$(PLATFORM)" == "Win32"
!ERROR SuiZip requires a 64-bit target.
!ENDIF

LIBS = $(LIBS) oleaut32.lib ole32.lib
RFLAGS = $(RFLAGS) -dWINVER=0x0A00 -d_WIN32_WINNT=0x0A00

!IF "$(CC)" != "clang-cl"
# for link time code generation:
# CFLAGS = $(CFLAGS) -GL
!ENDIF

!IFNDEF O
!IFDEF PLATFORM
O=$(PLATFORM)
!ELSE
O=o
!ENDIF
!ENDIF

!IF "$(CC)" != "clang-cl"
# CFLAGS = $(CFLAGS) -FAsc -Fa$O/asm/
!ENDIF

# LFLAGS = $(LFLAGS) /guard:cf


!IF "$(PLATFORM)" == "arm64"
MY_ML = armasm64
!ELSE
MY_ML = ml64 -WX
!ENDIF

# MY_ML = "$(MY_ML) -Fl$O\asm\


!IFDEF UNDER_CE
!ERROR SuiZip requires Windows 10 or newer.
!ENDIF
!IF "$(PLATFORM)" != "arm64"
CFLAGS = $(CFLAGS) -Gr
!ENDIF
LIBS = $(LIBS) user32.lib advapi32.lib shell32.lib

!IF "$(PLATFORM)" == "arm64"
COMPL_ASM = $(MY_ML) $** $O/$(*B).obj
!ELSE
COMPL_ASM = $(MY_ML) -c -Fo$O/ $**
!ENDIF

CFLAGS_c_switch = -c -Fo$O/

!IFDEF OLD_COMPILER
CFLAGS_WARN_LEVEL = -W4
!ELSE
!IF "$(CC)" != "clang-cl"
CFLAGS_WARN_LEVEL = -Wall -analyze
!ELSE
CFLAGS_WARN_LEVEL = -Wall --analyze -Xclang -analyzer-output=text
# CFLAGS_c_switch =
!ENDIF
CFLAGS_WARN_LEVEL = -Wall
!ENDIF

CFLAGS = $(CFLAGS) -nologo $(CFLAGS_c_switch) $(CFLAGS_WARN_LEVEL) -WX -EHsc -Gy -GR- -GF

!IF "$(CC)" == "clang-cl"
CFLAGS = $(CFLAGS) -Werror -Wall -Wextra -Weverything -Wfatal-errors
!ENDIF

# !IFDEF MY_DYNAMIC_LINK
!IF "$(MY_DYNAMIC_LINK)" != ""
CFLAGS = $(CFLAGS) -MD
!ELSE
!IFNDEF MY_SINGLE_THREAD
CFLAGS = $(CFLAGS) -MT
!ENDIF
!ENDIF


CFLAGS = $(CFLAGS_COMMON) $(CFLAGS)


!IFNDEF OLD_COMPILER

CFLAGS = $(CFLAGS) -GS- -Zc:wchar_t
!IFDEF VCTOOLSVERSION
!IF "$(VCTOOLSVERSION)" >= "14.00"
!IF "$(CC)" != "clang-cl"
CFLAGS = $(CFLAGS) -Zc:throwingNew
!ENDIF
!ENDIF
!ELSE
# -Zc:forScope is default in VS2010. so we need it only for older versions
CFLAGS = $(CFLAGS) -Zc:forScope
!ENDIF

!IFNDEF UNDER_CE
!IF "$(CC)" != "clang-cl"
MP_NPROC = 16
!IFDEF NUMBER_OF_PROCESSORS
!IF $(NUMBER_OF_PROCESSORS) < $(MP_NPROC)
MP_NPROC = $(NUMBER_OF_PROCESSORS)
!ENDIF
!ENDIF
CFLAGS = $(CFLAGS) -MP$(MP_NPROC)
!ENDIF
!ENDIF

!ENDIF


!IFDEF MY_CONSOLE
CFLAGS = $(CFLAGS) -D_CONSOLE
!ENDIF

!IF "$(PLATFORM)" == "x64"
CFLAGS_O1 = $(CFLAGS) -O1
!ELSE
CFLAGS_O1 = $(CFLAGS) -O1
!ENDIF
CFLAGS_O2 = $(CFLAGS) -O2

LFLAGS = $(LFLAGS) -nologo -OPT:REF -OPT:ICF -INCREMENTAL:NO

!IFNDEF UNDER_CE
LFLAGS = $(LFLAGS) /LARGEADDRESSAWARE
!ENDIF

!IFDEF DEF_FILE
LFLAGS = $(LFLAGS) -DLL -DEF:$(DEF_FILE)
!ELSE
!IF defined(MY_FIXED) && "$(PLATFORM)" != "arm64"
LFLAGS = $(LFLAGS) /FIXED
!ELSE
LFLAGS = $(LFLAGS) /FIXED:NO
!ENDIF
# /BASE:0x400000
!ENDIF

!IF "$(PLATFORM)" == "arm64"
# we can get better compression ratio with ARM64 filter if we change alignment to 4096
# LFLAGS = $(LFLAGS) /FILEALIGN:4096
!ENDIF

!IFNDEF DEF_FILE
LFLAGS = $(LFLAGS) /STACK:8388608
!ENDIF

!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /SUBSYSTEM:CONSOLE,10.00
!ELSE
LFLAGS = $(LFLAGS) /SUBSYSTEM:WINDOWS,10.00
!ENDIF


!IF "$(PLATFORM)" == "arm64"
CLANG_FLAGS_TARGET = --target=arm64-pc-windows-msvc
!ENDIF

COMPL_CLANG_SPEC=clang-cl $(CLANG_FLAGS_TARGET)
COMPL_ASM_CLANG = $(COMPL_CLANG_SPEC) -nologo -c -Fo$O/ $(CFLAGS_WARN_LEVEL) -WX $**
# COMPL_C_CLANG   = $(COMPL_CLANG_SPEC) $(CFLAGS_O2)


PROGPATH = $O\$(PROG)

COMPL_O1   = $(CC) $(CFLAGS_O1) $**
COMPL_O2   = $(CC) $(CFLAGS_O2) $**
COMPL_PCH  = $(CC) $(CFLAGS_O1) -Yc"StdAfx.h" -Fp$O/a.pch $**
COMPL      = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $**
COMPLB     = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $<
COMPLB_O2  = $(CC) $(CFLAGS_O2) $<
# COMPLB_O2  = $(CC) $(CFLAGS_O2) -Yu"StdAfx.h" -Fp$O/a.pch $<

CFLAGS_C_ALL = $(CFLAGS_O2) $(CFLAGS_C_SPEC)

CCOMPL_PCH  = $(CC) $(CFLAGS_C_ALL) -Yc"Precomp.h" -Fp$O/a.pch $**
CCOMPL_USE  = $(CC) $(CFLAGS_C_ALL) -Yu"Precomp.h" -Fp$O/a.pch $**
CCOMPLB_USE = $(CC) $(CFLAGS_C_ALL) -Yu"Precomp.h" -Fp$O/a.pch $<
CCOMPL      = $(CC) $(CFLAGS_C_ALL) $**
CCOMPLB     = $(CC) $(CFLAGS_C_ALL) $<

!IF "$(CC)" == "clang-cl"
COMPL  = $(COMPL) -FI StdAfx.h
COMPLB = $(COMPLB) -FI StdAfx.h
CCOMPL_USE  = $(CCOMPL_USE) -FI Precomp.h
CCOMPLB_USE = $(CCOMPLB_USE) -FI Precomp.h
!ENDIF

all: $(PROGPATH)

clean:
	-del /Q $(PROGPATH) $O\*.exe $O\*.dll $O\*.obj $O\*.lib $O\*.exp $O\*.res $O\*.pch $O\*.asm

$O:
	if not exist "$O" mkdir "$O"
$O/asm:
	if not exist "$O/asm" mkdir "$O/asm"

!IF "$(CC)" != "clang-cl"
# for link time code generation:
# LFLAGS = $(LFLAGS) -LTCG
!ENDIF

$(PROGPATH): $O $O/asm $(OBJS) $(DEF_FILE)
	link $(LFLAGS) -out:$(PROGPATH) $(OBJS) $(LIBS)

!IFNDEF NO_DEFAULT_RES
$O\resource.res: $(*B).rc
	rc $(RFLAGS) -fo$@ $**
!ENDIF
$O\StdAfx.obj: $(*B).cpp
	$(COMPL_PCH)

predef: empty.c
	$(CCOMPL)   /EP /Zc:preprocessor /PD
predef2: A.cpp
	$(COMPL)   -EP -Zc:preprocessor -PD
predef3: A.cpp
	$(COMPL)   -E -dM
predef4: A.cpp
	$(COMPL_O2)   -E
