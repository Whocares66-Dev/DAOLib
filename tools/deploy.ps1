<#
.SYNOPSIS
    Copy a build's loader and plugins into the game's bin_ship folder, or take them out.

.DESCRIPTION
    After `.\tools\build.ps1 -Preset <preset>`, puts build\<preset>\version.dll beside
    DAOrigins.exe and each plugin's folder, build\<preset>\daolib\plugins\<name>\ (its DLL,
    its .pdb, so the crash logger can name our frames (dev/CRASHES.md), and its own files),
    under bin_ship\daolib\plugins, and
    the address library's files (build\<preset>\daolib\addresses\*.bin, one per build
    of the game) under bin_ship\daolib\addresses, where the loader reads its own. With
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

    -Install picks the game: gog (the default), ea, the EA app's install, or steam,
    Steam's (dev/BUILDS.md), each of which takes the same files in the same places.
    Steam's library on this machine is on E:, writable as it is. The EA app installs
    under Program Files with no write access for the user, so its bin_ship needs that
    granted once, from an elevated shell:
        icacls "C:\Program Files\EA Games\Dragon Age\bin_ship" /grant "${env:USERNAME}:(OI)(CI)M"
    -Game names a bin_ship folder that is none of them.

.EXAMPLE
    .\tools\deploy.ps1                 # debug build into the GOG install
    .\tools\deploy.ps1 -Install ea     # into the EA app's
    .\tools\deploy.ps1 -Install steam  # into Steam's
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
    [ValidateSet('gog', 'ea', 'steam')]
    [string] $Install = 'gog',
    [string] $Game
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
$repo = Split-Path -Parent $PSScriptRoot

if (-not $Game) {
    $Game = @{
        gog   = 'C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins\bin_ship'
        ea    = 'C:\Program Files\EA Games\Dragon Age\bin_ship'
        steam = 'E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition\bin_ship'
    }[$Install]
}
if (-not (Test-Path (Join-Path $Game 'DAOrigins.exe'))) { throw "No DAOrigins.exe in $Game" }
# The game holds its DLLs open. Another install of the game has the same
# process name (the EA version beside GOG's), so the running one is asked
# where it runs from: only this install's blocks the copy. One whose path
# cannot be read is taken for this install's.
$exe = (Resolve-Path (Join-Path $Game 'DAOrigins.exe')).Path
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
# Each plugin is a folder of its own, its DLL and its files (DAOLib #26);
# the loader loads daolib\plugins\<name>\<name>.dll from each.
foreach ($folder in Get-ChildItem -Path (Join-Path $build 'daolib\plugins') -Directory) {
    $name = $folder.Name
    if (-not (Test-Path (Join-Path $folder.FullName "$name.dll"))) { continue }
    $there = Join-Path $plugins $name
    # The old layout's loose DLL, which the loader no longer loads.
    foreach ($loose in @("$name.dll", "$name.pdb")) {
        $old = Join-Path $plugins $loose
        if (Test-Path $old) { Remove-Item -Force $old; Show-Ok "removed $old (a plugin is a folder now)" }
    }
    if ($name -like 'example_*') {
        $part = $name.Substring('example_'.Length)
        $wanted = $Examples | Where-Object { $part -like $_ }
        if (-not $wanted) {
            if (Test-Path $there) {
                Remove-Item -Recurse -Force $there
                Show-Ok "example $part taken out (not named with -Examples)"
            }
            continue
        }
    }
    New-Item -ItemType Directory -Force $there | Out-Null
    # The plugin and its files, not the linker's leftovers.
    Copy-Item -Force -Recurse (Join-Path $folder.FullName '*') $there -Exclude '*.ilk', '*.exp', '*.lib'
    Show-Ok "plugin  -> $there"
}
# The address library's files, one per build of the game: the loader and every
# plugin read the one for the game they are in, and load nothing without it.
$builtAddresses = Join-Path $build 'daolib\addresses'
if (-not (Test-Path (Join-Path $builtAddresses '*.bin'))) { throw "No address files in $builtAddresses; the build makes them" }
$addresses = Join-Path $daolib 'addresses'
New-Item -ItemType Directory -Force $addresses | Out-Null
$built = @(Get-ChildItem -Path $builtAddresses -Filter '*.bin' | ForEach-Object { $_.Name })
# How many IDs a file has an address for: the words after its 0x24-byte
# header that are not 0xFFFFFFFF. IDs are only ever added, so of two files
# for one build the one that knows more is the newer, and plugins built on
# different DAOLibs share the folder: one already there that knows more is
# kept, for the plugin that brought it.
function Get-KnownIdCount([string] $path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $known = 0
    for ($at = 0x24; $at + 4 -le $bytes.Length; $at += 4) {
        if ([BitConverter]::ToUInt32($bytes, $at) -ne [uint32]::MaxValue) { $known++ }
    }
    return $known
}
foreach ($name in $built) {
    $from = Join-Path $builtAddresses $name
    $to = Join-Path $addresses $name
    if ((Test-Path $to) -and (Get-KnownIdCount $to) -gt (Get-KnownIdCount $from)) {
        Show-Info "kept $name already there: it knows more IDs than this build's"
        continue
    }
    Copy-Item -Force $from $to
}
# The folder holds what this build has and nothing else: a file the address
# library no longer writes (the names by link stamp alone, before its
# ae992b3) would be stale here, and a loader that still asked for it would
# read an old table.
foreach ($file in Get-ChildItem -Path $addresses -Filter 'daoaddresses-*.bin') {
    if ($built -notcontains $file.Name) {
        Remove-Item -Force $file.FullName
        Show-Info "removed $($file.Name): the build has no such file"
    }
}
Show-Ok "addresses -> $addresses"
$functions = Join-Path $repo 'game\ghidra\functions.tsv'
if (Test-Path $functions) {
    Copy-Item -Force $functions (Join-Path $daolib 'functions.tsv')
    Show-Ok "names   -> $(Join-Path $daolib 'functions.tsv')"
}
else {
    Show-Warn "no game\ghidra\functions.tsv: crash logs will not name the game's functions (dev/PROBE.md)"
}
Show-Info "logs land in $([Environment]::GetFolderPath('MyDocuments'))\BioWare\Dragon Age\daolib\"
