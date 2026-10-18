# What Paragon needs for character creation's looks

Paragon (paragon-agent, 2026-10-18) now draws the hero from the script world after character creation: core's `character::Resolve` over a `Template` filled from the hero's appearance and gender, with the head the appearance scene resets to (no morph; face, eyes and hair rows 1; dev/CHARGEN.md). Clothes come once items are objects of Paragon's script world. Two readings are missing for the hero to look as the game draws it.

## 1. The head presets (`.mop`) and a morph from one

`chargenmorphcfg.xml` (misc.erf) lists per race and gender the presets the appearance scene offers: `HM_CPS_P01.mop` to `P08` and `HM_PCC_B01.mop` (84 `.mop` files in face.erf and chargen.rim). There are also `.mor` files `hm_pcc_b01` to `b04` (b01 3600 bytes, the others ~50 KB). Paragon needs:

- A reader of a `.mop` (core, like Morph.h): what it holds. Is it slider values over a base `.mor`, or a whole morph?
- How the engine makes the hero's head from a preset and the sliders. Is the result a morph Paragon can draw like any `.mor` (core's `Morph`: parts, vertices, bones, tints)? Or does it take the preset's base `.mor` plus per-slider targets? Which file is that base?
- The rest of `chargenmorphcfg.xml` as data (hairs with their `cut`, beards, eyes, tints): a reader returning its lists.
- What each slider id of the appearance scene (0..0x3A, dev/CHARGEN.md) changes, when that is read.

The first use is simple: the hero gets the first preset of its race and gender, as the scene offers it, instead of the bare reset head.

## 2. A morph-less head's tints

A creature with no head morph (the reset hero, `default_hm_noble.utc`'s elf) has the template's `SkinTint`, `HairColor`, `EyesColor` and so on as rows of the tint tables (81 to 88, dev/CHARGEN.md). Which table does each field index, and which column is the tint file? Can `character::Resolve` (or `Character::head`) return the tint files a morph-less head is drawn with, as `Morph` gives a morph's? Today Paragon draws such a head untinted, so the skin is white.

## Why

Issue #17 of DAOParagonEngine, character creation; #22, playing through the mage origin.
