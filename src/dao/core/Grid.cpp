// dao/core/Grid.h: a level layout's walk grid, its cells as lists and its
// coarse levels decoded as the exe reads them.
#include "dao/core/Grid.h"

#include <array>
#include <cmath>
#include <cstring>
#include <span>

namespace dao::core::navigation
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kPathfindExport = 3020;
constexpr std::uint32_t kGridArea = 3110;
constexpr std::uint32_t kColumns = 3086;
constexpr std::uint32_t kRows = 3087;
constexpr std::uint32_t kCellSize = 3088;
constexpr std::uint32_t kClearance = 3089;
constexpr std::uint32_t kBase = 3090;
constexpr std::uint32_t kNormal = 3091;
constexpr std::uint32_t kCells = 3092;
constexpr std::uint32_t kHeights = 3093;
constexpr std::uint32_t kSectorSize = 3094;
constexpr std::uint32_t kSectors = 3095;
constexpr std::uint32_t kMemory = 3096;
constexpr std::uint32_t kSoundData = 3114;
constexpr std::uint32_t kPadding = 3120;
constexpr std::uint32_t kSectorEntrances = 3115;
constexpr std::uint32_t kSectorOffset = 3116;
constexpr std::uint32_t kLightSubsets = 3117;
constexpr std::uint32_t kCollision = 3744;
constexpr std::uint32_t kCollisionData = 3745;

constexpr std::uint8_t kKindBits = 0x0F;
constexpr std::uint8_t kCounterBits = 0xF0;
constexpr std::uint8_t kFirstUnwalkable = 5;
constexpr std::uint32_t kWrap = 0x10000;

// The block a direction leads to, as 0x00A4AB80 steps: a column and a row.
constexpr std::array<std::array<std::int32_t, 2>, 8> kSteps{{
    {0, -1},
    {1, 0},
    {0, 1},
    {-1, 0},
    {1, -1},
    {1, 1},
    {-1, 1},
    {-1, -1},
}};

template <class T> std::vector<T> ListOf(const gff4::Struct &grid, std::uint32_t label, gff4::Type type)
{
    const std::span<const std::byte> bytes = grid.ListBytes(label, type);
    std::vector<T> out(bytes.size() / sizeof(T));
    if (!out.empty())
    {
        std::memcpy(out.data(), bytes.data(), out.size() * sizeof(T));
    }
    return out;
}

Point PointOf(const std::vector<float> &floats)
{
    return floats.size() >= 3 ? Point{floats[0], floats[1], floats[2]} : Point{};
}

// A point's index along one axis, as 0x00A49410 and 0x00A49440 work it out: truncated by cvttsd2si, and negative
// taken as 0. Past an int's range cvttsd2si answers INT_MIN, so that is 0 too.
std::int32_t IndexOf(float at, float base, float padding, float cellSize)
{
    const double index = ((static_cast<double>(at) - padding - base) / cellSize) + 0.5;
    constexpr double kPastInt = 2147483648.0;
    return index >= 0.0 && index < kPastInt ? static_cast<std::int32_t>(index) : 0;
}

std::int32_t Across(std::int32_t cells, std::int32_t side)
{
    return side > 0 ? (cells + side - 1) / side : 0;
}

// A sector as written: its offset into the memory and its count of entrances.
struct Sector
{
    std::uint32_t offset = 0;
    std::uint8_t entrances = 0;
};

// The sectors, each offset 0x10000 more from the first that is less than the one before.
std::vector<Sector> SectorsOf(const gff4::Struct &grid)
{
    std::vector<Sector> out;
    for (const gff4::Struct &sector : grid.Structs(kSectors))
    {
        out.push_back({.offset = sector.Get<std::uint16_t>(kSectorOffset).value_or(0),
                       .entrances = sector.Get<std::uint8_t>(kSectorEntrances).value_or(0)});
    }
    for (std::size_t i = 1; i < out.size(); ++i)
    {
        if (out[i].offset < out[i - 1].offset)
        {
            for (std::size_t j = i; j < out.size(); ++j)
            {
                out[j].offset += kWrap;
            }
            break;
        }
    }
    return out;
}

// A level's cut of the grid: the cells a block's side, and its sectors.
struct Cut
{
    std::int32_t blockCells = 0;
    std::int32_t columns = 0;
    std::int32_t rows = 0;
    std::span<const Sector> sectors;
    bool second = false; // places in sixteenths of the block, not cells
};

// An entrance's cell from its place byte in block `block`.
std::array<std::int32_t, 2> CellOf(const Cut &cut, std::uint32_t block, std::uint8_t place, std::int32_t size)
{
    const std::int32_t left = (static_cast<std::int32_t>(block) % cut.columns) * cut.blockCells;
    const std::int32_t top = (static_cast<std::int32_t>(block) / cut.columns) * cut.blockCells;
    if (cut.second)
    {
        return {left + ((cut.blockCells * (place >> 4)) / 16), top + ((cut.blockCells * (place & 0x0F)) / 16)};
    }
    return {left + (place % size), top + (place / size)};
}

// A block's entrances and their edges; none where its bytes run out of the memory.
std::vector<Entrance> EntrancesOf(const Cut &cut, std::uint32_t block, std::span<const std::uint8_t> memory,
                                  std::int32_t size)
{
    std::vector<Entrance> out;
    const Sector &sector = cut.sectors[block];
    const std::size_t first = sector.offset;
    const std::size_t edges = first + (2 * std::size_t{sector.entrances});
    if (edges > memory.size())
    {
        return out;
    }
    std::size_t from = 0;
    for (std::size_t k = 0; k < sector.entrances; ++k)
    {
        const auto [column, row] = CellOf(cut, block, memory[first + (2 * k)], size);
        Entrance entrance{.column = column, .row = row, .edges = {}};
        const std::size_t to = memory[first + (2 * k) + 1];
        for (std::size_t j = from; j < to && edges + j < memory.size(); ++j)
        {
            const std::uint8_t edge = memory[edges + j];
            const auto [dx, dy] = kSteps.at(edge >> 5);
            const std::int32_t x = (static_cast<std::int32_t>(block) % cut.columns) + dx;
            const std::int32_t y = (static_cast<std::int32_t>(block) / cut.columns) + dy;
            const auto index = static_cast<std::uint32_t>((y * cut.columns) + x);
            const auto at = static_cast<std::uint8_t>(edge & 0x1F);
            if (x >= 0 && y >= 0 && x < cut.columns && y < cut.rows && at < cut.sectors[index].entrances)
            {
                entrance.edges.push_back({.block = index, .entrance = at});
            }
        }
        from = to;
        out.push_back(std::move(entrance));
    }
    return out;
}

Level LevelOf(const Cut &cut, std::span<const std::uint8_t> memory, std::int32_t size)
{
    Level level{.blockCells = cut.blockCells, .columns = cut.columns, .rows = cut.rows, .blocks = {}};
    for (std::uint32_t block = 0; block < cut.sectors.size(); ++block)
    {
        level.blocks.push_back(EntrancesOf(cut, block, memory, size));
    }
    return level;
}

// The coarse levels: the first's blocks, then the second's where the sectors hold both.
std::vector<Level> AbstractionOf(const Grid &grid, const std::vector<Sector> &sectors,
                                 std::span<const std::uint8_t> memory)
{
    std::vector<Level> out;
    const std::int32_t size = grid.sectorSize;
    const std::int32_t columns = Across(grid.columns, size);
    const std::int32_t rows = Across(grid.rows, size);
    const auto first = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    const std::int32_t superColumns = Across(columns, 2);
    const std::int32_t superRows = Across(rows, 2);
    const auto second = static_cast<std::size_t>(superColumns) * static_cast<std::size_t>(superRows);
    if (size <= 0 || (sectors.size() != first && sectors.size() != first + second))
    {
        return out;
    }
    const std::span<const Sector> all(sectors);
    out.push_back(
        LevelOf({.blockCells = size, .columns = columns, .rows = rows, .sectors = all.first(first)}, memory, size));
    if (sectors.size() == first + second)
    {
        out.push_back(LevelOf({.blockCells = 2 * size,
                               .columns = superColumns,
                               .rows = superRows,
                               .sectors = all.subspan(first),
                               .second = true},
                              memory, size));
    }
    return out;
}

} // namespace

std::uint8_t Grid::Kind(std::int32_t row, std::int32_t column) const
{
    if (row < 0 || column < 0 || row >= rows || column >= columns)
    {
        return kOffGrid;
    }
    const auto at =
        (static_cast<std::size_t>(row) * static_cast<std::size_t>(columns)) + static_cast<std::size_t>(column);
    return at < cells.size() ? static_cast<std::uint8_t>(cells[at] & kKindBits) : kOffGrid;
}

bool Grid::Walkable(std::int32_t row, std::int32_t column) const
{
    return Kind(row, column) < kFirstUnwalkable;
}

std::optional<float> Grid::Height(std::int32_t row, std::int32_t column) const
{
    if (row < 0 || column < 0 || row >= rows || column >= columns)
    {
        return std::nullopt;
    }
    const auto at =
        (static_cast<std::size_t>(row) * static_cast<std::size_t>(columns)) + static_cast<std::size_t>(column);
    return at < heights.size() ? std::optional(static_cast<float>(heights[at]) * kHeightUnit) : std::nullopt;
}

Point Grid::Place(std::int32_t row, std::int32_t column) const
{
    return {base[0] + (static_cast<float>(column) * cellSize) + padding,
            base[1] + (static_cast<float>(row) * cellSize) + padding, Height(row, column).value_or(0.0F)};
}

Cell Grid::CellAt(float x, float y) const
{
    return {.row = IndexOf(y, base[1], padding, cellSize), .column = IndexOf(x, base[0], padding, cellSize)};
}

void Grid::SetKind(std::int32_t row, std::int32_t column, std::uint8_t kind)
{
    if (row < 0 || column < 0 || row >= rows || column >= columns)
    {
        return;
    }
    const auto at =
        (static_cast<std::size_t>(row) * static_cast<std::size_t>(columns)) + static_cast<std::size_t>(column);
    if (at < cells.size())
    {
        cells[at] = static_cast<std::uint8_t>((cells[at] & kCounterBits) | (kind & kKindBits));
    }
}

Grid GridOf(const gff4::Struct &area)
{
    Grid grid;
    grid.columns = area.Get<std::int32_t>(kColumns).value_or(0);
    grid.rows = area.Get<std::int32_t>(kRows).value_or(0);
    grid.cellSize = area.Get<float>(kCellSize).value_or(0.0F);
    grid.clearance = area.Get<float>(kClearance).value_or(0.0F);
    const std::optional<float> padding = area.Get<float>(kPadding);
    grid.padding = padding && !std::isnan(*padding) ? *padding : grid.cellSize / 2;
    grid.base = PointOf(area.Floats(kBase));
    grid.normal = PointOf(area.Floats(kNormal));
    grid.cells = ListOf<std::uint8_t>(area, kCells, gff4::Type::U8);
    grid.heights = ListOf<std::int16_t>(area, kHeights, gff4::Type::S16);
    grid.sounds = ListOf<std::int8_t>(area, kSoundData, gff4::Type::S8);
    grid.lightSubsets = ListOf<std::uint8_t>(area, kLightSubsets, gff4::Type::U8);
    grid.sectorSize = area.Get<std::int32_t>(kSectorSize).value_or(0);
    const std::vector<std::uint8_t> memory = ListOf<std::uint8_t>(area, kMemory, gff4::Type::U8);
    grid.abstraction = AbstractionOf(grid, SectorsOf(area), memory);
    if (const std::optional<gff4::Struct> collision = area.Child(kCollision))
    {
        grid.collision = ListOf<std::uint8_t>(*collision, kCollisionData, gff4::Type::U8);
    }
    return grid;
}

std::optional<Grid> GridOf(const gff4::File &layout)
{
    const std::optional<gff4::Struct> root = layout.Root();
    const std::optional<gff4::Struct> pathfind = root ? root->Child(kPathfindExport) : std::nullopt;
    const std::optional<gff4::Struct> area = pathfind ? pathfind->Child(kGridArea) : std::nullopt;
    if (!area)
    {
        return std::nullopt;
    }
    return GridOf(*area);
}

} // namespace dao::core::navigation
