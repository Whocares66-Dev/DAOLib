// A Direct3D 9 shader's constant table, read from its bytecode: what each constant the shader takes
// is called, which registers it fills and what type it is (DAOLib #246).
//
//   const auto table = dao::core::ctab::Read(bytecode);
//   for (const auto &constant : table->constants)
//       ...   // constant.name, constant.set, constant.index, constant.count
//
// The table is the compiler's: a comment token in the bytecode, right after the version token, that
// begins with "CTAB" (D3DXSHADER_CONSTANTTABLE). The game's shaders (shaders.erf's .vsh.fxo and
// .psh.fxo) are plain bytecode that carry one; read 2026-10-23 in face0.psh.fxo, ps_3_0 with 34
// constants. Offsets inside the table are from the byte after "CTAB".
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::ctab
{

// The registers a constant is put in (D3DXREGISTER_SET).
enum class RegisterSet : std::uint16_t
{
    Bool = 0,
    Int4 = 1,
    Float4 = 2,
    Sampler = 3,
};

// What a constant's type is (D3DXPARAMETER_CLASS).
enum class Class : std::uint16_t
{
    Scalar = 0,
    Vector = 1,
    MatrixRows = 2,
    MatrixColumns = 3,
    Object = 4,
    Struct = 5,
};

// One constant of the table.
struct Constant
{
    std::string name;
    RegisterSet set = RegisterSet::Float4;
    std::uint16_t index = 0; // its first register
    std::uint16_t count = 0; // how many registers it takes
    Class kind = Class::Scalar;
    std::uint16_t type = 0; // D3DXPARAMETER_TYPE: 1 bool, 2 int, 3 float, 10-14 the samplers
    std::uint16_t rows = 0;
    std::uint16_t columns = 0;
    std::uint16_t elements = 0; // an array's length, 1 for one that is not an array
};

struct Table
{
    std::uint32_t version = 0; // the shader's version token: 0xFFFF0300 is ps_3_0, 0xFFFE0300 vs_3_0
    std::string target;        // "ps_3_0"
    std::string creator;       // the compiler's name
    std::vector<Constant> constants;
};

// Returns a shader's constant table from its bytecode, or nothing for bytecode with no table or one
// that does not read as one.
std::optional<Table> Read(std::span<const std::byte> bytecode);

// Returns the constant whose registers of a set include one, or null.
const Constant *At(const Table &table, RegisterSet set, std::uint16_t reg);

} // namespace dao::core::ctab
