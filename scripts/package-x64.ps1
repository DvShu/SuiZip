#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidatePattern('^[\p{L}\p{Nd}][\p{L}\p{Nd}_-]*$')]
    [string]$Name,

    # The upstream version that supplies the language and help files.
    [ValidatePattern('^[0-9]+\.[0-9]+(?:\.[0-9]+)*$')]
    [string]$ResourceVersion = '26.06',

    [string]$ResourceInstaller
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable exited with code $LASTEXITCODE."
    }
}

function Publish-File {
    param([string]$Source, [string]$Destination)

    if ([IO.File]::Exists($Destination)) {
        [IO.File]::Replace($Source, $Destination, [NullString]::Value)
    } else {
        [IO.File]::Move($Source, $Destination)
    }
}

function Remove-BuildDirectory {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path -PathType Container) {
        $directory = Get-Item -LiteralPath $Path -Force
        if ($directory.FullName -ne $Path -or
            -not $directory.FullName.StartsWith($repoRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
            ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Unexpected build directory: $Path"
        }
        Remove-Item -LiteralPath $directory.FullName -Recurse -Force
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
$versionFile = Join-Path $repoRoot 'C\SuiZipVersion.h'
$versionSource = [IO.File]::ReadAllText($versionFile)
$versionParts = foreach ($part in @('MAJOR', 'MINOR', 'PATCH')) {
    $pattern = '(?m)^#define[ \t]+MY_PRODUCT_VER_' + $part + '[ \t]+(0|[1-9][0-9]*)[ \t]*\r?$'
    $matched = [regex]::Match($versionSource, $pattern)
    $number = [uint16]0
    if (-not $matched.Success -or -not [uint16]::TryParse($matched.Groups[1].Value, [ref]$number)) {
        throw "Cannot read a valid MY_PRODUCT_VER_$part from $versionFile."
    }
    $number
}
$version = $versionParts -join '.'
if (-not $Name) {
    $nameFile = Join-Path $repoRoot 'C\7zVersion.h'
    $nameMatch = [regex]::Match([IO.File]::ReadAllText($nameFile), '(?m)^\s*#define\s+MY_PRODUCT_NAME\s+"([A-Za-z0-9][A-Za-z0-9_-]*)"')
    if (-not $nameMatch.Success) { throw "Cannot read a valid MY_PRODUCT_NAME from $nameFile." }
    $Name = $nameMatch.Groups[1].Value
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Visual Studio Installer was not found. Install the Desktop development with C++ workload.'
}
$vsRoot = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ x86/x64 build tools were not found.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'

$dist = Join-Path $repoRoot 'dist'
$id = [Guid]::NewGuid().ToString('N')
$work = Join-Path $dist ".pack-$id"
$buildOutput = "pack_out_$id"
$payload = Join-Path $work 'payload'
$cache = Join-Path $dist '.pack-cache'
$cachedInstaller = Join-Path $cache "7zip-$ResourceVersion-x64.exe"
$filename = "$Name-$version-x64-setup.exe"
$destination = Join-Path $dist $filename

$targets = @(
    @{ Path = 'CPP\7zip\Bundles\Format7zF'; File = '7z.dll' },
    @{ Path = 'CPP\7zip\UI\Console'; File = '7z.exe' },
    @{ Path = 'CPP\7zip\UI\FileManager'; File = '7zFM.exe' },
    @{ Path = 'CPP\7zip\UI\GUI'; File = '7zG.exe' },
    @{ Path = 'CPP\7zip\UI\Explorer'; File = '7-zip.dll' },
    @{ Path = 'CPP\7zip\Bundles\SFXWin'; File = '7z.sfx' },
    @{ Path = 'CPP\7zip\Bundles\SFXCon'; File = '7zCon.sfx' },
    @{ Path = 'C\Util\7zipInstall'; File = '7zipInstall.exe'; Stub = $true },
    @{ Path = 'C\Util\7zipUninstall'; File = '7zipUninstall.exe'; Rename = 'Uninstall.exe' }
)

try {
    New-Item -ItemType Directory -Path $payload -Force | Out-Null
    $buildDirectories = @()
    $commands = @('@echo off', 'setlocal DisableDelayedExpansion', ('call "{0}" x64 || exit /b 1' -f $vcvars))
    foreach ($item in $targets) {
        $projectPath = Join-Path $repoRoot $item.Path
        $outputDirectory = Join-Path $projectPath $buildOutput
        $buildDirectories += $outputDirectory
        $commands += 'cd /d "{0}" || exit /b 1' -f $projectPath
        $commands += "nmake /nologo /s PLATFORM=x64 O=$buildOutput || exit /b 1"
        $item.Output = Join-Path $outputDirectory $item.File
    }
    $commands += 'exit /b 0'
    $buildScript = Join-Path $work 'build.cmd'
    [IO.File]::WriteAllLines($buildScript, [string[]]$commands, [Text.Encoding]::Default)
    Write-Host "Building $Name $version for x64..."
    Push-Location -LiteralPath $work
    try { Invoke-Checked $env:ComSpec @('/d', '/c', 'build.cmd') } finally { Pop-Location }

    foreach ($item in $targets) {
        if ($item.Stub) {
            $stub = $item.Output
        } else {
            $outputFile = $item.File
            if ($item.Rename) { $outputFile = $item.Rename }
            Copy-Item -LiteralPath $item.Output -Destination (Join-Path $payload $outputFile)
        }
    }

    $newResourceSource = $false
    if ($ResourceInstaller) {
        $resources = (Resolve-Path -LiteralPath $ResourceInstaller).ProviderPath
        $newResourceSource = $true
    } elseif (Test-Path -LiteralPath $cachedInstaller -PathType Leaf) {
        $resources = $cachedInstaller
    } else {
        $resources = Join-Path $work 'upstream.exe'
        $releaseFile = '7z' + $ResourceVersion.Replace('.', '') + '-x64.exe'
        $releaseUrl = "https://github.com/ip7z/7zip/releases/download/$ResourceVersion/$releaseFile"
        Write-Host "Downloading 7-Zip $ResourceVersion resources..."
        Invoke-Checked curl.exe @('--fail', '--location', '--silent', '--show-error', '--retry', '2', '--max-time', '180', '--output', $resources, $releaseUrl)
        $newResourceSource = $true
    }

    $sevenZip = Join-Path $payload '7z.exe'
    $resourceFiles = @('Lang\zh-cn.txt', 'Lang\en.ttt', '7-zip.chm', 'descript.ion', 'History.txt', 'readme.txt')
    Invoke-Checked $sevenZip (@('x', $resources, "-o$payload", '-y', '-bso0') + $resourceFiles)
    foreach ($file in $resourceFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $payload $file) -PathType Leaf)) {
            throw "The resource installer does not contain $file."
        }
    }
    foreach ($nameOfLicense in @('License.txt', 'copying.txt', 'unRarLicense.txt')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot "DOC\$nameOfLicense") -Destination $payload
    }
    if ($newResourceSource) {
        New-Item -ItemType Directory -Path $cache -Force | Out-Null
        if ($resources -ne $cachedInstaller) {
            $cacheCandidate = Join-Path $work 'resource-cache.exe'
            Copy-Item -LiteralPath $resources -Destination $cacheCandidate
            Publish-File $cacheCandidate $cachedInstaller
        }
    }

    $archive = Join-Path $work 'install-files.7z'
    Push-Location -LiteralPath $payload
    try {
        Invoke-Checked $sevenZip @('a', '-t7z', $archive, '*', '-m0=LZMA:d=5m', '-mf=BCJ2', '-mx=9', '-mmt=2', '-mqs=on')
    } finally { Pop-Location }

    $candidate = Join-Path $work $filename
    $output = [IO.File]::Open($candidate, [IO.FileMode]::CreateNew)
    try {
        foreach ($file in @($stub, $archive)) {
            $inputFile = [IO.File]::OpenRead($file)
            try { $inputFile.CopyTo($output) } finally { $inputFile.Dispose() }
        }
    } finally { $output.Dispose() }
    Invoke-Checked $sevenZip @('t', $candidate)

    Publish-File $candidate $destination
    $installer = Get-Item -LiteralPath $destination
    Write-Host ('Installer: {0} ({1:N0} bytes)' -f $installer.FullName, $installer.Length)
} finally {
    foreach ($directory in $buildDirectories) { Remove-BuildDirectory $directory }
    Remove-BuildDirectory $work
}
