#Requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$Debugger,
    [switch]$BuildOnly,
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
$project = Join-Path $repoRoot 'CPP\7zip\Bundles\Fm'
$output = Join-Path $project 'dev_x64'
$exe = Join-Path $output '7zFM.exe'

$running = Get-Process -Name 7zFM -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
if ($running) { throw 'Close the running dev preview before rebuilding.' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Visual Studio Installer was not found.' }
$vsRoot = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Install the Visual Studio Desktop development with C++ workload.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'

$buildStarted = [DateTime]::UtcNow
$stampPath = Join-Path $output 'last-build.stamp'
if (Test-Path -LiteralPath $stampPath -PathType Leaf) {
    $previousBuild = (Get-Item -LiteralPath $stampPath).LastWriteTimeUtc
    $inputRoots = @('C', 'CPP', 'Asm') | ForEach-Object { Join-Path $repoRoot $_ }
    $changedInput = Get-ChildItem -LiteralPath $inputRoots -Recurse -File | Where-Object {
        $_.LastWriteTimeUtc -gt $previousBuild -and ($_.Extension -in @('.h', '.inc', '.mak') -or $_.Name -eq 'makefile')
    } | Select-Object -First 1
    if ($changedInput -or (Get-Item -LiteralPath $PSCommandPath).LastWriteTimeUtc -gt $previousBuild) { $Rebuild = $true }
} else {
    $Rebuild = $true
}

# NMAKE tracks resource.rc itself, while it includes other resources and bitmaps.
$compiledResources = Join-Path $output 'resource.res'
if (Test-Path -LiteralPath $compiledResources -PathType Leaf) { Remove-Item -LiteralPath $compiledResources }
$nmakeOptions = '/nologo /s'
if ($Rebuild) { $nmakeOptions += ' /A' }
$buildCommand = 'call "{0}" >nul && set "_LINK_=/DEBUG:FULL /OPT:NOREF /OPT:NOICF" && nmake {1} PLATFORM=x64 O=dev_x64 CFLAGS_COMMON=/Z7 "CFLAGS_O1=$(CFLAGS) /Od" "CFLAGS_O2=$(CFLAGS) /Od"' -f $vcvars, $nmakeOptions

Push-Location -LiteralPath $project
try {
    & $env:ComSpec /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw "The debug build failed ($LASTEXITCODE)." }
} finally { Pop-Location }

$pdb = Join-Path $output '7zFM.pdb'
foreach ($file in @($exe, $pdb)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "The build did not generate $file." }
}
[IO.File]::WriteAllText($stampPath, '')
[IO.File]::SetLastWriteTimeUtc($stampPath, $buildStarted)

$resourceCache = Join-Path $repoRoot 'dist\.pack-cache'
$localization = Join-Path $output 'Lang\zh-cn.txt'
if ((Test-Path -LiteralPath $resourceCache -PathType Container) -and -not (Test-Path -LiteralPath $localization -PathType Leaf)) {
    $cachedInstaller = Get-ChildItem -LiteralPath $resourceCache -Filter '7zip-*-x64.exe' -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($cachedInstaller) {
        & tar.exe -xf $cachedInstaller.FullName -C $output 'Lang/zh-cn.txt' 'Lang/en.ttt'
        if ($LASTEXITCODE -ne 0) { Write-Warning 'Unable to copy cached language files. The preview can run in English.' }
    }
}

Write-Host "Debug executable: $exe"
if ($BuildOnly) { return }
if ($Debugger) {
    $devenv = Join-Path $vsRoot 'Common7\IDE\devenv.exe'
    if (-not (Test-Path -LiteralPath $devenv -PathType Leaf)) { throw 'Visual Studio IDE is required to open the debugger.' }
    Start-Process -FilePath $devenv -ArgumentList ('/DebugExe "{0}"' -f $exe) -WorkingDirectory $output
} else {
    Start-Process -FilePath $exe -WorkingDirectory $output
}
