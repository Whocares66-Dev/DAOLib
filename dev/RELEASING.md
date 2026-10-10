# Releasing

A release is a tag, `v<version>`, and on GitHub the zip a player installs: `DAOLib-<version>.zip`, the loader, its PDB and the address files laid out as the game's folder (`tools\package.ps1`). A plugin's repository holds its DAOLib submodule at the tag, and the documentation site builds that version's pages from it. The process is SkyrimFollowerTactics' (`C:\project\SkyrimFollowerTactics\dev\RELEASING.md`) and DAOAddressLibrary's. Written 2026-10-10, for 0.1.0 (DAOLib #55).

## The version

`project(DAOLib VERSION x.y.z)` in `CMakeLists.txt` is the one place it is written; the loader's log and the zip's name take it from there. What a number promises a player's plugins is not the version's to say but `kPluginApiVersion`'s (`src/dao/Plugin.h`): a plugin works with any loader of the same API version, since each plugin carries its own copy of the library. So:

- **patch**: fixes, and more of the library, with the same loader contract.
- **minor**: something a player or a plugin's author would call new: a part of the library, a kind of log, another build of the game.
- **major**: `kPluginApiVersion` changes, so plugins built before it are refused and must be rebuilt, or the address library's `format` word does (below).

## The address library

A release holds `extern\DAOAddressLibrary` at one of that repository's own release tags, and the zip's files are that release's byte for byte. `package.ps1` writes which into the zip's `README.txt`, the changelog's section names it, and `release.ps1` refuses when the submodule is at any other commit. `dev/BUILDS.md`, "The files", has how the two zips fit for a player. A new `format` word there goes out together with a DAOLib that reads it, and neither before the other.

## What the zip holds, and what it must not

- `bin_ship\version.dll` and `bin_ship\version.pdb`, the release build's. The PDB is 7 MB (the zip is 3 MB with it, under 1 MB without): it is what lets a player's crash log name DAOLib's frames with file and line, which is the reason a crash log is sent at all. A release build makes one for the loader and each plugin (`dao_release_symbols` in `cmake\DAOLib.cmake`); the code is as optimised as it was without.
- `bin_ship\daolib\addresses\*.bin`, one per build of the game.
- `README.txt`.
- **Nothing that says who built it.** A PDB holds the paths of its sources (`C:\project\DAOLib`, `C:\vcpkg`, Visual Studio's), and the linker adds the folder of its temporary files, which is under the user's name. `tools\build.ps1` gives a build a temporary folder in its own tree, and `package.ps1` refuses a loader or PDB that holds the path of the user's folder. A build made any other way (the IDE's, `cmake --build` by hand) is not one to package.
- No plugin, and not `functions.tsv` (Ghidra's names for the game's own functions, which `deploy.ps1` copies for our own crash logs).

## Cutting one

1. On a work branch: set the version if it changes, and add `## x.y.z` at the top of `CHANGELOG.md`: about five entries, the most important first, in a player's and a plugin author's words. The section is the GitHub release's notes, word for word, so no link in it is relative. Move the submodule to the address library's tag if it is not at one.
2. Run the three commands of "Before every commit" (`CLAUDE.md`), then `.\tools\build.ps1 -Preset release -Test` and `.\tools\package.ps1`, and read the zip: its files and its `README.txt`.
3. See the release build in the game: the zip's `version.dll` and `version.pdb` with at least one plugin of the same build, a start to the main menu and a load of a save. `loader.log` says the version, "address library: N IDs for this build" and each plugin "loaded". Play tests are otherwise of debug builds, so this is the one time the optimised loader is run before a player runs it.
4. Publishing waits for the user's word: a request to prepare a release is not one to publish it.
5. Merge to `master`, then `.\tools\release.ps1 none -DryRun` and `.\tools\release.ps1 none` (`-Jobs 4` while someone plays). It builds the release preset and runs its tests, writes the zip, makes the annotated tag, pushes `master` and the tag, and creates the release with the zip attached, a prerelease below 1.0. `patch`, `minor` or `major` in place of `none` bumps and commits the version first.
6. Check the release's tag, notes, prerelease flag and one zip, and the zip's hash against `build\package\`. Check that the tag's run of the Documentation workflow passed and the site's version selector has the version. A tag is never moved: a correction is the next patch.

If the build, the tests or the zip fail, nothing was tagged or pushed. If `gh release create` fails after the push, run it alone on the tag that is there: `gh release create vX.Y.Z --repo Whocares66-Dev/DAOLib --verify-tag --title "DAOLib X.Y.Z" --notes-file build\release-notes-X.Y.Z.md build\package\DAOLib-X.Y.Z.zip` (`--prerelease` below 1.0).
