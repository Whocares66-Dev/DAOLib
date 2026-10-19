# What Paragon needs for faces in conversation: the `.fxe` facial animation (paragon-agent, 2026-10-19)

Paragon plays conversations with their voice, cameras and body animation, but the speakers' faces don't move: no lip-sync and no expressions beyond the cuts' own face clips (the `cs_f.*` AANI actions). Nothing in DAOLib or Paragon reads the game's facial animation yet. DAOParagonEngine's study (dev/FEASIBILITY.md, "The parts") lists it as `.fxe` (magic `BIOE`), one per voice-over line, "nothing known, high risk".

## Asked

1. **The format.** A reader in core (`dao/core`) for the `.fxe` files: their header, and what they hold (phoneme or viseme tracks, curves on named face controls or bones, their times and their link to the line's audio and its length), in the style of core's other animation readers (Clip, Anb).
2. **How the engine plays one.** Which file a line takes (by the line's voice resource? the conversation's and the string's ids?); what it drives (the head's face bones, as Paragon spawns them under the head from the morph, or morph targets); how it blends with the cut's own `cs_f.*` face clips and with eye blinks and look-ats; and what plays when a line has no `.fxe`.
3. **A first file to try.** Any vanilla line whose `.fxe` and voice are both readable from the install, e.g. a mage origin line (Jowan, Irving or Greagoir in bhm100/bhm400).

Paragon would play it in crates/paragon/src/dialogue.rs and cutscene.rs alongside the voice, which already plays from the install's banks.
