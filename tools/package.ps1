<#
.SYNOPSIS
    Make the release zip a player installs DAOLib from.

.DESCRIPTION
    After `.\tools\build.ps1 -Preset release`, writes dist\DAOLib-<version>.zip,
    laid out as the game's install folder, so a player extracts it there (beside
    bin_ship) as SKSE is installed into Skyrim's: bin_ship\dinput8.dll and its .pdb (so
    the crash logger names DAOLib's frames), bin_ship\daolib\addresses\*.bin (one per
    build of the game the address library knows: GOG, Steam, EA), and README.txt
    saying where it goes, how to see that it works and how to remove it, and which
    release of the address library the files are: the tag extern\DAOAddressLibrary is
    at, where a DAOLib release holds it (DAOLib #54). No plugin goes
    in: a mod ships its own folder, bin_ship\daolib\plugins\<name>\, the same way
    (DAOLib #26). A .dazip cannot carry the loader: DAUpdater unpacks into the Documents
    folder, never the game's (DAOLib #28).

    The version is the project's (CMakeLists.txt). Nothing is copied into the game.

    -Trial names a zip that is not a release: one given to a player to try what is on
    master between releases. It is dist\DAOLib-<version>-<trial>.zip, beside the
    release's own and not over it, and its README says which commit it is.

.EXAMPLE
    .\tools\package.ps1                 # from build\release
    .\tools\package.ps1 -Preset debug   # a debug build's, for a trial
    .\tools\package.ps1 -Trial dinput8  # dist\DAOLib-<version>-dinput8.zip, not a release
#>
[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string] $Preset = 'release',
    [ValidatePattern('^[A-Za-z0-9._-]*$')]
    [string] $Trial
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
. (Join-Path $PSScriptRoot 'address_release.ps1')
$repo = Split-Path -Parent $PSScriptRoot

$build = Join-Path $repo "build\$Preset"
$builtLoader = Join-Path $build 'dinput8.dll'
if (-not (Test-Path $builtLoader)) { throw "No $builtLoader; run .\tools\build.ps1 -Preset $Preset first" }
$builtPdb = Join-Path $build 'dinput8.pdb'
if (-not (Test-Path $builtPdb)) { throw "No $builtPdb; a crash log names none of DAOLib's frames without it" }
# What a player is given does not say who built it. The linker writes the folder of its
# temporary files into a PDB, which tools\build.ps1 keeps in the build tree for that
# reason (DAOLib #55); a loader linked any other way is caught here.
$profileFolder = [Environment]::GetFolderPath('UserProfile')
foreach ($file in $builtLoader, $builtPdb) {
    $bytes = [Text.Encoding]::Latin1.GetString([IO.File]::ReadAllBytes($file))
    foreach ($written in $profileFolder, ($profileFolder.ToCharArray() -join "`0")) {
        if ($bytes.IndexOf($written, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "$file holds the path of this user's folder; delete $builtLoader and build with .\tools\build.ps1 -Preset $Preset, which links it afresh"
        }
    }
}
$builtAddresses = Join-Path $build 'daolib\addresses'
$addressFiles = @(Get-ChildItem -Path $builtAddresses -Filter '*.bin' -ErrorAction SilentlyContinue)
if ($addressFiles.Count -eq 0) { throw "No address files in $builtAddresses; the build makes them" }

$cmake = Get-Content -Raw (Join-Path $repo 'CMakeLists.txt')
if ($cmake -notmatch 'project\(DAOLib VERSION (\S+)') { throw 'No version in CMakeLists.txt' }
$version = $Matches[1]
$name = "DAOLib-$version"
$title = "DAOLib $version"
if ($Trial) {
    $name += "-$Trial"
    $title += ", a trial build ($Trial, commit $(git -C $repo rev-parse --short HEAD)), not a release"
}

# The address library has releases of its own, the same files in a zip of their own, so
# the README says which these are. At no tag they are a commit's, which a trial may be
# and a release is not.
$addressRepo = Join-Path $repo 'extern\DAOAddressLibrary'
$addressVersion = Get-AddressLibraryRelease $addressRepo
if ($addressVersion) {
    $addressRelease = "DAOAddressLibrary $addressVersion"
}
else {
    $addressRelease = "DAOAddressLibrary at its commit $(git -C $addressRepo rev-parse --short HEAD), between releases"
    Show-Warn "extern\DAOAddressLibrary is at no release's tag; a DAOLib release holds it at one"
}

$out = Join-Path $repo 'dist'
$stage = Join-Path $out $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
$binShip = Join-Path $stage 'bin_ship'
$addresses = Join-Path $binShip 'daolib\addresses'
New-Item -ItemType Directory -Force $addresses | Out-Null

Copy-Item -Force $builtLoader $binShip
Copy-Item -Force $builtPdb $binShip
foreach ($file in $addressFiles) { Copy-Item -Force $file.FullName $addresses }
# The code index: which build's file an exe's code is, for an exe whose headers no file is
# named for (a repack of a known build, DAOLib #110).
foreach ($file in Get-ChildItem -Path $builtAddresses -Filter '*.index') { Copy-Item -Force $file.FullName $addresses }

$readme = @"
$title
A plugin loader and library for Dragon Age: Origins (1.05, the GOG, Steam and EA builds).
https://github.com/Whocares66-Dev/DAOLib (GPLv3)

Requirements
  The latest Microsoft Visual C++ Redistributable, the x86 one (vc_redist.x86.exe) even on
  64-bit Windows: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
  Without it, or with an older one, the game does not start (error 0xc0000142).

Installing
  Extract this archive into the game's install folder, the one that holds bin_ship.
  dinput8.dll goes beside DAOrigins.exe, and the address files into bin_ship\daolib\addresses.
  dinput8.pdb, beside dinput8.dll, is what lets a crash log name DAOLib's own code.
  Start the game as you always do: Galaxy, Steam, the EA app or the exe itself.

Coming from DAOLib 0.1.0
  Delete bin_ship\version.dll and bin_ship\version.pdb first. The loader was version.dll
  then and is dinput8.dll now, and with both there every mod is loaded twice.

Mods for DAOLib
  Each mod is a folder of its own, extracted into bin_ship\daolib\plugins\<mod>\.
  Without one, DAOLib still corrects a few faults of the game's own and makes it faster.

Fixes and fast loading
  The loader corrects faults of the game's own, always, and makes it faster: the engine's
  memory from the system's heap, and every processor for the game. Fast loading is off
  until you tick "Fast loading" under Performance on the options screen's Game tab.
  https://whocares66-dev.github.io/DAOLib/loader.html lists each.

Compatibility with other mods
  A mod that is a dinput8.dll: two files cannot have the name. Keep DAOLib's as
  dinput8.dll and put the other mod's beside it renamed dinput8.chain.dll: DAOLib passes
  the game's calls on to it, and loader.log says "another mod's dinput8.dll is chained".
  One such mod at a time.

  ASI mods (DAFix and others): DAOLib loads *.asi files from bin_ship, bin_ship\scripts
  and bin_ship\plugins, as Ultimate ASI Loader does. Install only the mod's .asi and .ini;
  leave out the dinput8.dll and dinput8.ini that come with it, which are Ultimate ASI
  Loader. Renamed dinput8.chain.dll, that loader closes the game. loader.log says
  "loaded, an ASI plugin".
  https://whocares66-dev.github.io/DAOLib/compatibility.html has more.

Seeing that it works
  The loader writes Documents\BioWare\Dragon Age\daolib\loader.log at each start:
  each mod's line says "loaded", and a crash or a long freeze leaves a log there too.

Address files
  Those in bin_ship\daolib\addresses are $addressRelease
  (https://github.com/Whocares66-Dev/DAOAddressLibrary/releases). They say where things are
  in each build of the game, and a newer release's serve every older DAOLib and mod.
  Extracting this archive replaces the ones already there, newer or not. If a mod asked for
  a newer Address Library, extract that again after this. loader.log's line
  "address library: N IDs for this build" says which are in place: N never falls from one
  release to the next.

Removing
  Delete bin_ship\dinput8.dll, bin_ship\dinput8.pdb and the bin_ship\daolib folder.
  The game is then as it was; DAOLib changes none of its own files.
"@
Set-Content -Path (Join-Path $stage 'README.txt') -Value $readme -Encoding utf8

$zip = Join-Path $out "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Remove-Item -Recurse -Force $stage
Show-Ok "package -> $zip"
