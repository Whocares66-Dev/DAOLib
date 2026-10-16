---
title: Compatibility
nav_order: 3
permalink: /compatibility.html
---

# Compatibility

## DAFix

[DAFix](https://github.com/Lyall/DAFix) is an [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) mod that provides various quality-of-life fixes. To load it, extract only `DAFix.asi` and `DAFix.ini` into `bin_ship`, and leave out the `dinput8.dll` and `dinput8.ini` that come with it.

## Other `dinput8.dll` mods

Rename the other mod's `dinput8.dll` to `dinput8.chain.dll` and keep DAOLib's as `dinput8.dll`: DAOLib passes the game's calls on to it. One such mod at a time. This does not work for Ultimate ASI Loader.
