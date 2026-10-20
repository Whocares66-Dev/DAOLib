# A material object's `<texture>` tag in lower case (from paragon-agent, 2026-10-20)

`MaterialObject.cpp`'s `Take` compares the element name case-sensitively (`element == "Texture"`), and two of the game's material objects write it `<texture Name='SkyTextureA' ResName='SB03_BCloud' />`: `Clouds.mao` and `CloudMaterial.mao` (the Fade's and Redcliffe's sky domes' clouds; Clouds.mat). Core reads each with no textures (`Mao::textures()` is empty), so a port of Clouds.psh has no SkyTextureA, SkyTextureB or PerlinNoise to bind. Their attributes are single-quoted (`Attribute` already takes either quote). The other tags of those files (`<Material>`) are in the usual case.

The game's own reader takes them (its clouds draw with the textures, captured by elgara-agent in the Fade and Redcliffe), so its element match is not case-sensitive.

Wanted: an ASCII case-insensitive match of the element names in `Take` (`MaterialObject`, `Material`, `DefaultSemantic`, `Texture`, and the parameter elements). Paragon reads those two files' textures itself meanwhile (level.rs `lower_case_textures`) and will drop that when core does.

Also seen: Clouds.mat has no "Default" semantic (it has "Clouds" and "FadeClouds"), and CloudMaterial.mao and Clouds.mao name none, yet the game drew Clouds.psh with CloudMaterial in the Fade (clouds.vsh/psh in capture.fade.jsonl). Paragon takes the first semantic where the default is missing for Clouds.mat; how the exe chooses is not read.
