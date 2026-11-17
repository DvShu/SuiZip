// Windows/FileSystem.cpp

#include "StdAfx.h"



#include "FileSystem.h"
#include "WinDefs.h"


namespace NWindows {
namespace NFile {
namespace NSystem {

#ifdef _WIN32

bool MyGetVolumeInformation(
    CFSTR rootPath,
    UString &volumeName,
    LPDWORD volumeSerialNumber,
    LPDWORD maximumComponentLength,
    LPDWORD fileSystemFlags,
    UString &fileSystemName)
{
  BOOL res;
  {
    WCHAR v[MAX_PATH + 2]; v[0] = 0;
    WCHAR f[MAX_PATH + 2]; f[0] = 0;
    res = GetVolumeInformationW(fs2us(rootPath),
        v, MAX_PATH,
        volumeSerialNumber, maximumComponentLength, fileSystemFlags,
        f, MAX_PATH);
    volumeName = v;
    fileSystemName = f;
  }
  return BOOLToBool(res);
}

UINT MyGetDriveType(CFSTR pathName)
{
  {
    return GetDriveTypeW(fs2us(pathName));
  }
}



bool MyGetDiskFreeSpace(CFSTR rootPath, UInt64 &clusterSize, UInt64 &totalSize, UInt64 &freeSize)
{
  DWORD numSectorsPerCluster, bytesPerSector, numFreeClusters, numClusters;
  bool sizeIsDetected = false;
  {
    {
      ULARGE_INTEGER freeBytesToCaller2, totalSize2, freeSize2;
      sizeIsDetected = BOOLToBool(
        GetDiskFreeSpaceExW
          (fs2us(rootPath), &freeBytesToCaller2, &totalSize2, &freeSize2));
      totalSize = totalSize2.QuadPart;
      freeSize = freeSize2.QuadPart;
    }
    if (!::GetDiskFreeSpaceW(fs2us(rootPath), &numSectorsPerCluster, &bytesPerSector, &numFreeClusters, &numClusters))
      return false;
  }
  clusterSize = (UInt64)bytesPerSector * (UInt64)numSectorsPerCluster;
  if (!sizeIsDetected)
  {
    totalSize = clusterSize * (UInt64)numClusters;
    freeSize = clusterSize * (UInt64)numFreeClusters;
  }
  return true;
}

#endif

/*
bool Is_File_LimitedBy_4GB(CFSTR _path, bool &isFsDetected)
{
  isFsDetected = false;
  FString path (_path);
  path.DeleteFrom(NName::GetRootPrefixSize(path));
  // GetVolumeInformation supports super paths.
  // NName::If_IsSuperPath_RemoveSuperPrefix(path);
  if (!path.IsEmpty())
  {
    DWORD volumeSerialNumber, maximumComponentLength, fileSystemFlags;
    UString volName, fileSystemName;
    if (MyGetVolumeInformation(path, volName,
        &volumeSerialNumber, &maximumComponentLength, &fileSystemFlags,
        fileSystemName))
    {
      isFsDetected = true;
      if (fileSystemName.IsPrefixedBy_Ascii_NoCase("fat"))
        return true;
    }
  }
  return false;
}
*/

}}}
