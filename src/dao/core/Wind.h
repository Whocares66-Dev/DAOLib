// A level's wind (.wnd) read, over bytes: the strength and direction the
// level's trees and cloth are blown with, the gusts, and the tree and
// cloth parameters the toolset lets a designer set (DAOLib #196).
//
//   const auto wind = dao::core::wind::WindOf(file);
//   wind->strength;  // 0.25
//   wind->direction; // {0, 1, 0}
//
// The file is GFF4 "WND V4.0" with one struct, WIND, named by an area's
// EnvAreaGlobalwindResRef (oth000d_193.wnd is the Fade's). Read 2026-10-18
// over all 115 of the game's (core and add-ins): every file has the same
// template, 0xC0 bytes, and every field below, and every file is 592 bytes.
//
// Only WndStrength, WndDirection and the gusts (WndGustMin/MaxStrength,
// WndGustMin/MaxDuration and the gust frequency) reach the engine's wind:
// the trees' five CSpeedWinds are loaded from a text built into the exe, so
// the file's matrix count, leaf angles, response, exponents, oscillations,
// rocking and rustling are never used (dev/TREES.md, "the trees as drawn",
// lib-agent-2, 2026-10-18). They are kept here as the file holds them, as is
// the cloth's, which dev/TREES.md does not trace, and WndRadiusFalloff.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>

namespace dao::core::wind
{

struct Wind
{
    // What reaches the engine's wind.
    float strength = 0.0F;            // WndStrength (22003)
    std::array<float, 3> direction{}; // WndDirection (22004)
    float gustMinStrength = 0.0F;     // WndGustMinStrength (22010)
    float gustMaxStrength = 0.0F;     // WndGustMaxStrength (22011)
    float gustMinDuration = 0.0F;     // WndGustMinDuration (22012), seconds
    float gustMaxDuration = 0.0F;     // WndGustMaxDuration (22013), seconds
    float gustFrequency = 0.0F;       // LvlWindSptGustFrequency (3730), a minute; 0 for no gusts

    // Read, and not used by the engine.
    float radiusFalloff = 0.0F;                    // WndRadiusFalloff (22005)
    std::int8_t treeNumWindMatrices = 0;           // WndTreeNumWindMatrices (22020)
    std::int8_t treeNumLeafAngles = 0;             // WndTreeNumLeafAngles (22021)
    float treeResponse = 0.0F;                     // WndTreeResponse (22022)
    float treeResponseLimit = 0.0F;                // WndTreeResponseLimit (22023)
    float treeBranchExponent = 0.0F;               // WndTreeBranchExponent (22025)
    float treeLeafExponent = 0.0F;                 // WndTreeLeafExponent (22026)
    std::array<float, 4> treeBranchOscillationX{}; // WndTreeBranchOscillationX (22027)
    std::array<float, 4> treeBranchOscillationY{}; // WndTreeBranchOscillationY (22028)
    std::array<float, 4> treeLeafRocking{};        // WndTreeLeafRocking (22029)
    std::array<float, 4> treeLeafRustling{};       // WndTreeLeafRustling (22030)
    float clothResponse = 0.0F;                    // WndClothResponse (22031)
    float clothResponseLimit = 0.0F;               // WndClothResponseLmt (22032)
    float clothStrength = 0.0F;                    // WndClothStrength (22033)
    float clothGustStrengthMin = 0.0F;             // WndClothGustStrengthMin (22034)
    float clothGustStrengthMax = 0.0F;             // WndClothGustStrengthMax (22035)
    float clothGustDurationMin = 0.0F;             // WndClothGustDurationMin (22036)
    float clothGustDurationMax = 0.0F;             // WndClothGustDurationMax (22037)
    float clothGustIntervalMin = 0.0F;             // WndClothGustIntervalMin (22038)
    float clothGustIntervalMax = 0.0F;             // WndClothGustIntervalMax (22039)
    float clothGustDirChange = 0.0F;               // WndClothGustDirChange (22040)
    std::array<float, 3> clothGustAxisRatio{};     // WndClothGustAxisRatio (22041)
};

// Returns a wind file's wind, or nothing for a file that is not one.
std::optional<Wind> WindOf(const gff4::File &file);

// Returns the wind a WIND struct holds.
Wind WindOf(const gff4::Struct &root);

} // namespace dao::core::wind
