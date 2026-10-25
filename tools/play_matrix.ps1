<#
.SYNOPSIS
    Start every kept exe of the game under Windows compatibility settings, side by side as
    instances, and check that the loader did its whole start in each.

.DESCRIPTION
    dev/TESTING.md says when this is run and why (DAOLib #146).

    A row is one exe under one setting. The exes are those kept in C:\Modding\DAOVersions
    (-Versions), each checked against its MANIFEST.sha256 first. Each row is staged in a folder
    of its own under build\matrix: a bin_ship of the store's install with the row's exe, the
    loader, the address files and the plugins of build\<Preset> (the plugins that are not
    examples), hard-linked from one copy of each; and junctions to the install's other folders,
    which the game only reads. A setting is a registry value keyed by the exe's path, so each
    row's exe has its own path for its own setting. Nothing is deployed and no install is written.

    Each row's game is a DAOLib instance (DAOLIB_INSTANCE=matrix-<exe>-<n>), with its own mutex
    and profile, so the rows run together, -Batch at a time, and beside any other game. A
    row's profile is given the default profile's Settings and marked seeded before its first
    start, so the player's saves are never copied into it.

    THIS STARTS THE GAME, -Batch copies at a time (8), each for -Seconds, and ends them.

    Without -All the rows are the two settings a merge to master is seen under, none and
    Disable fullscreen optimizations, for GOG's and Steam's exes. -All is the whole matrix for
    a release: every kept exe that runs, under those two, the three high DPI overrides and the
    compatibility modes from Windows XP SP2 to Windows 8. -Exes and -Layers name others.

    A row passes when all of these hold:

      - the DLL of the loader's name in the process is the row's, not the system's
      - the row's loader.log, written by this run, says the game's start was reached (the
        entry point in an unwrapped exe, the CRT's start in a wrapped one)
      - the engine heap went in
      - every plugin has its "<name>.dll: loaded" line
      - no crash, hang or exit log was written
      - the process is still there after -Seconds, and its window is the game's own and
        answering by -Answer seconds more: with a dozen copies starting at once, the last ones
        started reach the menu later than one alone does

    Steam's stock exe hands off to Steam and its launcher's window unless SteamAppId and
    SteamGameId are in its environment, which this sets; Steam must be running.

    EA's exe cannot be staged: it hands off to the EA app, which starts the game in the one
    folder HKLM\SOFTWARE\WOW6432Node\BioWare\Dragon Age's Path names, as the game as ever. -EaApp
    runs its rows after the others, one at a time, in EA's install, which needs the loader and
    a plugin deployed there (.\tools\deploy.ps1 -Install ea); where Path names another install
    (Steam's installer writes it), -SwitchPath sets it to EA's for the runs and puts it back,
    from an elevated shell.

    Each row's exe is a path of its own, and the game opens a port, so Windows Firewall asks
    about each the first time it runs. -Firewall, from an elevated shell, adds an inbound block
    rule for every row's exe path, every kept exe under every setting, in the rule group
    "DAOLib play matrix", replacing the group's rules if it has any, and starts nothing.

    Not in the matrix, and why (dev/TESTING.md): 8-bit colour, 16-bit colour and 640 x 480,
    which change the desktop's own display mode while the game is up; Run as administrator,
    which asks at a prompt.

.EXAMPLE
    .\tools\play_matrix.ps1                                  # the merge's rows, from build\debug
    .\tools\play_matrix.ps1 -Preset release -All             # the whole matrix, for a release
    .\tools\play_matrix.ps1 -Preset release -All -EaApp -SwitchPath
    .\tools\play_matrix.ps1 -Firewall                        # once, elevated: no firewall prompts
    .\tools\play_matrix.ps1 -Exes nexus-laa -Layers WIN7RTM
#>
[CmdletBinding()]
param(
    [string] $Preset = 'debug',
    [switch] $All,
    [string[]] $Exes,
    [string[]] $Layers,
    [int] $Seconds = 30,
    [int] $Answer = 60,
    [int] $Batch = 8,
    [string] $Versions = 'C:\Modding\DAOVersions',
    [switch] $EaApp,
    [switch] $SwitchPath,
    [switch] $Firewall
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
# A throw run inside a shell ends the script as an exception, which a `*>` redirect does not
# take: the log had only the lines before it, and not why the script stopped (a -Remove that
# refused for EA's game still running, 2026-10-23). So it is said as a line first.
trap { Show-Bad $_.Exception.Message; break }

$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo "build\$Preset"
$stage = Join-Path $repo 'build\matrix'
$loaderName = 'dinput8.dll'
# The layers' names as the Compatibility tab writes them; '' is no setting at all.
$merge = @('', 'DISABLEDXMAXIMIZEDWINDOWEDMODE')
$whole = $merge + @('HIGHDPIAWARE', 'DPIUNAWARE', 'GDIDPISCALING DPIUNAWARE', 'WINXPSP2', 'WINXPSP3', 'VISTARTM',
    'VISTASP1', 'VISTASP2', 'WIN7RTM', 'WIN8RTM')
if (-not $PSBoundParameters.ContainsKey('Layers')) { $Layers = if ($All) { $whole } else { $merge } }
$installs = @{
    gog   = 'C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins'
    steam = 'E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition'
    ea    = 'C:\Program Files\EA Games\Dragon Age'
}
# Each kept exe that runs staged, and the install whose other folders it is played with.
# EA's two are not here: its stock exe is -EaApp's, and its decrypted dump does not run.
$kept = [ordered]@{
    'gog'           = @{ File = 'GOG\DAOrigins-gog-1.05.13263.0.exe'; Install = 'gog' }
    'steam'         = @{ File = 'Steam\DAOrigins-steam-stock-1.05.13263.0.exe'; Install = 'steam'; Steam = $true }
    'steamless'     = @{ File = 'Steam\DAOrigins-steam-steamless-unpacked-1.05.13263.0.exe'; Install = 'steam' }
    'steamless-laa' = @{ File = 'Steam\DAOrigins-steam-steamless-unpacked-laa-1.05.13263.0.exe'; Install = 'steam' }
    'nexus'         = @{ File = 'Steam_Decrypted\DAOrigins-steam-nexus-decrypted-1.05.13263.0.exe'; Install = 'steam' }
    'nexus-laa'     = @{ File = 'Steam_Decrypted\DAOrigins-steam-nexus-decrypted-laa-1.05.13263.0.exe'; Install = 'steam' }
}
if (-not $PSBoundParameters.ContainsKey('Exes')) { $Exes = if ($All) { @($kept.Keys) } else { @('gog', 'steam') } }
foreach ($key in $Exes) { if (-not $kept.Contains($key)) { throw "No kept exe '$key'; there are $($kept.Keys -join ', ')" } }
$steamAppId = '47810'
$layersKey = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers'
$pathKey = 'HKLM:\SOFTWARE\WOW6432Node\BioWare\Dragon Age'
$documents = [Environment]::GetFolderPath('MyDocuments')
$instances = Join-Path $env:LOCALAPPDATA 'DAOLib\instances'

# The processes this script started, and the games it found of its rows (a start that hands off
# is another process): the cleanup waits for these alone, since other agents' games run at the
# same time (DAOLib #183).
$script:Pids = [Collections.Generic.HashSet[int]]::new()

function Wait-OwnGame {
    $until = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $until -and @($script:Pids | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }).Count -gt 0) {
        Start-Sleep -Milliseconds 500
    }
}

# Folders under build\matrix hold junctions into the game's installs. A recursive delete
# follows a junction and empties what it points at, so each one goes first, as a link, and the
# rest only once none is left. With -Lenient a stage that still will not go is a warning: the
# removal after the rows, which is not to end the run before EA's rows. Once the base's copy of
# binkw32.dll was refused for the whole twenty seconds with no game running from the stage, held
# by something not found (2026-10-23); the next run's removal, before its rows, must succeed.
function Clear-Stage([switch] $Lenient) {
    if (-not (Test-Path $stage)) { return }
    foreach ($link in @(Get-ChildItem $stage -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue)) {
        [IO.Directory]::Delete($link.FullName, $false)
    }
    if (@(Get-ChildItem $stage -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue).Count -gt 0) {
        throw "A junction is still under $stage; delete it by hand before anything else"
    }
    # The games' files let go within seconds of the process ending, so a refusal is tried again.
    Wait-OwnGame
    for ($try = 1; ; $try++) {
        try { Remove-Item -Recurse -Force $stage -ErrorAction Stop; return }
        catch {
            if ($try -lt 10) { Start-Sleep -Seconds 2; continue }
            if (-not $Lenient) { throw }
            Show-Warn "the stage was left in $stage ($($_.Exception.Message)); the next run removes it"
            return
        }
    }
}

function Get-Run([string] $Exe) {
    @(Get-CimInstance Win32_Process -Filter "Name='DAOrigins.exe'" |
            Where-Object { $_.ExecutablePath -and [string]::Equals($_.ExecutablePath, $Exe, 'OrdinalIgnoreCase') })
}
# Waits on the processes it stopped, by id: one being torn down can have no ExecutablePath,
# which Get-Run leaves out, and a deploy.ps1 -Remove run then found it, its path unreadable,
# and refused to remove anything (2026-10-23).
function Close-Run([string] $Exe) {
    $ids = @(Get-Run $Exe | ForEach-Object { [int] $_.ProcessId })
    foreach ($id in $ids) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
    $until = (Get-Date).AddSeconds(15)
    while (($ids | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }) -and (Get-Date) -lt $until) {
        Start-Sleep -Milliseconds 300
    }
}
# The game as ever holds the mutex "dragonage" (dev/BUILDS.md); a second one exits at once.
function Test-GameAsEver {
    $held = $null
    if ([Threading.Mutex]::TryOpenExisting('dragonage', [ref] $held)) { $held.Dispose(); return $true }
    return $false
}
# The modules of a 32-bit process, which only a 32-bit PowerShell lists.
function Get-GameModule([int] $Id) {
    $shell = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    @(& $shell -NoProfile -Command "(Get-Process -Id $Id).Modules | ForEach-Object { `$_.FileName }")
}

# One copy of what every row of a kind shares, which the rows are hard-linked from: a store's
# bin_ship without any loader or plugin a session left in it, each exe, and build\<Preset>'s
# loader, address files and plugins.
function Build-Base {
    $base = Join-Path $stage 'base'
    $loaderFiles = @('dinput8.dll', 'dinput8.pdb', 'dinput8.chain.dll', 'dinput8.ini', 'version.dll', 'version.pdb')
    foreach ($install in @($Exes | ForEach-Object { $kept[$_].Install } | Sort-Object -Unique)) {
        $to = Join-Path $base "bin_ship-$install"
        New-Item -ItemType Directory -Force $to | Out-Null
        Get-ChildItem (Join-Path $installs[$install] 'bin_ship') -File |
            Where-Object { $_.Name -notin $loaderFiles -and $_.Extension -ne '.asi' -and $_.Name -ne 'DAOrigins.exe' } |
            Copy-Item -Destination $to
    }
    $manifest = @{}
    foreach ($line in Get-Content (Join-Path $Versions 'MANIFEST.sha256')) {
        if ($line -match '^([0-9a-f]{64})\s+(.+)$') { $manifest[$Matches[2].Trim().Replace('/', '\')] = $Matches[1] }
    }
    foreach ($key in $Exes) {
        $file = $kept[$key].File
        $from = Join-Path $Versions $file
        $hash = (Get-FileHash $from -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($manifest[$file] -ne $hash) { throw "$from is not the exe its manifest names" }
        Copy-Item $from (Join-Path $base "$key.exe")
    }
    $loader = Join-Path $base 'loader'
    New-Item -ItemType Directory -Force (Join-Path $loader 'daolib\addresses') | Out-Null
    Copy-Item (Join-Path $build $loaderName) $loader
    if (Test-Path (Join-Path $build 'dinput8.pdb')) { Copy-Item (Join-Path $build 'dinput8.pdb') $loader }
    Copy-Item (Join-Path $build 'daolib\addresses\*') (Join-Path $loader 'daolib\addresses')
    $plugins = @(Get-ChildItem (Join-Path $build 'daolib\plugins') -Directory |
            Where-Object { $_.Name -notlike 'example_*' -and (Test-Path (Join-Path $_.FullName "$($_.Name).dll")) })
    foreach ($plugin in $plugins) {
        $to = Join-Path $loader "daolib\plugins\$($plugin.Name)"
        New-Item -ItemType Directory -Force $to | Out-Null
        Copy-Item (Join-Path $plugin.FullName '*') $to -Recurse -Exclude '*.ilk', '*.exp', '*.lib'
    }
    return @{ Path = $base; Plugins = @($plugins.Name) }
}

# Every file of a folder hard-linked under another, its folders made.
function Copy-AsLink([string] $From, [string] $To) {
    foreach ($file in Get-ChildItem $From -Recurse -File) {
        $there = Join-Path $To $file.FullName.Substring($From.Length).TrimStart('\')
        New-Item -ItemType Directory -Force (Split-Path $there -Parent) | Out-Null
        New-Item -ItemType HardLink -Path $there -Target $file.FullName | Out-Null
    }
}

# A row's folder: the store's bin_ship, the exe and the loader linked into it, and a junction
# to each of the install's other folders.
function Get-RowName([string] $Key, [int] $Index) { "matrix-$Key-$Index" }

function Build-Row([hashtable] $Base, [string] $Key, [int] $Index, [string] $Layer) {
    $name = Get-RowName $Key $Index
    $folder = Join-Path $stage $name
    $install = $installs[$kept[$Key].Install]
    foreach ($dir in Get-ChildItem $install -Directory | Where-Object Name -NE 'bin_ship') {
        New-Item -ItemType Junction -Path (Join-Path $folder $dir.Name) -Target $dir.FullName | Out-Null
    }
    $bin = Join-Path $folder 'bin_ship'
    Copy-AsLink (Join-Path $Base.Path "bin_ship-$($kept[$Key].Install)") $bin
    Copy-AsLink (Join-Path $Base.Path 'loader') $bin
    $exe = Join-Path $bin 'DAOrigins.exe'
    New-Item -ItemType HardLink -Path $exe -Target (Join-Path $Base.Path "$Key.exe") | Out-Null
    # The instance's profile: Settings from the default one, and seeded, so its first start
    # copies none of the player's saves (loader/Instance.cpp).
    $own = Join-Path $instances $name
    $settings = Join-Path $own 'BioWare\Dragon Age\Settings'
    if (-not (Test-Path (Join-Path $own 'seeded'))) {
        New-Item -ItemType Directory -Force $settings | Out-Null
        Copy-Item (Join-Path $documents 'BioWare\Dragon Age\Settings\*') $settings -Recurse -Force
        Set-Content (Join-Path $own 'seeded') 'seeded'
    }
    return [pscustomobject]@{
        Name     = "$Key, $(if ($Layer) { $Layer } else { 'no layer' })"
        Exe      = $exe
        Folder   = $bin
        Layer    = $Layer
        Instance = $name
        Logs     = Join-Path $own 'BioWare\Dragon Age\daolib'
        Steam    = [bool] $kept[$Key].Steam
        Plugins  = $Base.Plugins
        Run      = $null
        Began    = $null
        Answered = $null
    }
}

# CreateProcess with the window shown and not activated (SW_SHOWNOACTIVATE), so a game that starts
# does not take the keys from the window in front; Start-Process has no such style, and a
# minimised fullscreen game stops drawing (DAOLib #198). The environment is the script's own.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class NoActivateStart {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct STARTUPINFO {
        public int cb; public string lpReserved, lpDesktop, lpTitle;
        public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
        public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit, int flags,
        IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    public static int Start(string exe, string dir) {
        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
        si.dwFlags = 1; // STARTF_USESHOWWINDOW
        si.wShowWindow = 4; // SW_SHOWNOACTIVATE
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(exe, "\"" + exe + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, dir, ref si, out pi))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return pi.dwProcessId;
    }
}
'@

function Open-Row($Row) {
    if ($Row.Layer) { New-ItemProperty -Path $layersKey -Name $Row.Exe -Value "~ $($Row.Layer)" -PropertyType String -Force | Out-Null }
    $Row.Began = Get-Date
    $env:DAOLIB_INSTANCE = $Row.Instance
    if ($Row.Steam) { $env:SteamAppId = $steamAppId; $env:SteamGameId = $steamAppId }
    try { [void] $script:Pids.Add([NoActivateStart]::Start($Row.Exe, $Row.Folder)) }
    finally { Remove-Item Env:DAOLIB_INSTANCE, Env:SteamAppId, Env:SteamGameId -ErrorAction SilentlyContinue }
}

function Close-Row($Row) {
    Close-Run $Row.Exe
    if ($Row.Layer) { Remove-ItemProperty -Path $layersKey -Name $Row.Exe -ErrorAction SilentlyContinue }
}

# The run that stays: Steam's and EA's first may be a hand-off that exits.
function Find-Run($Row) {
    $found = Get-Run $Row.Exe | Where-Object { ((Get-Date) - $_.CreationDate).TotalSeconds -ge 12 } | Select-Object -First 1
    if ($found) { [void] $script:Pids.Add([int] $found.ProcessId) }
    return $found
}

# What is wrong with a row whose game has been up its time: none if it passes.
function Test-Row($Row) {
    if (-not $Row.Run) { return @('no game stayed up') }
    $faults = @()
    $process = Get-Process -Id $Row.Run.ProcessId -ErrorAction SilentlyContinue
    if (-not $process) { $faults += 'the game ended by itself' }
    else {
        if ($process.MainWindowTitle -ne 'Dragon Age: Origins') { $faults += "its window is '$($process.MainWindowTitle)'" }
        elseif (-not $Row.Answered) { $faults += "its window does not answer by $($Seconds + $Answer) s" }
        $loaded = @(Get-GameModule $Row.Run.ProcessId | Where-Object { (Split-Path $_ -Leaf) -ieq $loaderName })
        if (-not ($loaded | Where-Object { $_.StartsWith($Row.Folder, [StringComparison]::OrdinalIgnoreCase) })) {
            $faults += "$loaderName is the system's: the loader is not in the game"
        }
    }
    $log = Get-ChildItem $Row.Logs -Filter 'loader*.log' -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -GE $Row.Began | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $log) { $faults += 'no loader.log written' }
    else {
        $lines = Get-Content $log.FullName
        if (-not ($lines -match 'entry point reached|the game''s code is running')) { $faults += "the game's start was not reached" }
        if (-not ($lines -match "the engine's memory is the system's heap")) { $faults += 'the engine heap is not in' }
        if ($Row.Instance -and -not ($lines -match "instance `"$([regex]::Escape($Row.Instance))`"")) { $faults += 'not its own instance' }
        foreach ($plugin in $Row.Plugins) {
            if (-not ($lines -match "\] $([regex]::Escape($plugin))\.dll: loaded")) { $faults += "$plugin.dll not loaded" }
        }
    }
    $written = @(Get-ChildItem $Row.Logs -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^(crash|hang|exit)-' -and $_.LastWriteTime -ge $Row.Began })
    if ($written) { $faults += "wrote $($written.Name -join ', ')" }
    return $faults
}

# Each row's window read until it is the game's and answers, up to -Answer seconds more: the
# seconds from its start when it first did, in the row's Answered, or none.
function Wait-Answer([object[]] $Rows) {
    $until = (Get-Date).AddSeconds($Answer)
    do {
        foreach ($row in $Rows | Where-Object { $_.Run -and -not $_.Answered }) {
            $process = Get-Process -Id $row.Run.ProcessId -ErrorAction SilentlyContinue
            if ($process -and $process.MainWindowTitle -eq 'Dragon Age: Origins' -and $process.Responding) {
                $row.Answered = ((Get-Date) - $row.Run.CreationDate).TotalSeconds
            }
        }
        $left = @($Rows | Where-Object { $_.Run -and -not $_.Answered })
        if ($left.Count -gt 0) { Start-Sleep -Seconds 1 }
    } while ($left.Count -gt 0 -and (Get-Date) -lt $until)
}

# Rows started together, a few seconds apart since a start stalls the others; each is read once
# it has been up -Seconds, then all are ended. The count of rows that failed.
function Invoke-Batch([object[]] $Rows) {
    $failed = 0
    try {
        foreach ($row in $Rows) { Open-Row $row; Start-Sleep -Seconds 3 }
        $until = (Get-Date).AddSeconds(150)
        while ((Get-Date) -lt $until -and @($Rows | Where-Object { -not $_.Run }).Count -gt 0) {
            Start-Sleep -Seconds 2
            foreach ($row in $Rows | Where-Object { -not $_.Run }) { $row.Run = Find-Run $row }
        }
        $last = @($Rows | Where-Object Run | ForEach-Object { $_.Run.CreationDate } | Sort-Object)[-1]
        if ($last) {
            $wait = $Seconds - ((Get-Date) - $last).TotalSeconds
            if ($wait -gt 0) { Start-Sleep -Seconds $wait }
        }
        Wait-Answer $Rows
        Start-Sleep -Milliseconds 600 # the logs' writer thread (dao::kLogLatency)
        foreach ($row in $Rows) {
            $faults = @(Test-Row $row)
            $when = if ($row.Answered) { " (answering at $([int]$row.Answered) s)" } else { '' }
            if ($faults) { $failed++; Show-Bad "$($row.Name)$($when): $($faults -join '; ')" } else { Show-Ok "$($row.Name)$when" }
        }
    }
    finally {
        foreach ($row in $Rows) { Close-Row $row }
    }
    return $failed
}

# EA's rows, in its install, one at a time, as the game as ever.
function Invoke-Ea {
    $root = $installs.ea
    $bin = Join-Path $root 'bin_ship'
    if (-not (Test-Path (Join-Path $bin $loaderName))) { throw "No $loaderName in $bin; deploy first (.\tools\deploy.ps1 -Install ea)" }
    $plugins = @(Get-ChildItem (Join-Path $bin 'daolib\plugins') -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName "$($_.Name).dll") } | ForEach-Object Name)
    if ($plugins.Count -eq 0) { throw "No plugin in $bin\daolib\plugins; a start with none shows less" }
    if (Test-GameAsEver) { throw 'The game as ever is running (its mutex "dragonage" is held); EA''s rows run as it, one at a time' }
    $names = (Get-ItemProperty $pathKey).Path
    $before = $null
    $failed = 0
    try {
        if (-not [string]::Equals($names.TrimEnd('\'), $root, 'OrdinalIgnoreCase')) {
            if (-not $SwitchPath) { throw "The EA app would start the game in $names (the Path of $pathKey); -SwitchPath sets it to $root for the runs" }
            $before = $names
            Set-ItemProperty $pathKey -Name Path -Value $root
            Show-Info "Path of $pathKey is $root for the runs, $names after"
        }
        Show-Head "EA's $bin\DAOrigins.exe, $($plugins.Count) plugin(s), $($Layers.Count) settings, one at a time"
        foreach ($layer in $Layers) {
            $row = [pscustomobject]@{
                Name = "ea, $(if ($layer) { $layer } else { 'no layer' })"; Exe = (Get-Item (Join-Path $bin 'DAOrigins.exe')).FullName
                Folder = $bin; Layer = $layer; Instance = $null; Steam = $false; Plugins = $plugins; Run = $null; Began = $null; Answered = $null
                Logs = Join-Path $documents 'BioWare\Dragon Age\daolib'
            }
            $failed += Invoke-Batch @($row)
            # Each row leaves a game of EA's running, which the next row's start met: that row ended
            # before the loader ran in 2 of 4 runs with no settle here, and in none of 6 with it (#260).
            Close-EaGame $bin
        }
    }
    finally {
        Close-EaGame $bin
        if ($before) { Set-ItemProperty $pathKey -Name Path -Value $before; Show-Info "Path of $pathKey is $before again" }
    }
    return $failed
}

# The EA app starts EA's game, and once started one during a row after that row's games were
# stopped, which ran on past the rows and kept a deploy.ps1 -Remove from the install (2026-10-23).
# No game of EA's runs before its rows (Test-GameAsEver), so every one from its folder after them
# is the matrix's: they are stopped until none has been seen for five seconds, a minute at most.
function Close-EaGame([string] $Bin) {
    $prefix = $Bin.TrimEnd('\') + '\'
    $quiet = (Get-Date).AddSeconds(5)
    $until = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $quiet -and (Get-Date) -lt $until) {
        $runs = @(Get-Process -Name DAOrigins -ErrorAction SilentlyContinue |
                Where-Object { $_.Path -and $_.Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) })
        foreach ($run in $runs) {
            Show-Info "EA's game still ran after its row (process $($run.Id)); stopped"
            Stop-Process -Id $run.Id -Force -ErrorAction SilentlyContinue
            $run.WaitForExit(15000) | Out-Null
            $quiet = (Get-Date).AddSeconds(5)
        }
        Start-Sleep -Milliseconds 500
    }
}

if ($Firewall) {
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw '-Firewall adds rules to Windows Firewall: run it from an elevated PowerShell'
    }
    $group = 'DAOLib play matrix'
    Get-NetFirewallRule -Group $group -ErrorAction SilentlyContinue | Remove-NetFirewallRule
    $made = 0
    foreach ($key in $kept.Keys) {
        for ($i = 0; $i -lt $whole.Count; $i++) {
            $name = Get-RowName $key $i
            $exe = Join-Path $stage "$name\bin_ship\DAOrigins.exe"
            New-NetFirewallRule -DisplayName "$group`: $name" -Group $group -Direction Inbound -Program $exe -Action Block | Out-Null
            $made++
        }
    }
    Show-Ok "$made inbound block rules in the firewall's group '$group', one for each row's exe"
    return
}

if (-not (Test-Path (Join-Path $build $loaderName))) { throw "No $build\$loaderName; run .\tools\build.ps1 -Preset $Preset first" }
if (($Exes | Where-Object { $kept[$_].Steam }) -and -not (Get-Process steam -ErrorAction SilentlyContinue)) {
    throw "Steam is not running; its stock exe's stub needs it"
}

$failed = 0
$total = 0
try {
    Clear-Stage
    $base = Build-Base
    if ($base.Plugins.Count -eq 0) { throw "No plugin in $build\daolib\plugins; a start with none shows less" }
    $rows = @(foreach ($key in $Exes) { for ($i = 0; $i -lt $Layers.Count; $i++) { Build-Row -Base $base -Key $key -Index $i -Layer $Layers[$i] } })
    $total = $rows.Count
    Show-Head "$($rows.Count) rows ($($Exes.Count) exes, $($Layers.Count) settings), $($base.Plugins.Count) plugin(s) of build\$Preset, $Batch at a time, $Seconds s each"
    for ($i = 0; $i -lt $rows.Count; $i += $Batch) {
        $failed += Invoke-Batch $rows[$i..([Math]::Min($i + $Batch, $rows.Count) - 1)]
    }
}
finally {
    Clear-Stage -Lenient
}
if ($EaApp) {
    $total += $Layers.Count
    $failed += Invoke-Ea
}
if ($failed) { throw "$failed of $total rows failed" }
Show-Ok "all $total rows"
