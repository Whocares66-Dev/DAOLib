<#
.SYNOPSIS
    Start one install's game under Windows compatibility settings in turn and check that the
    loader did its whole start under each.

.DESCRIPTION
    dev/TESTING.md says when this is run and why. The loader and at least one plugin must be in
    the game already (.\tools\deploy.ps1 -Install <install>): nothing is copied here.

    THIS STARTS THE GAME, once per setting, half a minute each, and ends it. Say so first, and
    run it with no game as ever running (an instance beside it is fine: this refuses while the
    game's own mutex is held).

    Without -All the settings are the two a merge to master is seen under: none, and Disable
    fullscreen optimizations, the one that brings the shim DLL whose imports took the system's
    version.dll ahead of the loader (DAOLib #93). -All is the whole matrix, for a release:
    those, the three high DPI overrides, and the compatibility modes from Windows XP SP2 to
    Windows 8. -Layers names others.

    For each setting the exe is given the layer as the Compatibility tab gives it (its path
    under HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers), the game is
    started the way the install starts, left up -Seconds, read, and ended; the layer is taken
    out again. A row passes when all of these hold:

      - the DLL of the loader's name in the process is the one beside the exe, not the system's
      - loader.log, written by this run, says the game's start was reached (the entry point in
        GOG's build, the CRT's start in a wrapped one)
      - the engine heap went in
      - every plugin folder in the game has its "<name>.dll: loaded" line
      - no crash, hang or exit log was written
      - the process is still there at the end, its window the game's own and answering

    How each install starts. GOG's exe starts the game. Steam's hands off to Steam and its
    launcher's window unless SteamAppId and SteamGameId are in its environment, which this sets;
    Steam must be running. EA's hands off to the EA app, which starts the game under the folder
    HKLM\SOFTWARE\WOW6432Node\BioWare\Dragon Age's Path names: where that is another install's
    (Steam's installer writes it), -SwitchPath sets it to EA's for the runs and puts it back,
    from an elevated shell.

    Not in the matrix, and why (dev/TESTING.md): 8-bit colour, 16-bit colour and 640 x 480,
    which change the desktop's own display mode while the game is up; Run as administrator,
    which asks at a prompt.

.EXAMPLE
    .\tools\play_matrix.ps1 -Install steam                   # the two settings of a merge
    .\tools\play_matrix.ps1 -Install ea -SwitchPath
    .\tools\play_matrix.ps1 -Game game\gogrun\bin_ship        # a copy of GOG's bin_ship
    .\tools\play_matrix.ps1 -Install steam -All              # the whole matrix, for a release
    .\tools\play_matrix.ps1 -Install steam -Layers WIN7RTM
#>
[CmdletBinding()]
param(
    [ValidateSet('gog', 'ea', 'steam')]
    [string] $Install = 'gog',
    [string] $Game,
    [switch] $All,
    [string[]] $Layers,
    [int] $Seconds = 30,
    [switch] $SwitchPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')

$loaderName = 'dinput8.dll'
# The layers' names as the Compatibility tab writes them; '' is no setting at all.
$merge = @('', 'DISABLEDXMAXIMIZEDWINDOWEDMODE')
$whole = $merge + @('HIGHDPIAWARE', 'DPIUNAWARE', 'GDIDPISCALING DPIUNAWARE', 'WINXPSP2', 'WINXPSP3', 'VISTARTM',
    'VISTASP1', 'VISTASP2', 'WIN7RTM', 'WIN8RTM')
if (-not $PSBoundParameters.ContainsKey('Layers')) { $Layers = if ($All) { $whole } else { $merge } }
$steamAppId = '47810'
$layersKey = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers'
$pathKey = 'HKLM:\SOFTWARE\WOW6432Node\BioWare\Dragon Age'

if (-not $Game) {
    $Game = @{
        gog   = 'C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins\bin_ship'
        ea    = 'C:\Program Files\EA Games\Dragon Age\bin_ship'
        steam = 'E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition\bin_ship'
    }[$Install]
}
$Game = (Resolve-Path $Game).Path
$exe = (Get-Item (Join-Path $Game 'DAOrigins.exe')).FullName
if (-not (Test-Path (Join-Path $Game $loaderName))) { throw "No $loaderName in $Game; deploy first (.\tools\deploy.ps1)" }
$plugins = @(Get-ChildItem (Join-Path $Game 'daolib\plugins') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "$($_.Name).dll") } | ForEach-Object Name)
if ($plugins.Count -eq 0) { throw "No plugin in $Game\daolib\plugins; a start with none shows less" }
$logs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'BioWare\Dragon Age\daolib'

function Get-Run {
    @(Get-CimInstance Win32_Process -Filter "Name='DAOrigins.exe'" |
            Where-Object { $_.ExecutablePath -and [string]::Equals($_.ExecutablePath, $exe, 'OrdinalIgnoreCase') })
}
function Close-Run {
    foreach ($left in Get-Run) { Stop-Process -Id $left.ProcessId -Force -ErrorAction SilentlyContinue }
    $until = (Get-Date).AddSeconds(15)
    while ((Get-Run) -and (Get-Date) -lt $until) { Start-Sleep -Milliseconds 300 }
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

# One start under one layer: the faults found, none if the row passes.
function Test-Start {
    $faults = @()
    $began = Get-Date
    Start-Process -FilePath $exe -WorkingDirectory $Game
    # The run that stays: Steam's and EA's first may be a hand-off that exits.
    $run = $null
    while (-not $run -and (Get-Date) -lt $began.AddSeconds(150)) {
        Start-Sleep -Seconds 2
        $run = Get-Run | Where-Object { ((Get-Date) - $_.CreationDate).TotalSeconds -ge 12 } | Select-Object -First 1
    }
    if (-not $run) { return @('no game stayed up') }
    $wait = $Seconds - ((Get-Date) - $run.CreationDate).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Seconds $wait }

    $process = Get-Process -Id $run.ProcessId -ErrorAction SilentlyContinue
    if (-not $process) { $faults += 'the game ended by itself' }
    else {
        if ($process.MainWindowTitle -ne 'Dragon Age: Origins') { $faults += "its window is '$($process.MainWindowTitle)'" }
        elseif (-not $process.Responding) { $faults += 'its window does not answer' }
        $loaded = @(Get-GameModule $run.ProcessId | Where-Object { (Split-Path $_ -Leaf) -ieq $loaderName })
        if (-not ($loaded | Where-Object { $_.StartsWith($Game, [StringComparison]::OrdinalIgnoreCase) })) {
            $faults += "$loaderName is the system's: the loader is not in the game"
        }
    }
    Start-Sleep -Milliseconds 600 # the log's writer thread (dao::kLogLatency)
    $log = Get-ChildItem $logs -Filter 'loader*.log' -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -GE $began | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $log) { $faults += 'no loader.log written' }
    else {
        $lines = Get-Content $log.FullName
        if (-not ($lines -match 'entry point reached|the game''s code is running')) { $faults += "the game's start was not reached" }
        if (-not ($lines -match "the engine's memory is the system's heap")) { $faults += 'the engine heap is not in' }
        foreach ($plugin in $plugins) {
            if (-not ($lines -match "\] $([regex]::Escape($plugin))\.dll: loaded")) { $faults += "$plugin.dll not loaded" }
        }
    }
    $written = @(Get-ChildItem $logs -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^(crash|hang|exit)-' -and $_.LastWriteTime -ge $began })
    if ($written) { $faults += "wrote $($written.Name -join ', ')" }
    return $faults
}

if (Get-Run) { throw "A game from $exe is running already; close it first" }
if (Test-GameAsEver) { throw 'The game as ever is running (its mutex "dragonage" is held); one runs at a time' }
if ($Install -eq 'steam' -and -not (Get-Process steam -ErrorAction SilentlyContinue)) { throw 'Steam is not running; its stub needs it' }

$pathBefore = $null
$failed = 0
try {
    if ($Install -eq 'steam') { $env:SteamAppId = $steamAppId; $env:SteamGameId = $steamAppId }
    if ($Install -eq 'ea') {
        $root = Split-Path $Game -Parent
        $names = (Get-ItemProperty $pathKey).Path
        if (-not [string]::Equals($names.TrimEnd('\'), $root, 'OrdinalIgnoreCase')) {
            if (-not $SwitchPath) { throw "The EA app would start the game in $names (the Path of $pathKey); -SwitchPath sets it to $root for the runs" }
            $pathBefore = $names
            Set-ItemProperty $pathKey -Name Path -Value $root
            Show-Info "Path of $pathKey is $root for the runs, $names after"
        }
    }
    Show-Head "$exe, $($plugins.Count) plugin(s), $($Layers.Count) settings, $Seconds s each"
    foreach ($layer in $Layers) {
        $shown = if ($layer) { $layer } else { 'no layer' }
        try {
            if ($layer) { New-ItemProperty -Path $layersKey -Name $exe -Value "~ $layer" -PropertyType String -Force | Out-Null }
            $faults = @(Test-Start)
        }
        finally {
            Close-Run
            if ($layer) { Remove-ItemProperty -Path $layersKey -Name $exe -ErrorAction SilentlyContinue }
        }
        if ($faults) { $failed++; Show-Bad "${shown}: $($faults -join '; ')" } else { Show-Ok $shown }
    }
}
finally {
    if ($pathBefore) { Set-ItemProperty $pathKey -Name Path -Value $pathBefore; Show-Info "Path of $pathKey is $pathBefore again" }
}
if ($failed) { throw "$failed of $($Layers.Count) settings failed" }
Show-Ok "all $($Layers.Count) settings"
