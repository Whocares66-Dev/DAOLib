// What dao/core/Model.cpp's reader and ModelWrite.cpp's writers share: the
// fields' labels, and a hierarchy's nodes' structs found as HierarchyOf finds
// the nodes. Not an API.
#pragma once

#include "dao/core/Gff4.h"

#include <cstdint>
#include <vector>

namespace dao::core::model
{

// The fields' labels (dev/gff4_fields.csv).
inline constexpr std::uint32_t kName = 2;
inline constexpr std::uint32_t kMmhName = 6000;
inline constexpr std::uint32_t kMaterialObject = 6001;
inline constexpr std::uint32_t kNodeId = 6004;
inline constexpr std::uint32_t kModelMeshFile = 6005;
inline constexpr std::uint32_t kMeshGroupName = 6006;
inline constexpr std::uint32_t kTranslation = 6047;
inline constexpr std::uint32_t kRotation = 6048;
inline constexpr std::uint32_t kScale = 6278;
inline constexpr std::uint32_t kBoneIndex = 6254;
inline constexpr std::uint32_t kBonesUsed = 6255;
inline constexpr std::uint32_t kTotalBones = 6256;
inline constexpr std::uint32_t kChildren = 6999;
inline constexpr std::uint32_t kChunkVertexSize = 8000;
inline constexpr std::uint32_t kChunkVertexCount = 8001;
inline constexpr std::uint32_t kChunkIndexCount = 8002;
inline constexpr std::uint32_t kChunkPrimitive = 8003;
inline constexpr std::uint32_t kChunkIndexFormat = 8004;
inline constexpr std::uint32_t kChunkBaseVertex = 8005;
inline constexpr std::uint32_t kChunkVertexOffset = 8006;
inline constexpr std::uint32_t kChunkMinIndex = 8007;
inline constexpr std::uint32_t kChunkVerticesReferenced = 8008;
inline constexpr std::uint32_t kChunkStartIndex = 8009;
inline constexpr std::uint32_t kMeshChunks = 8021;
inline constexpr std::uint32_t kMeshVertices = 8022;
inline constexpr std::uint32_t kMeshIndices = 8023;
inline constexpr std::uint32_t kChunkDeclarators = 8025;
inline constexpr std::uint32_t kDeclaratorStream = 8026;
inline constexpr std::uint32_t kDeclaratorOffset = 8027;
inline constexpr std::uint32_t kDeclaratorType = 8028;
inline constexpr std::uint32_t kDeclaratorUsage = 8029;
inline constexpr std::uint32_t kDeclaratorUsageIndex = 8030;
inline constexpr std::uint32_t kDeclaratorMethod = 8031;
inline constexpr std::uint32_t kBoxMin = 6054;
inline constexpr std::uint32_t kBoxMax = 6055;
inline constexpr std::uint32_t kChunkAdditionalStreams = 8011;
inline constexpr std::uint32_t kBoundsBoxMin = 8017;
inline constexpr std::uint32_t kBoundsBoxMax = 8018;
inline constexpr std::uint32_t kBoundsSphere = 8019;
inline constexpr std::uint32_t kChunkBounds = 8020;

} // namespace dao::core::model

namespace dao::core::model::detail
{

// Returns the structs of a hierarchy's nodes, in Hierarchy::nodes' order.
std::vector<gff4::Struct> NodeStructs(const gff4::File &hierarchy);

} // namespace dao::core::model::detail
