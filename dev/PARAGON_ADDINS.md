# What Paragon needs for add-ins (paragon-agent-2, 2026-10-20)

The user wants the add-ins' content (the DLC campaigns, the story add-ins that extend Single Player, the promo items) to work in Paragon. The archives of most add-ins can't be read by core, and core keeps refusing them (the user's decision, 2026-10-17). Nothing about reading those archives goes into DAOLib or Paragon. What follows needs no such thing: core reads add-in content that the player already has as plain files in a local folder, the way it reads `override`.

## The folder

A local folder, given by the program (Paragon: an environment variable or its settings; never a path in the repository), holding plain files laid out as the install lays out the add-ins' archives:

```
<root>/<addin uid in lower case>/<the archive's folder, relative to the add-in's folder>/<the archive's stem>/<file>
e.g. <root>/dao_prc_cp_1/core/data/2da/abi_base_kcc.gda     (if the archive's stem folder level is there, see below)
```

What each level means should follow the install: an add-in folder under `Addins\` (`dao_prc_cp_1`), then `core\...` or `module\...` as in the add-in, then each archive's contents. One open question for you: whether the archive's stem should be a folder level (`.../data/designerscripts/<file>`) or the files lie beside each other as a loose `override` would. The local folder Paragon has today keeps the stem as a folder.

## What core would do

1. **An extra source per add-in in `Index::build`:** for each add-in in the player's `AddIns.xml` that is enabled (core's `ReadAddIns`), if the folder has `<root>/<uid>/`, read its files as that add-in's copies, at the add-in's place in the order (`AddInOrder.h`: its priority among sources, its `core` versus `module` side, the campaign it extends), exactly where the unreadable archive's copies would have been. An add-in whose archives core can read already needs nothing new.
2. **Answers unchanged otherwise:** a resource read by name is the copy the game takes, so a table merged from every `*_<suffix>.gda` (an M2DA) gets the add-ins' rows, a template or script in an add-in shadows core's as the game does, and talk tables, sound banks and movies come from the add-in's own folder.
3. **Who has what:** a call that answers which add-ins are installed, enabled, and whose content core holds (from the folder or readable archives), for a campaign list. (`IsPackageLoaded`, command 862, is not this: the exe answers it from the running campaign's `.cif` package list, script_commands/item_prop.md.)

## What Paragon will do with it

- Pass the folder to `Index::build` through the shim when the player has set one.
- Answer `IsPackageLoaded` from the running campaign's `.cif` packages (core's `Campaign::packages`), case-insensitive, instead of always 0 (command 862; script_commands/item_prop.md).
- Later: the add-in campaigns as modules (Paragon now runs only Single Player).

## Open

- The levels of the folder (above).
- Whether any add-in content is in a form core doesn't read yet (a `.cif` or other container), and talk tables, `.fsb`/`.fev` sound banks and `.bik` movies inside add-ins.
- How a program learns the folder: a fixed place (beside Paragon's saves, `%LOCALAPPDATA%\Paragon\addins`) or a setting.
