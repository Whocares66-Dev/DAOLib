// A mod packed as the add-in package DAUpdater installs (.dazip, DAOLib
// #122): a zip of a Manifest.xml naming the add-in and its files under
// Contents\, which the updater copies into Documents\BioWare\Dragon Age and
// enters in Settings\AddIns.xml, so the player can turn it off in the game's
// Downloadable Content screen.
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace dao::core::dazip
{

// A file in a zip: its path, with forward slashes, and its bytes.
struct ZipItem
{
    std::string path;
    std::vector<std::byte> bytes;
};

// Returns a zip of the files, each stored as it is, uncompressed.
// @return empty for a path that is empty, starts with a slash, holds a backslash or "..", or is
// given twice
std::vector<std::byte> Zip(std::span<const ZipItem> files);

// What AddIns.xml says of an add-in, as the game's own entries have it.
struct AddIn
{
    std::string uid;   // its folder's name and the game's key for it: letters, digits, _
    std::string name;  // a short name
    std::string title; // what the player sees
    std::string description;
    std::string publisher;
    std::string version = "1.0";
    std::string image;                      // a picture the Downloadable Content screen shows, by name
    std::string releaseDate = "01/01/2026"; // as the game's entries have it, month/day/year
    std::string extends = "Single Player";  // the campaign it adds to: ExtendedModuleUID
    int priority = 1000;                    // of two add-ins with one file, the lower priority's wins
};

// A file of the add-in: its path in the add-in's folder, with forward
// slashes, and its bytes. Models, textures and tables go in "core/override/"
// (loose, or an .erf of many); the add-in's own talk tables in
// "core/data/talktables/<uid>_c_<language>.tlk", as the game's add-ins keep
// theirs.
struct AddInFile
{
    std::string path;
    std::vector<std::byte> bytes;
};

// Returns the Manifest.xml of an add-in's package.
std::string Manifest(const AddIn &addIn);

// Returns a .dazip of an add-in: its manifest, and its files in
// Contents/addins/<uid>/.
// @return empty for a uid that is empty or not letters, digits and _, or a path the zip refuses
std::vector<std::byte> Package(const AddIn &addIn, std::span<const AddInFile> files);

} // namespace dao::core::dazip
