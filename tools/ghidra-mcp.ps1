<#
.SYNOPSIS
    Analyse DAOrigins.exe in Ghidra once (-Import), then serve that analysis over
    MCP for the project's .mcp.json, or ask it questions from the shell (-Query).

.DESCRIPTION
    The executable is game\DAOrigins.exe, a copy of the GOG Ultimate Edition's
    bin_ship\DAOrigins.exe (1.05.13263.0; tools\daoexe.py --check says whether the
    copy is the one every address in src\dao\Addresses.h was read from). The Ghidra
    project is game\ghidra\dao.gpr, beside it and ignored by git like it.

    -Import creates the project and runs Ghidra's full auto-analysis on the exe,
    once. About six minutes; the log is game\ghidra\dao.import.log. Refuses if the
    project already exists: delete game\ghidra\dao.gpr and dao.rep to redo it.
    tools\ghidra\set_compiler.py runs first and marks the program as Visual
    Studio's, which the PE loader left "unknown"; after the analysis come Ghidra's
    RTTI pass and its class recovery, as -Recover runs them.

    -Recover runs Ghidra's RTTI pass (RunRttiAnalyzerScript.java) and its class
    recovery (RecoverClassesFromRTTIScript.java) over the analysed project and
    saves the result: vftables, class namespaces, constructors and destructors,
    and a structure per class. In Ghidra 12 these are scripts, not analyzers, and
    both refuse a program not marked as Visual Studio's in PeLoader's exact words,
    so set_compiler.py runs first here too. The log is game\ghidra\dao.recover.log.
    Class names are also applied on every launch from tools\daoexe.py's own walk
    of the RTTI, which needs none of this.

    -Query runs tools\ghidra\query.py over the project and prints its answers:
    "vtable <class>", "string <text>", "refs <0xaddr>", "decompile <0xaddr>",
    "symbols <text>", "analyzers [text]", "info". For a shell or a script; a
    Claude Code session uses the MCP server instead. The two cannot run at once:
    a project is locked while open.

    Without either, opens the analysed project headless without analysing again,
    names its functions and globals from tools\names.py (Addresses.h, the message
    handler and script command tables, every RTTI vftable; through
    tools\ghidra\ImportNames.java, and a name set by hand in Ghidra is kept), and
    runs GhidrAssistMCP's server on the program until -Stop.

    -Stop asks the server to close through its completion file, so Ghidra saves what
    was renamed or retyped over MCP and exits. Ctrl+C or closing the window loses
    that; the imported names come back on the next launch either way.

    Port 8090, so it can run beside the Skyrim projects' servers on 8080-8082.

.EXAMPLE
    .\tools\ghidra-mcp.ps1 -Import                                   # once: analyse the exe
    .\tools\ghidra-mcp.ps1 -Recover                                  # RTTI and class recovery on an existing analysis
    .\tools\ghidra-mcp.ps1                                           # serve it on http://127.0.0.1:8090/mcp
    .\tools\ghidra-mcp.ps1 -Stop                                     # save and close it
    .\tools\ghidra-mcp.ps1 -Query 'vtable GameModeController' 'decompile 0x006403C0'
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [switch] $Import,
    [switch] $Recover,
    [switch] $Stop,
    [switch] $Query,
    [Parameter(ValueFromRemainingArguments)] [string[]] $Questions,
    [string] $Ghidra = 'C:\Modding\ghidra_12.1.4_PUBLIC',
    [string] $MaxMemory = '8G',
    [int] $Port = 8090
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'console.ps1')
$repo = Split-Path -Parent $PSScriptRoot

$projects = Join-Path $repo 'game\ghidra'
$name = 'dao'
$exe = Join-Path $repo 'game\DAOrigins.exe'
$project = Join-Path $projects "$name.gpr"
$complete = Join-Path $projects "$name.mcp.complete"
$ourScripts = Join-Path $PSScriptRoot 'ghidra'

if ($Stop) {
    New-Item -ItemType File -Force $complete | Out-Null
    Show-Ok 'asked the server to save and close'
    return
}

if (-not (Test-Path $exe)) { throw "No executable at $exe; copy the game's bin_ship\DAOrigins.exe there" }
New-Item -ItemType Directory -Force $projects | Out-Null

# PyGhidra's launcher in headless mode: the same analyzer with Python 3 attached,
# which set_compiler.py, query.py and GhidrAssistMCP's eval_python need. It takes
# Java options from PYGHIDRA_JAVA_OPTIONS, not GHIDRA_HEADLESS_MAXMEM.
$headless = Join-Path $Ghidra 'support\pyghidraRun.bat'
if (-not (Test-Path $headless)) { throw "No Ghidra at $Ghidra" }
$env:PYGHIDRA_JAVA_OPTIONS = "-Xmx$MaxMemory"

if ($Import) {
    if (Test-Path $project) { throw "$project exists; delete it and $name.rep to analyse again" }
    Show-Head "Ghidra import: $exe -> $project"
    Show-Info "log: $projects\$name.import.log"
    & $headless -H $projects $name -import $exe -log (Join-Path $projects "$name.import.log") `
        -scriptPath $ourScripts `
        -preScript set_compiler.py `
        -postScript RunRttiAnalyzerScript.java `
        -postScript RecoverClassesFromRTTIScript.java
    if ($LASTEXITCODE -ne 0) { throw "analyzeHeadless failed ($LASTEXITCODE)" }
    Show-Ok 'analysed; serve it with .\tools\ghidra-mcp.ps1'
    return
}

if ($Recover) {
    if (-not (Test-Path $project)) { throw "No analysed project at $project; run .\tools\ghidra-mcp.ps1 -Import first" }
    if (Test-Path (Join-Path $projects "$name.lock")) { throw "$name.lock is there: close the Ghidra that has the project open first" }
    Show-Head "Ghidra RTTI and class recovery over $project"
    Show-Info "log: $projects\$name.recover.log"
    & $headless -H $projects $name -process 'DAOrigins.exe' -noanalysis -log (Join-Path $projects "$name.recover.log") `
        -scriptPath $ourScripts `
        -preScript set_compiler.py `
        -postScript RunRttiAnalyzerScript.java `
        -postScript RecoverClassesFromRTTIScript.java
    if ($LASTEXITCODE -ne 0) { throw "analyzeHeadless failed ($LASTEXITCODE)" }
    Show-Ok 'recovered; serve it with .\tools\ghidra-mcp.ps1'
    return
}

if (-not (Test-Path $project)) { throw "No analysed project at $project; run .\tools\ghidra-mcp.ps1 -Import first" }
if (Test-Path (Join-Path $projects "$name.lock")) {
    Show-Warn "$name.lock is there: another Ghidra may have the project open"
}

$names = Join-Path $projects "$name.names.tsv"
& python (Join-Path $PSScriptRoot 'names.py') --tsv | Set-Content -Encoding utf8 $names
if ($LASTEXITCODE -ne 0) { throw 'tools/names.py --tsv failed' }

if ($Query) {
    if (-not $Questions) { throw 'give -Query one or more questions: "vtable <class>", "string <text>", "refs <0xaddr>", "decompile <0xaddr>", "symbols <text>", "info"' }
    # -readOnly: nothing a question does is worth saving, and it leaves no lock.
    # Ghidra's own chatter goes to its log; the answers go to a file the script
    # is given, printed here, so nothing of Ghidra's is mixed in.
    $answers = Join-Path $projects "$name.query.txt"
    Remove-Item -Force $answers -ErrorAction SilentlyContinue
    & $headless -H $projects $name -process 'DAOrigins.exe' -noanalysis -readOnly `
        -scriptPath $ourScripts `
        -log (Join-Path $projects "$name.query.log") `
        -preScript ImportNames.java $names `
        -postScript query.py $answers @Questions *> (Join-Path $projects "$name.query.out")
    if (-not (Test-Path $answers)) { throw "query.py wrote no answers; see $projects\$name.query.out" }
    Get-Content $answers
    return
}

# Where Ghidra extracts an installed extension: the user's settings for this release.
$mcpScripts = Join-Path ([Environment]::GetFolderPath('ApplicationData')) `
    "ghidra\$(Split-Path -Leaf $Ghidra)\Extensions\GhidrAssistMCP\ghidra_scripts"
if (-not (Test-Path (Join-Path $mcpScripts 'GAMCPStartServerScript.java'))) {
    throw "GhidrAssistMCP is not installed for this Ghidra (looked in $mcpScripts)"
}
Remove-Item -Force $complete -ErrorAction SilentlyContinue

# agentic=false below (our branch of alandtse's fork, C:\Modding\GhidrAssistMCP,
# agentic-switch) lists every tool rather than the fork's core set: Claude Code
# loads a tool's definition only when it is used, so the long list costs little.
Show-Head "Ghidra MCP: DAOrigins.exe on http://127.0.0.1:$Port/mcp"
Show-Info 'stop it with: .\tools\ghidra-mcp.ps1 -Stop'
# Only our folder is named: an installed extension's scripts are on Ghidra's own
# path, and cmd, which runs the .bat launcher, would split a `;` list apart.
& $headless -H $projects $name -process 'DAOrigins.exe' -noanalysis `
    -scriptPath $ourScripts `
    -preScript ImportNames.java $names `
    -postScript GAMCPStartServerScript.java 'host=127.0.0.1' "port=$Port" 'wait=true' "completion_file=$complete" `
    'agentic=false'
