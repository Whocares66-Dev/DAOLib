<#
.SYNOPSIS
    Copy a build's loader and plugins into the game's bin_ship folder, or take them out.

.DESCRIPTION
    After `.\tools\build.ps1 -Preset <preset>`, puts build\<preset>\version.dll beside
    DAOrigins.exe and build\<preset>\daolib\plugins\*.dll under bin_ship\daolib\plugins,
    each with its .pdb, so the crash logger can name our frames (dev/CRASHES.md). With
    game\ghidra\functions.tsv present (dev/PROBE.md), copies it to bin_ship\daolib too,
    so the crash logger can name the game's. The game must not be running: it holds
    those files open.

    The examples (examples/, example_<part>.dll) go in only when named with -Examples,
    since each changes the game in its own way (examples/camera moves the camera, and
    would fight another plugin that does): -Examples camera,saves, or -Examples * for
    all. An example not named is taken out of the game if it is there.

    -Remove deletes them again, and the whole bin_ship\daolib folder, which returns the
    game to stock: version.dll is ours alone, the system's stays in SysWOW64 untouched.
    The logs are elsewhere (Documents) and stay.

.EXAMPLE
    .\tools\deploy.ps1                 # debug build into the GOG install
    .\tools\deploy.ps1 -Examples saves  # and examples\saves
    .\tools\deploy.ps1 -Preset release
    .\tools\deploy.ps1 -Remove
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string] $Preset = 'debug',
    [switch] $Remove,
    [string[]] $Examples = @(),
    [string] $Game = 'C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins\bin_ship'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
$repo = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path (Join-Path $Game 'DAOrigins.exe'))) { throw "No DAOrigins.exe in $Game" }
# The game holds its DLLs open. Another install of the game has the same
# process name (the EA version beside GOG's), so the running one is asked
# where it runs from: only this install's blocks the copy. One whose path
# cannot be read is taken for this install's.
$exe = Join-Path $Game 'DAOrigins.exe'
foreach ($running in @(Get-Process -Name DAOrigins -ErrorAction SilentlyContinue)) {
    $from = $running.Path
    if (-not $from -or [string]::Equals($from, $exe, [StringComparison]::OrdinalIgnoreCase)) {
        $where = if ($from) { $from } else { 'a path that cannot be read' }
        throw "DAOrigins.exe is running from $where (process $($running.Id)); close it first"
    }
    Show-Info "another install's game is running, from $from (process $($running.Id)); it does not hold this install's files"
}

$loader = Join-Path $Game 'version.dll'
$loaderPdb = Join-Path $Game 'version.pdb'
$daolib = Join-Path $Game 'daolib'
$plugins = Join-Path $daolib 'plugins'

if ($Remove) {
    foreach ($f in @($loader, $loaderPdb)) {
        if (Test-Path $f) { Remove-Item -Force $f; Show-Ok "removed $f" }
    }
    if (Test-Path $daolib) { Remove-Item -Recurse -Force $daolib; Show-Ok "removed $daolib" }
    return
}

$build = Join-Path $repo "build\$Preset"
$builtLoader = Join-Path $build 'version.dll'
if (-not (Test-Path $builtLoader)) { throw "No $builtLoader; run .\tools\build.ps1 -Preset $Preset first" }

New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item -Force $builtLoader $loader
$builtPdb = Join-Path $build 'version.pdb'
if (Test-Path $builtPdb) { Copy-Item -Force $builtPdb $loaderPdb }
Show-Ok "loader  -> $loader"
foreach ($dll in Get-ChildItem -Path (Join-Path $build 'daolib\plugins') -Filter '*.dll') {
    if ($dll.BaseName -like 'example_*') {
        $part = $dll.BaseName.Substring('example_'.Length)
        $wanted = $Examples | Where-Object { $part -like $_ }
        if (-not $wanted) {
            $there = Join-Path $plugins $dll.Name
            if (Test-Path $there) {
                Remove-Item -Force $there
                Remove-Item -Force -ErrorAction SilentlyContinue ([IO.Path]::ChangeExtension($there, '.pdb'))
                Show-Ok "example $part taken out (not named with -Examples)"
            }
            continue
        }
    }
    Copy-Item -Force $dll.FullName (Join-Path $plugins $dll.Name)
    $pdb = [IO.Path]::ChangeExtension($dll.FullName, '.pdb')
    if (Test-Path $pdb) { Copy-Item -Force $pdb $plugins }
    Show-Ok "plugin  -> $(Join-Path $plugins $dll.Name)"
}
$functions = Join-Path $repo 'game\ghidra\functions.tsv'
if (Test-Path $functions) {
    Copy-Item -Force $functions (Join-Path $daolib 'functions.tsv')
    Show-Ok "names   -> $(Join-Path $daolib 'functions.tsv')"
}
else {
    Show-Warn "no game\ghidra\functions.tsv: crash logs will not name the game's functions (dev/PROBE.md)"
}
Show-Info "logs land in $([Environment]::GetFolderPath('MyDocuments'))\BioWare\Dragon Age\daolib\"
