# The Dragon Age toolset, as a source

The toolset installer (`toolset\dragon-age-toolset-1.01-setup.zip`, GOG's, ignored by git) is an NSIS package that 7-Zip lists and extracts without installing. What it holds that matters here, looked at 2026-09-29:

- **`tools\Engine.dll`**: a separate build of the Eclipse engine, linked 2009-11-20, 32-bit, that the editor embeds for its previews. It exports only 160 names, mostly Scaleform's GFx, so it is not a symbol table. But it keeps about 300 `Class::method` strings (`AnimBase::LoadModel`, `AnimationManager::FindModelAnimationTree`) that the game exe does not. Matching its functions to the game's (Ghidra's Version Tracking or BSim) could carry those names across. A lead, not yet tried.
- **`tools\dbbak\bw_dragonage_content.bak`**: the toolset's content database, a SQL Server backup of 704 MB (extracted to `toolset\extract\tools\dbbak\` on 2026-09-30). It holds the source of the game's own scripts, including the combat AI and the rules scripts, which a turn-based mode will need to read. Its boot page gives the database format as 611, SQL Server 2005 (the toolset bundles SQL Server 2005 Express SP3, `tools\sqlexpr\`), and current SQL Server versions do not restore backups that old directly, so a restore needs an older SQL Server in between. But it needs no restoring: `tools/sqlbak.py` reads SQL Server 2005's page format straight from the backup (the pages lie in order after the tape header, page n at a fixed offset), and `tools/toolset_scripts.py` extracts the scripts with it, in about two seconds. Two tables hold them: object 242, (id, text), the 4,209 script texts as large values; and object 238, the toolset's resources, one row per version with the content's id, the name, the folder, the owner and a state, `C` for current and `H` for history. Joined on the id, the current rows give **2,101 scripts, 12.2 MB of source, with their names and folders**: the core (`_Core Scripts`, `_Core Includes`: `rules_core`, `ai_main_h_2`, `combat_h`, `core_h`, `events_h`), abilities, followers, systems, and a folder per area and origin. They go to `toolset\extract\scripts\` (ignored by git, being BioWare's), with an index, `scripts.csv`.
- **`tools\scripttemplates\`**: event-handler templates, small.

What turned out to be in the game already, so the toolset is not needed for it:

- **`script.ldf`** and **`cscript.ldf`**, in `packages\core\data\scripts.erf`: the declaration of every engine function with its command number (`dev/SCRIPT_COMMANDS.md`).
- **`packages\core\dascript.chm`**: the script reference, one page per function, with the same doc comments `script.ldf` carries.

The community wiki, <https://www.datoolset.net/wiki/Main_Page>, covers the toolset and scripting for content makers. Its front page lists getting started, tutorials, design, art, scripting and technical information, and nothing on engine internals. Use it to learn what a script function or event means, not to learn how the engine works.

To extract a file from the installer:

```powershell
7z e -otoolset\extract\tools toolset\extract\DragonAgeToolset1.01Setup.exe 'tools\Engine.dll'
```

(`toolset\extract\DragonAgeToolset1.01Setup.exe` is unzipped from the archive first.)
