// Notes on the add-in package, read 2026-10-14:
//
// DAUpdater (bin_ship\DAUpdater.Engine.dll, .NET; its IL's string loads and
// calls listed in order by a script of ours, no decompiler) opens a .dazip
// with SharpZipLib and reads Manifest.xml: the root element `manifest` in
// any case, its Type attribute ("addin" or "addins" or "prc", "offer",
// "module") choosing the list (AddInsList of AddInItem, OfferList of
// OfferItem, ModulesList of ModuleItem); of each item the UID attribute and
// the Version and GameVersion elements (no newer than the game's); and the
// item's FileList, each child a `folder` (its name in lower case) with a
// `src` under Contents/ and a `Type` choosing where it goes ("addin" to
// AddIns\, "package" to packages\, "module" to modules\, "offer" to
// offers\), merged into Documents\BioWare\Dragon Age. The item, its FileList
// taken out, is added to Settings\AddIns.xml. A manifest without a FileList
// is refused ("Couldn't find FileList in manifest.xml", seen 2026-10-14).
//
// The game then reads the item from AddIns.xml (LoadAddIns, 0x005FAB11) and
// checks it (0x005EE150) before it looks for its folder (0x005F0A90:
// ADDINS:<uid>, then BUILD_ADDINS:<uid>). An item with only Title,
// Description, Rating, RatingDescription, URL, Version, GameVersion,
// Publisher and PrereqList was installed by DAUpdater and never enabled, no
// "Enabled AddIn" or "Disabled AddIn" in the log (2026-10-14). The same item
// with every element the game's own enabled entries have (each text's en-us
// child, Image, ReleaseDate, Type 1, Price, Size) was enabled and its
// core\override read: the ogre drew red from it (DAOEluvianMCP #140,
// 2026-10-15). Which of them the check wants was not narrowed: all are
// written.
// The install's own offers keep their manifest.xml the same way:
// `<Manifest Type="Offer"><OfferList><OfferItem UID=... ExtendedModuleUID=...
// Priority=... Format="1" BioWare="1"><Title DefaultText=...>` and the rest
// of an AddIns.xml entry. An AddInItem's children here are those of the
// game's own entries in AddIns.xml (the Edge's: Title, Description, Image,
// Rating, RatingDescription, URL, ReleaseDate, Version, GameVersion, Type,
// Price, Size, Publisher, PrereqList); an add-in's core\override is a source
// of the game's (dev/FILES.md).
//
// The zip is the PKWARE format at its plainest: each file a local header and
// its bytes stored, then the central directory and its end, CRC-32 over each
// file, no extra fields, no data descriptors.
#include "dao/core/DaZip.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <set>

namespace dao::core::dazip
{

namespace
{

template <class T> void Append(std::vector<std::byte> &out, T value)
{
    const std::size_t at = out.size();
    out.resize(at + sizeof(T));
    std::memcpy(out.data() + at, &value, sizeof(T));
}

std::uint32_t Crc32(std::span<const std::byte> bytes)
{
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
            }
            t.at(i) = c;
        }
        return t;
    }();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte b : bytes)
    {
        crc = table.at((crc ^ std::to_integer<std::uint32_t>(b)) & 0xFFU) ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

bool Safe(const std::string &path)
{
    return !path.empty() && path.front() != '/' && path.find('\\') == std::string::npos &&
           path.find("..") == std::string::npos;
}

// Text for XML, its markup characters escaped.
std::string Escaped(const std::string &text)
{
    std::string out;
    for (const char c : text)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

} // namespace

std::vector<std::byte> Zip(std::span<const ZipItem> files)
{
    std::set<std::string> seen;
    for (const ZipItem &file : files)
    {
        if (!Safe(file.path) || !seen.insert(file.path).second)
        {
            return {};
        }
    }
    std::vector<std::byte> out;
    std::vector<std::byte> directory;
    for (const ZipItem &file : files)
    {
        const auto offset = static_cast<std::uint32_t>(out.size());
        const std::uint32_t crc = Crc32(file.bytes);
        const auto size = static_cast<std::uint32_t>(file.bytes.size());
        const auto length = static_cast<std::uint16_t>(file.path.size());
        // The local header: version 2.0 to extract, no flags, stored, no time, the sizes, the name.
        Append(out, 0x04034B50U);
        Append(out, std::uint16_t{20});
        Append(out, std::uint16_t{0});
        Append(out, std::uint16_t{0});
        Append(out, std::uint32_t{0x00210000}); // 1 January 1980, midnight, as DOS keeps a date
        Append(out, crc);
        Append(out, size);
        Append(out, size);
        Append(out, length);
        Append(out, std::uint16_t{0});
        for (const char c : file.path)
        {
            Append(out, c);
        }
        out.insert(out.end(), file.bytes.begin(), file.bytes.end());
        // Its central directory entry.
        Append(directory, 0x02014B50U);
        Append(directory, std::uint16_t{20});
        Append(directory, std::uint16_t{20});
        Append(directory, std::uint16_t{0});
        Append(directory, std::uint16_t{0});
        Append(directory, std::uint32_t{0x00210000});
        Append(directory, crc);
        Append(directory, size);
        Append(directory, size);
        Append(directory, length);
        Append(directory, std::uint16_t{0}); // extra
        Append(directory, std::uint16_t{0}); // comment
        Append(directory, std::uint16_t{0}); // disk
        Append(directory, std::uint16_t{0}); // internal attributes
        Append(directory, std::uint32_t{0}); // external attributes
        Append(directory, offset);
        for (const char c : file.path)
        {
            Append(directory, c);
        }
    }
    const auto directoryAt = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), directory.begin(), directory.end());
    Append(out, 0x06054B50U);
    Append(out, std::uint16_t{0});
    Append(out, std::uint16_t{0});
    Append(out, static_cast<std::uint16_t>(files.size()));
    Append(out, static_cast<std::uint16_t>(files.size()));
    Append(out, static_cast<std::uint32_t>(directory.size()));
    Append(out, directoryAt);
    Append(out, std::uint16_t{0});
    return out;
}

std::string Manifest(const AddIn &addIn)
{
    // A text as the game's entries keep theirs: a default, and the en-us text in a CDATA child.
    const auto text = [](const char *tag, const std::string &value) {
        std::string cdata = value;
        for (std::size_t at = 0; (at = cdata.find("]]>", at)) != std::string::npos; at += 15)
        {
            cdata.replace(at, 3, "]]]]><![CDATA[>");
        }
        return std::string("\t\t\t<") + tag + " DefaultText=\"" + Escaped(value) + "\">\n\t\t\t\t<en-us><![CDATA[" +
               cdata + "]]></en-us>\n\t\t\t</" + tag + ">\n";
    };
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                      "<Manifest Type=\"AddIn\">\n"
                      "\t<AddInsList>\n";
    out += "\t\t<AddInItem UID=\"" + Escaped(addIn.uid) + "\" Name=\"" + Escaped(addIn.name) +
           "\" ExtendedModuleUID=\"" + Escaped(addIn.extends) + "\" Priority=\"" + std::to_string(addIn.priority) +
           "\" Enabled=\"1\" State=\"2\" Format=\"1\" BioWare=\"0\" RequiresAuthorization=\"0\">\n";
    out += text("Title", addIn.title);
    out += text("Description", addIn.description);
    out += text("Rating", "");
    out += text("RatingDescription", "");
    out += text("URL", "");
    out += "\t\t\t<Image>" + Escaped(addIn.image) + "</Image>\n";
    out += "\t\t\t<ReleaseDate>" + Escaped(addIn.releaseDate) + "</ReleaseDate>\n";
    out += "\t\t\t<Version>" + Escaped(addIn.version) + "</Version>\n";
    out += "\t\t\t<GameVersion>1.0</GameVersion>\n";
    out += "\t\t\t<Type>1</Type>\n";
    out += "\t\t\t<Price>0</Price>\n";
    out += "\t\t\t<Size>1</Size>\n";
    out += text("Publisher", addIn.publisher);
    out += "\t\t\t<PrereqList />\n";
    // What DAUpdater copies: Contents/addins/ merged into Documents\BioWare\Dragon Age\AddIns\.
    out += "\t\t\t<FileList>\n\t\t\t\t<Folder src=\"addins\" Type=\"addin\" />\n\t\t\t</FileList>\n";
    out += "\t\t</AddInItem>\n\t</AddInsList>\n</Manifest>\n";
    return out;
}

std::vector<std::byte> Package(const AddIn &addIn, std::span<const AddInFile> files)
{
    if (addIn.uid.empty() || !std::ranges::all_of(addIn.uid, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        }))
    {
        return {};
    }
    std::vector<ZipItem> items;
    const std::string manifest = Manifest(addIn);
    const std::span<const std::byte> manifestBytes = std::as_bytes(std::span(manifest));
    items.push_back({"Manifest.xml", std::vector<std::byte>(manifestBytes.begin(), manifestBytes.end())});
    for (const AddInFile &file : files)
    {
        if (!Safe(file.path))
        {
            return {};
        }
        items.push_back({"Contents/addins/" + addIn.uid + "/" + file.path, file.bytes});
    }
    return Zip(items);
}

} // namespace dao::core::dazip
