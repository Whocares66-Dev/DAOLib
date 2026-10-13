<#
.SYNOPSIS
    Build DAOLib from any shell -- no "x86 Native Tools Command Prompt" needed.

.DESCRIPTION
    CMakePresets.json pins CMAKE_CXX_COMPILER=cl.exe, which only exists inside the
    Visual Studio developer environment. This script locates the VS install, imports
    the target environment into the current session, and then runs CMake: 32-bit
    (vcvarsall.bat amd64_x86: a 64-bit compiler producing x86 code, the game's
    architecture), or 64-bit for core-x64.

.PARAMETER Preset
    debug       loader (dinput8.dll), daolib, the plugins, and the tests.
                The first run compiles spdlog, Detours and Catch2 through vcpkg.
    release     same, optimized.
    debug-asan  same, under AddressSanitizer; for -Test. The DLLs it makes are
                not for the game (they need the ASan runtime beside the exe).
    debug-analyze
                same, our own targets under MSVC's static analyser (/analyze),
                a warning failing the build. Its own tree, so the debug tree's
                compile database, which clang-tidy reads, never holds /analyze
                (clang rejects it). Not for -Lint.
    core-x64    dao/core and its tests alone, built 64-bit: the build a tool
                that reads the game's files with no game links (DAOLib #106).
                The first run compiles Catch2 for x64. Not for -Lint or
                -Format, which the debug preset's tree covers.

.PARAMETER Format
    After building, rewrite every source in place with its formatter: the
    `format` target (cmake/Quality.cmake).

.PARAMETER Lint
    After building (and testing, with -Test), run the linters: clang-tidy over
    every C++ file, ruff over the Python, PSScriptAnalyzer over the PowerShell.
    The `tidy` target; a finding fails the run.

.PARAMETER Mutants
    After building, run tools\mutants.py: the core broken on purpose, one
    promise at a time, each mutant built and tested in a worker's copy of the
    tree under build\mutants, as many at once as -Jobs. With -Preset debug,
    whose packages the workers use. A mutant the tests do not catch fails the
    run. Minutes, not seconds: not part of the gate before every commit.

.PARAMETER MutantNames
    With -Mutants, only the mutants whose name holds one of these: one
    mutant checked again after its test was mended, in seconds.

.PARAMETER Jobs
    How many compilations to run at once. The default leaves two of the
    machine's cores to everything else. Also exported as
    CMAKE_BUILD_PARALLEL_LEVEL, so a `cmake --build` run afterwards in the same
    shell -- the `tidy` and `format` targets -- takes the same limit.

.EXAMPLE
    .\tools\build.ps1 -Preset debug -Test
    .\tools\build.ps1 -Preset debug-asan -Test
    .\tools\build.ps1 -Preset debug-analyze
    .\tools\build.ps1 -Preset release
    .\tools\build.ps1 -Preset core-x64 -Test
    .\tools\build.ps1 -Preset debug -Mutants
    .\tools\build.ps1 -Preset debug -Mutants -MutantNames msvc8-list
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release', 'debug-asan', 'debug-analyze', 'core-x64')]
    [string] $Preset = 'debug',
    [switch] $Test,
    [switch] $Format,
    [switch] $Lint,
    [switch] $Mutants,
    [switch] $Fresh,
    [int] $Jobs = 0,
    [string[]] $MutantNames = @()
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
if ($Mutants -and $Preset -ne 'debug') {
    throw 'the mutants build from the debug preset''s packages; run -Mutants with -Preset debug.'
}
if ($Lint -and $Preset -eq 'debug-analyze') {
    throw 'clang-tidy cannot read a compile database holding /analyze; lint with -Preset debug.'
}
if (($Lint -or $Format) -and $Preset -eq 'core-x64') {
    throw 'core-x64 builds dao/core alone, with no format or lint targets; format and lint with -Preset debug.'
}
# The compiler's target: x86 for everything that goes into the game, x64 for
# the core alone.
$targetArch = if ($Preset -eq 'core-x64') { 'amd64' } else { 'amd64_x86' }
$repo = Split-Path -Parent $PSScriptRoot

function Get-VcVarsAllPath {
    # Do NOT locate the toolset with `vswhere -latest -requires ...`: a VS install
    # flagged isComplete=False is invisible to it while cl.exe sits on disk. Probe
    # the filesystem and let the presence of vcvarsall.bat be the answer.
    $candidates = @()
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $candidates += & $vswhere -all -prerelease -products * -property installationPath
    }
    foreach ($root in ${env:ProgramFiles}, ${env:ProgramFiles(x86)}) {
        foreach ($ed in 'Enterprise', 'Professional', 'Community', 'BuildTools') {
            $candidates += (Join-Path $root "Microsoft Visual Studio\2022\$ed")
        }
    }
    foreach ($c in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        $vcvars = Join-Path $c 'VC\Auxiliary\Build\vcvarsall.bat'
        if (Test-Path $vcvars) { return $vcvars }
    }
    return $null
}

function Import-DeveloperEnvironment {
    $vcvars = Get-VcVarsAllPath
    if (-not $vcvars) {
        throw @'
No Visual Studio 2022 C++ toolset found (no vcvarsall.bat anywhere).

Install the C++ workload, with Visual Studio CLOSED:

  & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify `
      --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" `
      --add Microsoft.VisualStudio.Workload.NativeDesktop `
      --add Microsoft.VisualStudio.Component.VC.ASAN `
      --includeRecommended --quiet --norestart
'@
    }
    Show-Line "Importing developer environment from $vcvars ($targetArch)" -Colour DarkGray
    # `set` after vcvarsall dumps the whole environment; replay it into this session.
    $output = & ${env:ComSpec} /s /c "`"$vcvars`" $targetArch >nul 2>&1 && set"
    foreach ($line in $output) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:\$($Matches[1])" -Value $Matches[2] -ErrorAction SilentlyContinue
        }
    }
}

# vcvarsall sets VCPKG_ROOT to the vcpkg bundled inside Visual Studio, which is
# NOT the one this project uses. Preserve whatever was set beforehand.
$userVcpkgRoot = $env:VCPKG_ROOT
Import-DeveloperEnvironment
if ($userVcpkgRoot) {
    if ($env:VCPKG_ROOT -ne $userVcpkgRoot) {
        Show-Line "  restoring VCPKG_ROOT=$userVcpkgRoot (vcvarsall had reset it to $env:VCPKG_ROOT)" -Colour DarkGray
    }
    $env:VCPKG_ROOT = $userVcpkgRoot
}

# LLVM's own release first on PATH, not the older one Visual Studio bundles.
$llvmBin = if ($env:LLVM_ROOT) { Join-Path $env:LLVM_ROOT 'bin' } else { Join-Path $env:ProgramFiles 'LLVM\bin' }
if (Test-Path (Join-Path $llvmBin 'clang-tidy.exe')) {
    $env:PATH = "$llvmBin;$env:PATH"
}

foreach ($tool in 'cl', 'cmake', 'ninja') {
    $found = Get-Command $tool -ErrorAction SilentlyContinue
    if (-not $found) { throw "'$tool' is still not on PATH after importing vcvarsall." }
    Show-Line ("  {0,-6} {1}" -f $tool, $found.Source) -Colour DarkGray
}
if (-not $env:VCPKG_ROOT) { throw 'VCPKG_ROOT is not set; every preset needs vcpkg.' }
if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
    throw "vcpkg.exe missing. Run: & '$env:VCPKG_ROOT\bootstrap-vcpkg.bat'"
}

$buildDir = Join-Path $repo "build\$Preset"
if ($Fresh -and (Test-Path $buildDir)) {
    Show-Line "Removing $buildDir" -Colour Yellow
    Remove-Item -Recurse -Force $buildDir
}

$cores = [Environment]::ProcessorCount
$parallel = if ($Jobs -gt 0) { $Jobs } else { [Math]::Max(1, $cores - 2) }
$env:CMAKE_BUILD_PARALLEL_LEVEL = $parallel
(Get-Process -Id $PID).PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
Show-Line "  jobs   $parallel of $cores cores, below normal priority"

Push-Location $repo
try {
    Show-Line "`n== configure ($Preset) ==" -Colour Cyan
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

    Show-Line "`n== build ($Preset) ==" -Colour Cyan
    # The linker writes the path of its temporary files into a PDB, and the
    # user's own is under their name (C:\Users\<name>\AppData\Local\Temp);
    # the release's PDB is given to players (DAOLib #55). So a build's is in
    # its tree, for the build alone.
    $userTemp = $env:TEMP, $env:TMP
    $env:TEMP = $env:TMP = New-Item -ItemType Directory -Force (Join-Path $buildDir 'tmp')
    try { cmake --build --preset $Preset --parallel $parallel }
    finally { $env:TEMP, $env:TMP = $userTemp }
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

    if ($Test) {
        Show-Line "`n== test ($Preset) ==" -Colour Cyan
        # As many at once as the build runs: the tests share no files (the
        # Python ones make theirs in a temporary directory each) and none
        # goes by the clock. 7 to 12 s one at a time, 4 to 5 s so, the
        # floor the slowest, object_casts' read of the exe (2026-10-03).
        ctest --preset $Preset --parallel $parallel
        if ($LASTEXITCODE -ne 0) { throw "tests failed ($LASTEXITCODE)" }
    }

    if ($Format) {
        Show-Line "`n== format ==" -Colour Cyan
        cmake --build --preset $Preset --target format
        if ($LASTEXITCODE -ne 0) { throw "format failed ($LASTEXITCODE)" }
    }

    if ($Lint) {
        Show-Line "`n== lint ($Preset) ==" -Colour Cyan
        cmake --build --preset $Preset --parallel $parallel --target tidy
        if ($LASTEXITCODE -ne 0) { throw "lint failed ($LASTEXITCODE)" }
    }

    if ($Mutants) {
        Show-Line "`n== mutants ==" -Colour Cyan
        python (Join-Path $PSScriptRoot 'mutants.py') -j $parallel @MutantNames
        if ($LASTEXITCODE -ne 0) { throw "mutants survived or did not build ($LASTEXITCODE)" }
    }
    Show-Line "`nOK" -Colour Green
}
finally {
    Pop-Location
}
