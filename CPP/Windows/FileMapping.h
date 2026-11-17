// Windows/FileMapping.h

#ifndef ZIP7_INC_WINDOWS_FILE_MAPPING_H
#define ZIP7_INC_WINDOWS_FILE_MAPPING_H

#include "../Common/MyTypes.h"

#include "Handle.h"

namespace NWindows {

class CFileMapping: public CHandle
{
public:
  WRes Create(DWORD protect, UInt64 maxSize, LPCTSTR name)
  {
    _handle = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, protect, (DWORD)(maxSize >> 32), (DWORD)maxSize, name);
    return ::GetLastError();
  }

  WRes Open(DWORD
      desiredAccess
      , LPCTSTR name)
  {
    _handle = ::OpenFileMapping(desiredAccess, FALSE, name);
    if (_handle != NULL)
      return 0;
    return ::GetLastError();
  }

  LPVOID Map(DWORD desiredAccess, UInt64 fileOffset, SIZE_T numberOfBytesToMap)
  {
    return ::MapViewOfFile(_handle, desiredAccess, (DWORD)(fileOffset >> 32), (DWORD)fileOffset, numberOfBytesToMap);
  }

  LPVOID Map(DWORD desiredAccess, UInt64 fileOffset, SIZE_T numberOfBytesToMap, LPVOID baseAddress)
  {
    return ::MapViewOfFileEx(_handle, desiredAccess, (DWORD)(fileOffset >> 32), (DWORD)fileOffset, numberOfBytesToMap, baseAddress);
  }
};

class CFileUnmapper
{
  const void *_data;
public:
  CFileUnmapper(const void *data Z7_lifetimebound) : _data(data) {}
  ~CFileUnmapper() { ::UnmapViewOfFile(_data); }
};

}

#endif
