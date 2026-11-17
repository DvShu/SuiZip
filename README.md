# SuiZip

<!-- Modified 2026-11-16 for the fork: documented SuiZip, its upstream source, and the applicable licenses. -->

<img src="assets/suizip-logo.png" alt="SuiZip logo" width="112" height="112">

A Windows file archiver based on 7-Zip, featuring a modern interface design.

SuiZip is an independently maintained fork of 7-Zip 26.06, which Igor Pavlov developed. This fork focuses on updating the Windows interface and branding. [Upstream source](https://github.com/ip7z/7zip/tree/26.06) · [7-Zip website](https://www.7-zip.org/)

## Build and run on Windows

Install Visual Studio with the **Desktop development with C++** workload and the Windows SDK. From the repository root, run `.\dev.cmd` to build and open the file manager. Run `.\dev.cmd -Debugger` to open the debug build in Visual Studio.

### Build the x64 installer

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-x64.ps1
```

The installer is written to `dist/SuiZip-<version>-x64-setup.exe`. Packaging uses Simplified Chinese translation and help resources from the upstream release.

The SuiZip product version is set in [C/SuiZipVersion.h](C/SuiZipVersion.h). The 7-Zip source version used for binary compatibility is defined in [C/7zVersion.h](C/7zVersion.h).

See the [release workflow](AGENTS.md#发布版本) for version bumps, Git tags, and adding the local installer to a GitHub Release.

## License and attribution

7-Zip is copyright © 1999–2026 Igor Pavlov. Most of the source is licensed under the GNU LGPL version 2.1 or later. Some files are under the BSD 2-Clause or BSD 3-Clause licenses or are identified as public domain. The RAR decompression code is subject to the LGPL and the unRAR restriction against using it to recreate the RAR compression algorithm.

- [License and component notices](DOC/License.txt)
- [GNU LGPL 2.1 text](DOC/copying.txt)
- [unRAR terms](DOC/unRarLicense.txt)

When redistributing a modified installer, include the applicable copyright and license notices. Provide the complete corresponding source for that build, including the build and packaging scripts. For online downloads, make the source available from the same download location. Record changes and their dates in modified LGPL-covered files.
