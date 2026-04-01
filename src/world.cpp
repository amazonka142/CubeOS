#include "world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <utility>

#include <glm/gtc/noise.hpp>

namespace {

int floorDiv(int value, int divisor) {
  int q = value / divisor;
  int r = value % divisor;
  if (r != 0 && ((r < 0) != (divisor < 0))) {
    q -= 1;
  }
  return q;
}

int positiveMod(int value, int divisor) {
  int m = value % divisor;
  if (m < 0) {
    m += divisor;
  }
  return m;
}

uint32_t mixBits(uint32_t x);

double seedNoiseOffset(int seed, uint32_t salt) {
  uint32_t mixed = mixBits(static_cast<uint32_t>(seed) ^ (salt * 0x9E3779B9u));
  double unit = static_cast<double>(mixed) / static_cast<double>(std::numeric_limits<uint32_t>::max());
  return (unit * 2.0 - 1.0) * 32768.0;
}

double seededPerlin2D(double x,
                      double z,
                      int seed,
                      uint32_t saltX,
                      uint32_t saltZ,
                      double frequencyX,
                      double frequencyZ) {
  return glm::perlin(glm::dvec2((x + seedNoiseOffset(seed, saltX)) * frequencyX,
                                (z + seedNoiseOffset(seed, saltZ)) * frequencyZ));
}

double seededPerlin3D(double x,
                      double y,
                      double z,
                      int seed,
                      uint32_t saltX,
                      uint32_t saltY,
                      uint32_t saltZ,
                      double frequencyX,
                      double frequencyY,
                      double frequencyZ) {
  return glm::perlin(glm::dvec3((x + seedNoiseOffset(seed, saltX)) * frequencyX,
                                (y + seedNoiseOffset(seed, saltY)) * frequencyY,
                                (z + seedNoiseOffset(seed, saltZ)) * frequencyZ));
}

double seededRidge2D(double x,
                     double z,
                     int seed,
                     uint32_t saltX,
                     uint32_t saltZ,
                     double frequencyX,
                     double frequencyZ) {
  return 1.0 - std::abs(seededPerlin2D(x, z, seed, saltX, saltZ, frequencyX, frequencyZ));
}

double seededRidge3D(double x,
                     double y,
                     double z,
                     int seed,
                     uint32_t saltX,
                     uint32_t saltY,
                     uint32_t saltZ,
                     double frequencyX,
                     double frequencyY,
                     double frequencyZ) {
  return 1.0 - std::abs(seededPerlin3D(x, y, z, seed, saltX, saltY, saltZ,
                                       frequencyX, frequencyY, frequencyZ));
}

float fbmNoise(float x, float z, int seed) {
  float amplitude = 1.0f;
  float frequency = 0.008f;
  float total = 0.0f;
  float maxValue = 0.0f;
  double seedX = seedNoiseOffset(seed, 0xB5297A4Du);
  double seedZ = seedNoiseOffset(seed, 0x68E31DA4u);
  for (int i = 0; i < 4; ++i) {
    glm::dvec2 p((static_cast<double>(x) + seedX) * static_cast<double>(frequency),
                 (static_cast<double>(z) + seedZ) * static_cast<double>(frequency));
    total += static_cast<float>(glm::perlin(p)) * amplitude;
    maxValue += amplitude;
    amplitude *= 0.5f;
    frequency *= 2.0f;
  }
  if (maxValue <= 0.0f) {
    return 0.0f;
  }
  return total / maxValue;
}

float sampleSurfaceReliefNoise(int worldX, int worldZ, int seed) {
  float broad = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 83u, 79u, 0.0105, 0.0105));
  float fine = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 29u, 31u, 0.0215, 0.0215));
  float ridge = static_cast<float>(seededRidge2D(worldX, worldZ, seed, 11u, 13u, 0.0165, 0.0165));
  return broad * 0.72f + fine * 0.38f + (ridge - 0.5f) * 0.95f;
}

float sampleSurfacePatchNoise01(int worldX, int worldZ, int seed) {
  float broad = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 47u, 41u, 0.0185, 0.0185));
  float fine = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 71u, 67u, 0.0390, 0.0390));
  return std::clamp(broad * 0.68f + fine * 0.32f, 0.0f, 1.0f);
}

float sampleSurfaceThicknessNoise(int worldX, int worldZ, int seed) {
  float broad = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 19u, 23u, 0.032, 0.032));
  float fine = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 5u, 7u, 0.061, 0.061));
  return broad * 0.78f + fine * 0.42f;
}

float saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

float smooth01(float v) {
  float t = saturate(v);
  return t * t * (3.0f - 2.0f * t);
}

float lerpValue(float a, float b, float t) {
  return a + (b - a) * t;
}

float inverseLerp(float a, float b, float value) {
  float denom = b - a;
  if (std::abs(denom) < 1e-6f) {
    return 0.0f;
  }
  return (value - a) / denom;
}

template <size_t N>
float sampleSplineCurve(const std::array<std::pair<float, float>, N>& points, float x) {
  static_assert(N >= 2, "sampleSplineCurve requires at least 2 points");
  if (x <= points.front().first) {
    return points.front().second;
  }
  if (x >= points.back().first) {
    return points.back().second;
  }

  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[i + 1];
    if (x <= b.first) {
      float t = smooth01(inverseLerp(a.first, b.first, x));
      return lerpValue(a.second, b.second, t);
    }
  }

  return points.back().second;
}

float sampleBandCurve(float x, float center, float halfWidth) {
  if (halfWidth <= 0.0f) {
    return 0.0f;
  }
  float d = std::abs(x - center) / halfWidth;
  if (d >= 1.0f) {
    return 0.0f;
  }
  return smooth01(1.0f - d);
}

float peaksAndValleys(float weirdness) {
  float w = weirdness * 2.0f - 1.0f;
  float pv = -(std::abs(std::abs(w) - 0.6666667f) - 0.3333333f) * 3.0f;
  return std::clamp(pv, -1.0f, 1.0f);
}

uint32_t mixBits(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

float hashedNoise01(int x, int y, int z, int seed, uint32_t salt) {
  uint32_t h = static_cast<uint32_t>(x) * 73856093u;
  h ^= static_cast<uint32_t>(y) * 19349663u;
  h ^= static_cast<uint32_t>(z) * 83492791u;
  h ^= static_cast<uint32_t>(seed) * 2654435761u;
  h ^= salt;
  h = mixBits(h);
  return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
}

int chunkLocalIndex(int lx, int ly, int lz) {
  return lx + lz * kChunkSize + ly * kChunkSize * kChunkSize;
}

constexpr int kSectionKeyCoordBits = 30;
constexpr int64_t kSectionKeyCoordBias = static_cast<int64_t>(1) << (kSectionKeyCoordBits - 1);
constexpr uint64_t kSectionKeyCoordMask = (static_cast<uint64_t>(1) << kSectionKeyCoordBits) - 1u;
constexpr int kSectionKeySectionBits = 4;
constexpr uint64_t kSectionKeySectionMask = (static_cast<uint64_t>(1) << kSectionKeySectionBits) - 1u;

uint64_t packSectionKey(int cx, int cz, int sectionY) {
  int64_t bx = static_cast<int64_t>(cx) + kSectionKeyCoordBias;
  int64_t bz = static_cast<int64_t>(cz) + kSectionKeyCoordBias;
  uint64_t ux = static_cast<uint64_t>(bx) & kSectionKeyCoordMask;
  uint64_t uz = static_cast<uint64_t>(bz) & kSectionKeyCoordMask;
  uint64_t us = static_cast<uint64_t>(std::clamp(sectionY, 0, kChunkSectionCount - 1)) &
                kSectionKeySectionMask;
  return (ux << 34) | (uz << 4) | us;
}

bool unpackSectionKey(uint64_t key, int& outCx, int& outCz, int& outSectionY) {
  uint64_t ux = (key >> 34) & kSectionKeyCoordMask;
  uint64_t uz = (key >> 4) & kSectionKeyCoordMask;
  uint64_t us = key & kSectionKeySectionMask;
  outCx = static_cast<int>(static_cast<int64_t>(ux) - kSectionKeyCoordBias);
  outCz = static_cast<int>(static_cast<int64_t>(uz) - kSectionKeyCoordBias);
  outSectionY = static_cast<int>(us);
  return outSectionY >= 0 && outSectionY < kChunkSectionCount;
}

int sectionSampleIndex(int sx, int sy, int sz) {
  return sx + sz * kSectionSampleSize + sy * kSectionSampleSize * kSectionSampleSize;
}

constexpr int kCaveLightSampleMargin = 4;
constexpr int kCaveLightSampleSize = kSectionSampleSize + kCaveLightSampleMargin * 2;
constexpr size_t kCaveLightSampleCount =
  static_cast<size_t>(kCaveLightSampleSize * kCaveLightSampleSize * kCaveLightSampleSize);

int caveLightSampleIndex(int sx, int sy, int sz) {
  return sx + sz * kCaveLightSampleSize + sy * kCaveLightSampleSize * kCaveLightSampleSize;
}

bool isSkyLightPassable(uint8_t type) {
  return type == kAir || type == kSuspiciousGlass || isWaterBlock(type) || isDecorationBlock(type);
}

float surfaceSofteningMask(uint8_t biomeId, const BiomeClimateSample& climate) {
  float erosionSoft = smooth01((climate.erosion - 0.46f) / 0.24f);
  float weirdSoft = smooth01((0.60f - climate.weirdness) / 0.24f);
  float continentalSoft = smooth01((0.68f - climate.continentalness) / 0.20f);
  float biomeBias = (biomeId == 5 || biomeId == 6) ? 0.0f : (biomeId <= 1 ? 0.14f : 0.68f);
  return erosionSoft * weirdSoft * continentalSoft * biomeBias;
}

glm::vec3 blockColorForMesh(uint8_t type, bool aprilMode) {
  if (isWaterBlock(type)) {
    if (aprilMode) {
      return {0.34f, 0.92f, 0.66f};
    }
    return {0.22f, 0.45f, 0.88f};
  }
  if (isTorchBlock(type)) {
    return {0.98f, 0.78f, 0.26f};
  }
  if (isBedBlock(type)) {
    return {0.86f, 0.30f, 0.28f};
  }

  switch (type) {
    case kGrass:
      if (aprilMode) {
        return {0.96f, 0.27f, 0.73f};
      }
      return {0.2f, 0.8f, 0.2f};
    case kDirt:
      if (aprilMode) {
        return {0.69f, 0.31f, 0.52f};
      }
      return {0.55f, 0.35f, 0.2f};
    case kSand:
      if (aprilMode) {
        return {0.80f, 0.97f, 0.56f};
      }
      return {0.86f, 0.78f, 0.50f};
    case kGravel:
      if (aprilMode) {
        return {0.63f, 0.42f, 0.48f};
      }
      return {0.46f, 0.44f, 0.42f};
    case kSuspiciousGlass:
      if (aprilMode) {
        return {0.54f, 0.98f, 0.84f};
      }
      return {0.72f, 0.90f, 0.94f};
    case kWood:
      if (aprilMode) {
        return {0.63f, 0.34f, 0.74f};
      }
      return {0.49f, 0.33f, 0.16f};
    case kLeaves:
      if (aprilMode) {
        return {0.76f, 0.98f, 0.36f};
      }
      return {0.16f, 0.58f, 0.16f};
    case kSeagrass:
      if (aprilMode) {
        return {0.82f, 0.98f, 0.30f};
      }
      return {0.14f, 0.62f, 0.34f};
    case kCoral:
      if (aprilMode) {
        return {0.28f, 0.92f, 0.88f};
      }
      return {0.88f, 0.48f, 0.40f};
    case kCoalOre:
      return {0.22f, 0.22f, 0.22f};
    case kIronOre:
      return {0.73f, 0.54f, 0.40f};
    case kGoldOre:
      return {0.92f, 0.75f, 0.22f};
    case kDiamondOre:
      return {0.30f, 0.90f, 0.96f};
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
      if (aprilMode) {
        return {0.78f, 0.44f, 0.84f};
      }
      return {0.62f, 0.42f, 0.18f};
    case kPlanks:
      if (aprilMode) {
        return {0.90f, 0.58f, 0.84f};
      }
      return {0.78f, 0.58f, 0.34f};
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
      return {0.52f, 0.52f, 0.56f};
    case kLootCache:
      return {0.62f, 0.40f, 0.18f};
    case kStone:
      if (aprilMode) {
        return {0.56f, 0.29f, 0.24f};
      }
      return {0.6f, 0.6f, 0.6f};
    default:
      return {1.0f, 1.0f, 1.0f};
  }
}

int tileForBlockFace(uint8_t type, int axis, bool positive) {
  if (isBedBlock(type)) {
    return kTileBed;
  }
  if (type == kGrass) {
    if (axis == 1 && positive) {
      return kTileGrassTop;
    }
    if (axis == 1 && !positive) {
      return kTileDirt;
    }
    return kTileGrassSide;
  }
  if (isWaterBlock(type)) {
    return kTileWater;
  }
  if (type == kSand) {
    return kTileSand;
  }
  if (type == kGravel) {
    return kTileGravel;
  }
  if (type == kDirt) {
    return kTileDirt;
  }
  if (type == kSuspiciousGlass) {
    return kTileSuspiciousGlass;
  }
  if (type == kWood) {
    if (axis == 1) {
      return kTileWoodTop;
    }
    return kTileWood;
  }
  if (type == kLeaves) {
    return kTileLeaves;
  }
  if (type == kSeagrass) {
    return kTileSeagrass;
  }
  if (type == kCoral) {
    return kTileCoral;
  }
  if (type == kCoalOre) {
    return kTileCoalOre;
  }
  if (type == kIronOre) {
    return kTileIronOre;
  }
  if (type == kGoldOre) {
    return kTileGoldOre;
  }
  if (type == kDiamondOre) {
    return kTileDiamondOre;
  }
  if (isWorkbenchBlock(type)) {
    if (axis == 1 && positive) {
      return kTileWorkbench;
    }
    if (axis == 1 && !positive) {
      return kTilePlanks;
    }
    if (faceMatchesFacing(blockFacingIndex(type), axis, positive)) {
      return kTileWorkbench;
    }
    return kTilePlanks;
  }
  if (type == kPlanks) {
    return kTilePlanks;
  }
  if (isTorchBlock(type)) {
    return kTileTorch;
  }
  if (isFurnaceBlock(type)) {
    if (faceMatchesFacing(blockFacingIndex(type), axis, positive)) {
      return kTileFurnaceFront;
    }
    return kTileFurnace;
  }
  if (type == kLootCache) {
    return kTileLootCache;
  }
  return kTileStone;
}

glm::vec2 uvForAtlasTile(int tile, float u, float v) {
  float tileSizeU = 1.0f / static_cast<float>(kAtlasCols);
  float tileSizeV = 1.0f / static_cast<float>(kAtlasRows);
  int tx = tile % kAtlasCols;
  int ty = tile / kAtlasCols;
  float u0 = static_cast<float>(tx) * tileSizeU;
  float v0 = static_cast<float>(ty) * tileSizeV;
  float atlasWidth = static_cast<float>(kAtlasTileSize * kAtlasCols);
  float atlasHeight = static_cast<float>(kAtlasTileSize * kAtlasRows);
  float padU = 0.5f / atlasWidth;
  float padV = 0.5f / atlasHeight;
  float uMin = u0 + padU;
  float vMin = v0 + padV;
  float uMax = u0 + tileSizeU - padU;
  float vMax = v0 + tileSizeV - padV;
  float uClamped = uMin + u * (uMax - uMin);
  float vClamped = vMin + v * (vMax - vMin);
  return glm::vec2(uClamped, vClamped);
}

void appendQuad(std::vector<Vertex>& vertices,
                std::vector<uint32_t>& indices,
                const glm::vec3& v0,
                const glm::vec3& v1,
                const glm::vec3& v2,
                const glm::vec3& v3,
                const glm::vec3& color,
                int tile) {
  glm::vec2 uv0 = uvForAtlasTile(tile, 0.0f, 0.0f);
  glm::vec2 uv1 = uvForAtlasTile(tile, 1.0f, 0.0f);
  glm::vec2 uv2 = uvForAtlasTile(tile, 1.0f, 1.0f);
  glm::vec2 uv3 = uvForAtlasTile(tile, 0.0f, 1.0f);

  uint32_t startIndex = static_cast<uint32_t>(vertices.size());
  vertices.push_back({v0, color, uv0});
  vertices.push_back({v1, color, uv1});
  vertices.push_back({v2, color, uv2});
  vertices.push_back({v3, color, uv3});

  indices.push_back(startIndex + 0);
  indices.push_back(startIndex + 1);
  indices.push_back(startIndex + 2);
  indices.push_back(startIndex + 0);
  indices.push_back(startIndex + 2);
  indices.push_back(startIndex + 3);
}

void appendTorchDecoration(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           float fx,
                           float fy,
                           float fz,
                           uint8_t type,
                           const glm::vec3& color,
                           int tile) {
  glm::vec3 blockOrigin(fx, fy, fz);
  glm::vec3 baseCenter = blockOrigin + torchBaseOffset(type);
  glm::vec3 topCenter = blockOrigin + torchTopOffset(type);
  float halfWidth = isWallTorchBlock(type) ? 0.16f : 0.18f;
  constexpr float kDiag = 0.70710678f;
  glm::vec3 diag1(halfWidth * kDiag, 0.0f, halfWidth * kDiag);
  glm::vec3 diag2(halfWidth * kDiag, 0.0f, -halfWidth * kDiag);
  float tint = isWallTorchBlock(type) ? 1.08f : 1.04f;
  glm::vec3 torchColor = glm::min(color * tint, glm::vec3(1.0f));

  appendQuad(vertices,
             indices,
             baseCenter - diag1,
             topCenter - diag1,
             topCenter + diag1,
             baseCenter + diag1,
             torchColor,
             tile);
  appendQuad(vertices,
             indices,
             baseCenter - diag2,
             topCenter - diag2,
             topCenter + diag2,
             baseCenter + diag2,
             torchColor,
             tile);
}

void appendUnderwaterPlantDecoration(std::vector<Vertex>& vertices,
                                     std::vector<uint32_t>& indices,
                                     float fx,
                                     float fy,
                                     float fz,
                                     uint8_t type,
                                     const glm::vec3& color,
                                     int tile) {
  float baseY = fy + 0.02f;
  float topY = type == kSeagrass ? (fy + 0.86f) : (fy + 0.64f);
  float halfWidth = type == kSeagrass ? 0.20f : 0.24f;
  constexpr float kDiag = 0.70710678f;
  glm::vec3 center(fx + 0.5f, 0.0f, fz + 0.5f);
  glm::vec3 diag1(halfWidth * kDiag, 0.0f, halfWidth * kDiag);
  glm::vec3 diag2(halfWidth * kDiag, 0.0f, -halfWidth * kDiag);
  glm::vec3 plantColor = type == kCoral
    ? glm::min(color * glm::vec3(1.06f, 1.00f, 1.00f), glm::vec3(1.0f))
    : color;

  appendQuad(vertices,
             indices,
             {center.x - diag1.x, baseY, center.z - diag1.z},
             {center.x - diag1.x, topY, center.z - diag1.z},
             {center.x + diag1.x, topY, center.z + diag1.z},
             {center.x + diag1.x, baseY, center.z + diag1.z},
             plantColor,
             tile);
  appendQuad(vertices,
             indices,
             {center.x - diag2.x, baseY, center.z - diag2.z},
             {center.x - diag2.x, topY, center.z - diag2.z},
             {center.x + diag2.x, topY, center.z + diag2.z},
             {center.x + diag2.x, baseY, center.z + diag2.z},
             plantColor,
             tile);
}

void appendBedDecoration(std::vector<Vertex>& vertices,
                         std::vector<uint32_t>& indices,
                         float fx,
                         float fy,
                         float fz,
                         uint8_t type,
                         const glm::vec3& color,
                         int tile) {
  glm::ivec3 facingInt = bedFacingVector(type);
  glm::vec3 forward(static_cast<float>(facingInt.x), 0.0f, static_cast<float>(facingInt.z));
  glm::vec3 right(forward.z, 0.0f, -forward.x);
  glm::vec3 center(fx + 0.5f, fy, fz + 0.5f);

  auto localToWorld = [&](float lx, float ly, float lz) {
    return center + right * lx + glm::vec3(0.0f, ly, 0.0f) + forward * lz;
  };

  auto appendBox = [&](float minX,
                       float minY,
                       float minZ,
                       float maxX,
                       float maxY,
                       float maxZ,
                       const glm::vec3& boxColor,
                       int boxTile) {
    glm::vec3 p000 = localToWorld(minX, minY, minZ);
    glm::vec3 p001 = localToWorld(minX, minY, maxZ);
    glm::vec3 p010 = localToWorld(minX, maxY, minZ);
    glm::vec3 p011 = localToWorld(minX, maxY, maxZ);
    glm::vec3 p100 = localToWorld(maxX, minY, minZ);
    glm::vec3 p101 = localToWorld(maxX, minY, maxZ);
    glm::vec3 p110 = localToWorld(maxX, maxY, minZ);
    glm::vec3 p111 = localToWorld(maxX, maxY, maxZ);

    appendQuad(vertices, indices, p100, p110, p111, p101, boxColor * 0.84f, boxTile);
    appendQuad(vertices, indices, p001, p011, p010, p000, boxColor * 0.80f, boxTile);
    appendQuad(vertices, indices, p010, p011, p111, p110, boxColor * 1.04f, boxTile);
    appendQuad(vertices, indices, p000, p100, p101, p001, boxColor * 0.54f, boxTile);
    appendQuad(vertices, indices, p101, p111, p011, p001, boxColor * 0.92f, boxTile);
    appendQuad(vertices, indices, p000, p010, p110, p100, boxColor * 0.72f, boxTile);
  };

  const glm::vec3 frameColor = glm::min(color * glm::vec3(0.58f, 0.40f, 0.22f), glm::vec3(1.0f));
  const glm::vec3 blanketColor = glm::min(color * glm::vec3(1.04f, 1.00f, 1.00f), glm::vec3(1.0f));
  const glm::vec3 sheetColor(0.92f, 0.89f, 0.87f);
  const float legHeight = 0.26f;
  const float bedTop = 0.56f;

  appendBox(-0.46f, 0.20f, -0.48f, 0.46f, bedTop, 0.48f, blanketColor, tile);
  appendBox(-0.48f, 0.16f, -0.50f, 0.48f, 0.24f, 0.50f, frameColor, kTileWood);

  const std::array<glm::vec2, 4> legOffsets{{
    {-0.39f, -0.39f},
    {0.39f, -0.39f},
    {-0.39f, 0.39f},
    {0.39f, 0.39f}
  }};
  for (const glm::vec2& leg : legOffsets) {
    appendBox(leg.x - 0.05f, 0.0f, leg.y - 0.05f, leg.x + 0.05f, legHeight, leg.y + 0.05f, frameColor, kTileWood);
  }

  if (isBedHeadBlock(type)) {
    appendBox(-0.42f, bedTop, 0.04f, 0.42f, 0.62f, 0.42f, sheetColor, kTileUiWhite);
    appendBox(-0.48f, 0.22f, 0.44f, 0.48f, 0.82f, 0.50f, frameColor, kTileWood);
  } else {
    appendBox(-0.48f, 0.22f, -0.50f, 0.48f, 0.72f, -0.44f, frameColor, kTileWood);
  }
}

bool isFaceVisibleForType(uint8_t current, uint8_t neighbor) {
  if (isDecorationBlock(current)) {
    return false;
  }
  if (isWaterBlock(current)) {
    return neighbor == kAir || isDecorationBlock(neighbor);
  }
  return neighbor == kAir || isWaterBlock(neighbor) || isDecorationBlock(neighbor);
}

template <typename BlockSampler, typename SurfaceSampler>
void computeSectionSkyLightSamples(int baseX,
                                   int baseY,
                                   int baseZ,
                                   BlockSampler&& sampleBlock,
                                   SurfaceSampler&& sampleSurfaceHeight,
                                   std::array<float, kSectionSampleCount>& outSkyLightSamples) {
  outSkyLightSamples.fill(0.0f);

  std::array<uint8_t, kCaveLightSampleCount> caveLightBlocks{};
  caveLightBlocks.fill(kAir);
  std::array<uint8_t, kCaveLightSampleCount> caveLightLevels{};
  caveLightLevels.fill(0u);
  std::array<int, static_cast<size_t>(kCaveLightSampleSize * kCaveLightSampleSize)> surfaceHeights{};
  surfaceHeights.fill(0);

  for (int sz = 0; sz < kCaveLightSampleSize; ++sz) {
    for (int sx = 0; sx < kCaveLightSampleSize; ++sx) {
      int worldX = baseX + (sx - 1 - kCaveLightSampleMargin);
      int worldZ = baseZ + (sz - 1 - kCaveLightSampleMargin);
      surfaceHeights[static_cast<size_t>(sx + sz * kCaveLightSampleSize)] =
        sampleSurfaceHeight(worldX, worldZ);
    }
  }

  std::deque<int> lightQueue;
  for (int sz = 0; sz < kCaveLightSampleSize; ++sz) {
    for (int sx = 0; sx < kCaveLightSampleSize; ++sx) {
      int worldX = baseX + (sx - 1 - kCaveLightSampleMargin);
      int worldZ = baseZ + (sz - 1 - kCaveLightSampleMargin);
      int surfaceY = surfaceHeights[static_cast<size_t>(sx + sz * kCaveLightSampleSize)];
      for (int sy = 0; sy < kCaveLightSampleSize; ++sy) {
        int worldY = baseY + (sy - 1 - kCaveLightSampleMargin);
        int sampleIdx = caveLightSampleIndex(sx, sy, sz);
        uint8_t type = sampleBlock(worldX, worldY, worldZ);
        caveLightBlocks[static_cast<size_t>(sampleIdx)] = type;
        if (!isSkyLightPassable(type)) {
          continue;
        }
        if (worldY > surfaceY) {
          caveLightLevels[static_cast<size_t>(sampleIdx)] = 15u;
          lightQueue.push_back(sampleIdx);
        }
      }
    }
  }

  constexpr std::array<glm::ivec3, 6> kLightNeighbors = {{
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1}
  }};

  while (!lightQueue.empty()) {
    int index = lightQueue.front();
    lightQueue.pop_front();

    int sy = index / (kCaveLightSampleSize * kCaveLightSampleSize);
    int rem = index - sy * kCaveLightSampleSize * kCaveLightSampleSize;
    int sz = rem / kCaveLightSampleSize;
    int sx = rem - sz * kCaveLightSampleSize;

    uint8_t currentLevel = caveLightLevels[static_cast<size_t>(index)];
    if (currentLevel <= 2u) {
      continue;
    }
    uint8_t nextLevel = static_cast<uint8_t>(currentLevel - 2u);

    for (const glm::ivec3& offset : kLightNeighbors) {
      int nx = sx + offset.x;
      int ny = sy + offset.y;
      int nz = sz + offset.z;
      if (nx < 0 || nx >= kCaveLightSampleSize ||
          ny < 0 || ny >= kCaveLightSampleSize ||
          nz < 0 || nz >= kCaveLightSampleSize) {
        continue;
      }

      int neighborIdx = caveLightSampleIndex(nx, ny, nz);
      if (!isSkyLightPassable(caveLightBlocks[static_cast<size_t>(neighborIdx)])) {
        continue;
      }
      if (nextLevel <= caveLightLevels[static_cast<size_t>(neighborIdx)]) {
        continue;
      }

      caveLightLevels[static_cast<size_t>(neighborIdx)] = nextLevel;
      lightQueue.push_back(neighborIdx);
    }
  }

  for (int sz = 0; sz < kSectionSampleSize; ++sz) {
    for (int sx = 0; sx < kSectionSampleSize; ++sx) {
      for (int sy = 0; sy < kSectionSampleSize; ++sy) {
        int gx = sx + kCaveLightSampleMargin;
        int gy = sy + kCaveLightSampleMargin;
        int gz = sz + kCaveLightSampleMargin;
        int centerIdx = caveLightSampleIndex(gx, gy, gz);
        float direct = static_cast<float>(caveLightLevels[static_cast<size_t>(centerIdx)]) / 15.0f;
        float accum = 0.0f;
        float totalWeight = 0.0f;

        for (int oz = -1; oz <= 1; ++oz) {
          for (int ox = -1; ox <= 1; ++ox) {
            int manhattan = std::abs(ox) + std::abs(oz);
            float horizontalWeight = manhattan == 0 ? 1.35f : (manhattan == 1 ? 0.72f : 0.34f);
            for (int oy = 0; oy <= 1; ++oy) {
              int nx = gx + ox;
              int ny = gy + oy;
              int nz = gz + oz;
              if (nx < 0 || nx >= kCaveLightSampleSize ||
                  ny < 0 || ny >= kCaveLightSampleSize ||
                  nz < 0 || nz >= kCaveLightSampleSize) {
                continue;
              }

              int neighborIdx = caveLightSampleIndex(nx, ny, nz);
              if (!isSkyLightPassable(caveLightBlocks[static_cast<size_t>(neighborIdx)])) {
                continue;
              }

              float verticalWeight = oy == 0 ? 1.0f : 0.55f;
              float weight = horizontalWeight * verticalWeight;
              accum += (static_cast<float>(caveLightLevels[static_cast<size_t>(neighborIdx)]) / 15.0f) * weight;
              totalWeight += weight;
            }
          }
        }

        float neighborhood = totalWeight > 0.0f ? (accum / totalWeight) : direct;
        outSkyLightSamples[static_cast<size_t>(sectionSampleIndex(sx, sy, sz))] =
          std::clamp(direct * 0.62f + neighborhood * 0.58f, 0.0f, 1.0f);
      }
    }
  }
}

void buildSectionMeshFromSamples(int cx,
                                 int cz,
                                 int sectionY,
                                 const std::array<uint8_t, kSectionSampleCount>& samples,
                                 const std::array<float, kSectionSampleCount>& skyLightSamples,
                                 bool aprilMode,
                                 bool overlayActive,
                                 const glm::ivec3& overlayBlock,
                                 int overlayStage,
                                 std::vector<Vertex>& outVertices,
                                 std::vector<uint32_t>& outIndices) {
  outVertices.clear();
  outIndices.clear();

  auto sampleAt = [&](int sx, int sy, int sz) -> uint8_t {
    sx = std::clamp(sx, 0, kSectionSampleSize - 1);
    sy = std::clamp(sy, 0, kSectionSampleSize - 1);
    sz = std::clamp(sz, 0, kSectionSampleSize - 1);
    return samples[static_cast<size_t>(sectionSampleIndex(sx, sy, sz))];
  };

  auto sampleSkyLightAt = [&](int sx, int sy, int sz) -> float {
    sx = std::clamp(sx, 0, kSectionSampleSize - 1);
    sy = std::clamp(sy, 0, kSectionSampleSize - 1);
    sz = std::clamp(sz, 0, kSectionSampleSize - 1);
    return skyLightSamples[static_cast<size_t>(sectionSampleIndex(sx, sy, sz))];
  };

  auto sampleWaterSurfaceHeight = [&](int sx, int sy, int sz) -> float {
    uint8_t type = sampleAt(sx, sy, sz);
    if (isUnderwaterPlantBlock(type)) {
      return 1.0f;
    }
    if (!isWaterBlock(type)) {
      return 0.0f;
    }
    if (isWaterVolumeBlock(sampleAt(sx, sy + 1, sz))) {
      return 1.0f;
    }
    uint8_t level = waterLevelFromBlock(type);
    if (level == 255) {
      return 0.0f;
    }
    if (level == 0) {
      return 1.0f;
    }
    return std::clamp((8.0f - static_cast<float>(level)) / 8.0f, 0.125f, 1.0f);
  };

  auto computeWaterCornerHeight = [&](int sx, int sy, int sz, int ox, int oz) -> float {
    std::array<glm::ivec2, 4> offsets = {{
      {0, 0},
      {ox, 0},
      {0, oz},
      {ox, oz}
    }};
    float weightedSum = 0.0f;
    float totalWeight = 0.0f;
    bool foundWater = false;
    for (const glm::ivec2& offset : offsets) {
      int nx = sx + offset.x;
      int nz = sz + offset.y;
      uint8_t type = sampleAt(nx, sy, nz);
      if (isUnderwaterPlantBlock(type)) {
        foundWater = true;
        weightedSum += 1.0f;
        totalWeight += 1.0f;
        continue;
      }
      if (!isWaterBlock(type)) {
        continue;
      }
      if (isWaterVolumeBlock(sampleAt(nx, sy + 1, nz))) {
        return 1.0f;
      }
      foundWater = true;
      float weight = waterLevelFromBlock(type) == 0 ? 1.6f : 1.0f;
      weightedSum += sampleWaterSurfaceHeight(nx, sy, nz) * weight;
      totalWeight += weight;
    }
    if (!foundWater || totalWeight <= 0.0f) {
      return 0.0f;
    }
    return std::clamp(weightedSum / totalWeight, 0.0f, 1.0f);
  };

  auto computeWaterHeightsForCell = [&](int sx, int sy, int sz) -> std::array<float, 4> {
    std::array<float, 4> heights = {{
      computeWaterCornerHeight(sx, sy, sz, -1, -1), // NW
      computeWaterCornerHeight(sx, sy, sz, -1, 1),  // SW
      computeWaterCornerHeight(sx, sy, sz, 1, 1),   // SE
      computeWaterCornerHeight(sx, sy, sz, 1, -1)   // NE
    }};
    if (heights[0] <= 0.0f && heights[1] <= 0.0f &&
        heights[2] <= 0.0f && heights[3] <= 0.0f &&
        isWaterBlock(sampleAt(sx, sy, sz))) {
      float fallback = sampleWaterSurfaceHeight(sx, sy, sz);
      heights.fill(fallback);
    }
    return heights;
  };

  const float overlayOffset = 0.01f;
  int overlayTile = kBreakTileBase + std::clamp(overlayStage, 1, kBreakStages) - 1;

  int baseX = cx * kChunkSize;
  int baseY = sectionY * kChunkSectionSize;
  int baseZ = cz * kChunkSize;

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      for (int ly = 0; ly < kChunkSectionSize; ++ly) {
        int y = baseY + ly;
        if (y < 0 || y >= kChunkHeight) {
          continue;
        }

        uint8_t blockType = sampleAt(lx + 1, ly + 1, lz + 1);
        if (blockType == kAir) {
          continue;
        }

        int x = baseX + lx;
        int z = baseZ + lz;
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        float fz = static_cast<float>(z);
        float fx1 = fx + 1.0f;
        float fy1 = fy + 1.0f;
        float fz1 = fz + 1.0f;
        float heightFactor = 0.6f + 0.4f * (fy / (kChunkHeight - 1));

        bool isBreakTarget = overlayActive &&
                             overlayBlock.x == x &&
                             overlayBlock.y == y &&
                             overlayBlock.z == z;

        if (isBedBlock(blockType)) {
          int tile = tileForBlockFace(blockType, 1, true);
          float exposure = sampleSkyLightAt(lx + 1, ly + 2, lz + 1);
          float caveShade = 0.34f + exposure * 0.66f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) *
                            (0.84f + 0.16f * heightFactor) *
                            caveShade;
          appendBedDecoration(outVertices, outIndices, fx, fy, fz, blockType, color, tile);
          continue;
        }

        if (isDecorationBlock(blockType)) {
          int tile = tileForBlockFace(blockType, 1, true);
          float exposure = sampleSkyLightAt(lx + 1, ly + 2, lz + 1);
          float caveShade = 0.34f + exposure * 0.66f;
          if (isTorchBlock(blockType)) {
            caveShade = std::max(caveShade, 0.58f);
          }
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) *
                            (0.84f + 0.16f * heightFactor) *
                            caveShade;
          if (isTorchBlock(blockType)) {
            appendTorchDecoration(outVertices, outIndices, fx, fy, fz, blockType, color, tile);
          } else if (isUnderwaterPlantBlock(blockType)) {
            appendUnderwaterPlantDecoration(outVertices, outIndices, fx, fy, fz, blockType, color, tile);
          } else {
            float inset = 0.16f;
            appendQuad(outVertices,
                       outIndices,
                       {fx + inset, fy, fz + inset},
                       {fx + inset, fy1, fz + inset},
                       {fx1 - inset, fy1, fz1 - inset},
                       {fx1 - inset, fy, fz1 - inset},
                       color,
                       tile);
            appendQuad(outVertices,
                       outIndices,
                       {fx1 - inset, fy, fz + inset},
                       {fx1 - inset, fy1, fz + inset},
                       {fx + inset, fy1, fz1 - inset},
                       {fx + inset, fy, fz1 - inset},
                       color,
                       tile);
          }
          if (!isUnderwaterPlantBlock(blockType)) {
            continue;
          }
        }

        if (isWaterBlock(blockType) || isUnderwaterPlantBlock(blockType)) {
          uint8_t waterRenderType = isWaterBlock(blockType) ? blockType : kWater;
          int tile = tileForBlockFace(waterRenderType, 1, true);
          auto currentHeights = computeWaterHeightsForCell(lx + 1, ly + 1, lz + 1);
          float hNW = currentHeights[0];
          float hSW = currentHeights[1];
          float hSE = currentHeights[2];
          float hNE = currentHeights[3];

          auto appendWaterSide = [&](uint8_t neighborType,
                                     float lowerA,
                                     float lowerB,
                                     float upperA,
                                     float upperB,
                                     const glm::vec3& v0,
                                     const glm::vec3& v1,
                                     const glm::vec3& v2,
                                     const glm::vec3& v3,
                                     float shade,
                                     int axis,
                                     bool positive) {
            bool neighborPassable =
              neighborType == kAir ||
              isWaterVolumeBlock(neighborType) ||
              (isDecorationBlock(neighborType) && !isUnderwaterPlantBlock(neighborType));
            if (!neighborPassable) {
              return;
            }
            if (upperA <= lowerA + 0.01f && upperB <= lowerB + 0.01f) {
              return;
            }
            glm::vec3 color = blockColorForMesh(waterRenderType, aprilMode) * 0.82f * heightFactor * shade;
            appendQuad(outVertices,
                       outIndices,
                       v0,
                       v1,
                       v2,
                       v3,
                       color,
                       tileForBlockFace(waterRenderType, axis, positive));
          };

          if (!isWaterVolumeBlock(sampleAt(lx + 1, ly + 2, lz + 1))) {
            float caveShade = 0.22f + sampleSkyLightAt(lx + 1, ly + 2, lz + 1) * 0.78f;
            glm::vec3 color = blockColorForMesh(waterRenderType, aprilMode) * 1.02f * heightFactor * caveShade;
            appendQuad(outVertices,
                       outIndices,
                       {fx, fy + hNW, fz},
                       {fx, fy + hSW, fz1},
                       {fx1, fy + hSE, fz1},
                       {fx1, fy + hNE, fz},
                       color,
                       tile);
          }

          {
            uint8_t eastNeighbor = sampleAt(lx + 2, ly + 1, lz + 1);
            float lowerNorth = 0.0f;
            float lowerSouth = 0.0f;
            if (isWaterVolumeBlock(eastNeighbor)) {
              auto eastHeights = computeWaterHeightsForCell(lx + 2, ly + 1, lz + 1);
              lowerNorth = eastHeights[0];
              lowerSouth = eastHeights[1];
            }
            float caveShade = 0.16f + sampleSkyLightAt(lx + 2, ly + 1, lz + 1) * 0.84f;
            appendWaterSide(eastNeighbor,
                            lowerNorth,
                            lowerSouth,
                            hNE,
                            hSE,
                            {fx1, fy + lowerNorth, fz},
                            {fx1, fy + hNE, fz},
                            {fx1, fy + hSE, fz1},
                            {fx1, fy + lowerSouth, fz1},
                            caveShade,
                            0,
                            true);
          }

          {
            uint8_t westNeighbor = sampleAt(lx, ly + 1, lz + 1);
            float lowerNorth = 0.0f;
            float lowerSouth = 0.0f;
            if (isWaterVolumeBlock(westNeighbor)) {
              auto westHeights = computeWaterHeightsForCell(lx, ly + 1, lz + 1);
              lowerNorth = westHeights[3];
              lowerSouth = westHeights[2];
            }
            float caveShade = 0.16f + sampleSkyLightAt(lx, ly + 1, lz + 1) * 0.84f;
            appendWaterSide(westNeighbor,
                            lowerNorth,
                            lowerSouth,
                            hNW,
                            hSW,
                            {fx, fy + lowerNorth, fz},
                            {fx, fy + lowerSouth, fz1},
                            {fx, fy + hSW, fz1},
                            {fx, fy + hNW, fz},
                            caveShade,
                            0,
                            false);
          }

          {
            uint8_t southNeighbor = sampleAt(lx + 1, ly + 1, lz + 2);
            float lowerWest = 0.0f;
            float lowerEast = 0.0f;
            if (isWaterVolumeBlock(southNeighbor)) {
              auto southHeights = computeWaterHeightsForCell(lx + 1, ly + 1, lz + 2);
              lowerWest = southHeights[0];
              lowerEast = southHeights[3];
            }
            float caveShade = 0.16f + sampleSkyLightAt(lx + 1, ly + 1, lz + 2) * 0.84f;
            appendWaterSide(southNeighbor,
                            lowerWest,
                            lowerEast,
                            hSW,
                            hSE,
                            {fx, fy + lowerWest, fz1},
                            {fx1, fy + lowerEast, fz1},
                            {fx1, fy + hSE, fz1},
                            {fx, fy + hSW, fz1},
                            caveShade,
                            2,
                            true);
          }

          {
            uint8_t northNeighbor = sampleAt(lx + 1, ly + 1, lz);
            float lowerWest = 0.0f;
            float lowerEast = 0.0f;
            if (isWaterVolumeBlock(northNeighbor)) {
              auto northHeights = computeWaterHeightsForCell(lx + 1, ly + 1, lz);
              lowerWest = northHeights[1];
              lowerEast = northHeights[2];
            }
            float caveShade = 0.16f + sampleSkyLightAt(lx + 1, ly + 1, lz) * 0.84f;
            appendWaterSide(northNeighbor,
                            lowerWest,
                            lowerEast,
                            hNW,
                            hNE,
                            {fx, fy + lowerWest, fz},
                            {fx, fy + hNW, fz},
                            {fx1, fy + hNE, fz},
                            {fx1, fy + lowerEast, fz},
                            caveShade,
                            2,
                            false);
          }

          uint8_t belowNeighbor = sampleAt(lx + 1, ly, lz + 1);
          if (belowNeighbor == kAir ||
              (isDecorationBlock(belowNeighbor) && !isUnderwaterPlantBlock(belowNeighbor))) {
            float caveShade = 0.14f + sampleSkyLightAt(lx + 1, ly, lz + 1) * 0.72f;
            glm::vec3 color = blockColorForMesh(waterRenderType, aprilMode) * 0.52f * heightFactor * caveShade;
            appendQuad(outVertices,
                       outIndices,
                       {fx, fy, fz},
                       {fx1, fy, fz},
                       {fx1, fy, fz1},
                       {fx, fy, fz1},
                       color,
                       tileForBlockFace(waterRenderType, 1, false));
          }
          continue;
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx + 2, ly + 1, lz + 1))) {
          float caveShade = 0.16f + sampleSkyLightAt(lx + 2, ly + 1, lz + 1) * 0.84f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 0.8f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 0, true);
          appendQuad(outVertices,
                     outIndices,
                     {fx1, fy, fz},
                     {fx1, fy1, fz},
                     {fx1, fy1, fz1},
                     {fx1, fy, fz1},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(overlayOffset, 0.0f, 0.0f);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx1, fy, fz) + offset,
                       glm::vec3(fx1, fy1, fz) + offset,
                       glm::vec3(fx1, fy1, fz1) + offset,
                       glm::vec3(fx1, fy, fz1) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx, ly + 1, lz + 1))) {
          float caveShade = 0.16f + sampleSkyLightAt(lx, ly + 1, lz + 1) * 0.84f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 0.8f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 0, false);
          appendQuad(outVertices,
                     outIndices,
                     {fx, fy, fz},
                     {fx, fy, fz1},
                     {fx, fy1, fz1},
                     {fx, fy1, fz},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(-overlayOffset, 0.0f, 0.0f);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx, fy, fz) + offset,
                       glm::vec3(fx, fy, fz1) + offset,
                       glm::vec3(fx, fy1, fz1) + offset,
                       glm::vec3(fx, fy1, fz) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx + 1, ly + 2, lz + 1))) {
          float caveShade = 0.18f + sampleSkyLightAt(lx + 1, ly + 2, lz + 1) * 0.82f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 1.0f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 1, true);
          appendQuad(outVertices,
                     outIndices,
                     {fx, fy1, fz},
                     {fx, fy1, fz1},
                     {fx1, fy1, fz1},
                     {fx1, fy1, fz},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, overlayOffset, 0.0f);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx, fy1, fz) + offset,
                       glm::vec3(fx, fy1, fz1) + offset,
                       glm::vec3(fx1, fy1, fz1) + offset,
                       glm::vec3(fx1, fy1, fz) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx + 1, ly, lz + 1))) {
          float caveShade = 0.14f + sampleSkyLightAt(lx + 1, ly, lz + 1) * 0.72f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 0.5f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 1, false);
          appendQuad(outVertices,
                     outIndices,
                     {fx, fy, fz},
                     {fx1, fy, fz},
                     {fx1, fy, fz1},
                     {fx, fy, fz1},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, -overlayOffset, 0.0f);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx, fy, fz) + offset,
                       glm::vec3(fx1, fy, fz) + offset,
                       glm::vec3(fx1, fy, fz1) + offset,
                       glm::vec3(fx, fy, fz1) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx + 1, ly + 1, lz + 2))) {
          float caveShade = 0.16f + sampleSkyLightAt(lx + 1, ly + 1, lz + 2) * 0.84f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 0.8f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 2, true);
          appendQuad(outVertices,
                     outIndices,
                     {fx, fy, fz1},
                     {fx1, fy, fz1},
                     {fx1, fy1, fz1},
                     {fx, fy1, fz1},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, overlayOffset);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx, fy, fz1) + offset,
                       glm::vec3(fx1, fy, fz1) + offset,
                       glm::vec3(fx1, fy1, fz1) + offset,
                       glm::vec3(fx, fy1, fz1) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }

        if (isFaceVisibleForType(blockType, sampleAt(lx + 1, ly + 1, lz))) {
          float caveShade = 0.16f + sampleSkyLightAt(lx + 1, ly + 1, lz) * 0.84f;
          glm::vec3 color = blockColorForMesh(blockType, aprilMode) * 0.8f * heightFactor * caveShade;
          int tile = tileForBlockFace(blockType, 2, false);
          appendQuad(outVertices,
                     outIndices,
                     {fx, fy, fz},
                     {fx, fy1, fz},
                     {fx1, fy1, fz},
                     {fx1, fy, fz},
                     color,
                     tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, -overlayOffset);
            appendQuad(outVertices,
                       outIndices,
                       glm::vec3(fx, fy, fz) + offset,
                       glm::vec3(fx, fy1, fz) + offset,
                       glm::vec3(fx1, fy1, fz) + offset,
                       glm::vec3(fx1, fy, fz) + offset,
                       glm::vec3(1.0f),
                       overlayTile);
          }
        }
      }
    }
  }
}

uint32_t nextRng(uint32_t& state) {
  if (state == 0u) {
    state = 0xA341316Cu;
  }
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

int randIntInclusive(uint32_t& state, int minValue, int maxValue) {
  if (maxValue <= minValue) {
    return minValue;
  }
  uint32_t span = static_cast<uint32_t>(maxValue - minValue + 1);
  return minValue + static_cast<int>(nextRng(state) % span);
}

float rand01(uint32_t& state) {
  return static_cast<float>(nextRng(state) & 0x00ffffffu) / 16777215.0f;
}

int hashChunkSeed(int seed, int cx, int cz, uint32_t salt) {
  uint32_t h = static_cast<uint32_t>(seed) * 0x9E3779B9u;
  h ^= mixBits(static_cast<uint32_t>(cx) * 0x85EBCA6Bu);
  h ^= mixBits(static_cast<uint32_t>(cz) * 0xC2B2AE35u);
  h ^= salt;
  return static_cast<int>(mixBits(h));
}

struct V02WorldTuning {
  float mountainScale = 1.0f;
  float caveDensity = 1.0f;
  float ravineFrequency = 1.0f;
  float treeSpawnThreshold = 0.84f;
  int oreAttemptsCoal = 20;
  int oreAttemptsIron = 20;
  int oreAttemptsGold = 2;
  int oreAttemptsDiamond = 1;
  int oreVeinCoalMin = 12;
  int oreVeinCoalMax = 17;
  int oreVeinIronMin = 6;
  int oreVeinIronMax = 9;
  int oreVeinGoldMin = 6;
  int oreVeinGoldMax = 9;
  int oreVeinDiamondMin = 4;
  int oreVeinDiamondMax = 6;
  int oreCoalMinY = 0;
  int oreCoalMaxY = kChunkHeight - 1;
  int oreIronMinY = 0;
  int oreIronMaxY = 63;
  int oreGoldMinY = 0;
  int oreGoldMaxY = 31;
  int oreDiamondMinY = 0;
  int oreDiamondMaxY = 18;
};

V02WorldTuning tuningForV02(const WorldGenSettings& settings) {
  V02WorldTuning tuning{};
  tuning.caveDensity = std::clamp(settings.caveDensity, 0.25f, 2.5f);
  tuning.ravineFrequency = std::clamp(settings.ravineFrequency, 0.25f, 2.5f);

  // v0.2 smoke tuning targets a balanced baseline for terrain density.
  if (settings.preset == WorldPreset::kClassicFlat) {
    tuning.mountainScale = 0.0f;
    tuning.treeSpawnThreshold = 0.90f;
    tuning.oreAttemptsCoal = 18;
    tuning.oreAttemptsIron = 18;
    tuning.oreAttemptsGold = 2;
    tuning.oreAttemptsDiamond = 1;
  }

  return tuning;
}

struct TerrainRouterSample {
  float baseHeight = 0.0f;
  float finalHeight = 0.0f;
  float factor = 0.11f;
  float jaggedness = 0.0f;
  float peakMask = 0.0f;
  float valleyMask = 0.0f;
  float plateauMask = 0.0f;
};

[[maybe_unused]] std::vector<uint8_t> generateChunkBlocks(int cx,
                                                          int cz,
                                                          int seed,
                                                          const WorldGenSettings& settings) {
  std::vector<uint8_t> blocks(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  constexpr int kSeaLevel = 32;
  constexpr int kFlatSurfaceY = 40;
  const V02WorldTuning tuning = tuningForV02(settings);
  const float seedF = static_cast<float>(seed);

  enum class BiomeType : uint8_t {
    kOcean = 0,
    kBeach = 1,
    kPlains = 2,
    kForest = 3,
    kDesert = 4,
    kMountains = 5
  };

  std::array<BiomeType, kChunkSize * kChunkSize> columnBiomes{};
  columnBiomes.fill(BiomeType::kPlains);
  std::array<float, kChunkSize * kChunkSize> columnContinentalness{};
  columnContinentalness.fill(0.55f);
  std::array<float, kChunkSize * kChunkSize> columnTemperature{};
  columnTemperature.fill(0.55f);
  std::array<float, kChunkSize * kChunkSize> columnHumidity{};
  columnHumidity.fill(0.50f);
  std::array<int, kChunkSize * kChunkSize> columnTargetHeights{};
  columnTargetHeights.fill(kSeaLevel + 8);

  auto columnIndex = [](int lx, int lz) {
    return static_cast<size_t>(lx + lz * kChunkSize);
  };

  // Stage 1: climate + biome classification + 3D density terrain.
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      size_t idx2d = columnIndex(lx, lz);

      if (settings.preset == WorldPreset::kClassicFlat) {
        columnBiomes[idx2d] = BiomeType::kPlains;
        columnContinentalness[idx2d] = 0.62f;
        columnTemperature[idx2d] = 0.58f;
        columnHumidity[idx2d] = 0.52f;
        columnTargetHeights[idx2d] = kFlatSurfaceY;

        for (int y = 0; y <= kFlatSurfaceY; ++y) {
          blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kStone;
        }
        continue;
      }

      // Domain warp to reduce obvious grid-like biome seams.
      float warpX = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 101.0f) * 0.0016f,
        (static_cast<float>(worldZ) - seedF * 89.0f) * 0.0016f));
      float warpZ = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) - seedF * 37.0f) * 0.0016f,
        (static_cast<float>(worldZ) + seedF * 53.0f) * 0.0016f));
      float wx = static_cast<float>(worldX) + warpX * 24.0f;
      float wz = static_cast<float>(worldZ) + warpZ * 24.0f;

      // Climate parameter noises (temperature/humidity/continentalness/erosion/weirdness).
      float temperature = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx + seedF * 17.0f) * 0.00095f,
        (wz - seedF * 11.0f) * 0.00095f));
      float humidity = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx - seedF * 7.0f) * 0.00105f,
        (wz + seedF * 13.0f) * 0.00105f));
      float continentalness = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx + static_cast<float>(seed) * 11.0f) * 0.00045f,
        (wz - static_cast<float>(seed) * 13.0f) * 0.00045f));
      float dxFromOrigin = static_cast<float>(worldX);
      float dzFromOrigin = static_cast<float>(worldZ);
      float distFromOrigin = std::sqrt(dxFromOrigin * dxFromOrigin + dzFromOrigin * dzFromOrigin);
      float spawnLandBias = smooth01(1.0f - distFromOrigin / 640.0f) * 0.20f;
      continentalness = std::clamp(continentalness + 0.08f + spawnLandBias, 0.0f, 1.0f);
      float erosion = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx - seedF * 37.0f) * 0.00120f,
        (wz + seedF * 23.0f) * 0.00120f));
      float weirdness = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx + seedF * 61.0f) * 0.00185f,
        (wz - seedF * 53.0f) * 0.00185f));
      float macroRelief = glm::perlin(glm::vec2(
        (wx - static_cast<float>(seed) * 3.0f) * 0.0013f,
        (wz + static_cast<float>(seed) * 7.0f) * 0.0013f));
      float detailNoise = fbmNoise(wx, wz, seed);
      BiomeType biome = BiomeType::kPlains;
      if (continentalness < 0.25f) {
        biome = BiomeType::kOcean;
      } else if (continentalness < 0.31f) {
        biome = BiomeType::kBeach;
      } else if (temperature > 0.72f && humidity < 0.34f) {
        biome = BiomeType::kDesert;
      } else if (weirdness > 0.67f && continentalness > 0.56f) {
        biome = BiomeType::kMountains;
      } else if (humidity > 0.58f) {
        biome = BiomeType::kForest;
      }

      columnBiomes[idx2d] = biome;
      columnContinentalness[idx2d] = continentalness;
      columnTemperature[idx2d] = temperature;
      columnHumidity[idx2d] = humidity;

      float biomeBase = static_cast<float>(kSeaLevel) - 6.0f +
                        continentalness * 34.0f +
                        macroRelief * 7.0f +
                        detailNoise * 4.5f -
                        (1.0f - erosion) * 5.5f;

      if (biome == BiomeType::kOcean) {
        biomeBase = static_cast<float>(kSeaLevel) - 3.0f + macroRelief * 2.5f;
      } else if (biome == BiomeType::kBeach) {
        biomeBase = static_cast<float>(kSeaLevel) + 1.2f + macroRelief * 1.8f;
      } else if (biome == BiomeType::kDesert) {
        biomeBase += 2.2f + detailNoise * 2.0f;
      } else if (biome == BiomeType::kForest) {
        biomeBase += 2.0f + detailNoise * 1.2f;
      } else if (biome == BiomeType::kMountains) {
        float mountainBoost = (12.0f + 46.0f * weirdness * weirdness) * tuning.mountainScale;
        biomeBase += mountainBoost;
      }

      int targetHeight = std::clamp(static_cast<int>(std::round(biomeBase)), 6, kChunkHeight - 6);
      columnTargetHeights[idx2d] = targetHeight;

      blocks[static_cast<size_t>(chunkLocalIndex(lx, 0, lz))] = kStone;
      for (int y = 1; y < kChunkHeight - 1; ++y) {
        float densityBase = (static_cast<float>(targetHeight) - static_cast<float>(y)) * 0.096f;
        float bodyNoise = glm::perlin(glm::vec3(
          (wx + seedF * 5.0f) * 0.043f,
          (static_cast<float>(y) - seedF * 3.0f) * 0.058f,
          (wz - seedF * 7.0f) * 0.043f));
        float detail3d = glm::perlin(glm::vec3(
          (wx - seedF * 31.0f) * 0.089f,
          (static_cast<float>(y) + seedF * 11.0f) * 0.103f,
          (wz + seedF * 29.0f) * 0.089f));
        float ridge = 1.0f - std::abs(glm::perlin(glm::vec3(
          (wx + seedF * 13.0f) * 0.021f,
          static_cast<float>(y) * 0.027f,
          (wz - seedF * 17.0f) * 0.021f)));
        float ridgeBoost = (biome == BiomeType::kMountains) ? ridge * 0.52f : ridge * 0.18f;
        float density = densityBase + bodyNoise * 0.82f + detail3d * 0.36f + ridgeBoost;
        if (y < 4) {
          density += 1.3f;
        }
        if (density > 0.0f) {
          blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kStone;
        }
      }
    }
  }

  // Stage 2: cave and canyon carvers.
  if (settings.preset != WorldPreset::kClassicFlat) {
    for (int lz = 0; lz < kChunkSize; ++lz) {
      for (int lx = 0; lx < kChunkSize; ++lx) {
        int worldX = baseX + lx;
        int worldZ = baseZ + lz;
        size_t idx2d = columnIndex(lx, lz);
        int targetHeight = columnTargetHeights[idx2d];

        float caveDensity = tuning.caveDensity;
        float caveRegion = glm::perlin(glm::vec2(
          (static_cast<float>(worldX) + seedF * 41.0f) * 0.0022f,
          (static_cast<float>(worldZ) - seedF * 37.0f) * 0.0022f));
        float caveRegionThreshold = std::clamp(0.60f - 0.11f * (caveDensity - 1.0f), 0.40f, 0.78f);
        bool allowCavesHere = caveRegion > caveRegionThreshold;

        int caveTop = std::clamp(targetHeight + 18, 18, kChunkHeight - 8);
        for (int y = 6; y < caveTop; ++y) {
          size_t idx = static_cast<size_t>(chunkLocalIndex(lx, y, lz));
          if (blocks[idx] == kAir || isWaterBlock(blocks[idx])) {
            continue;
          }
          if (!allowCavesHere) {
            continue;
          }

          float caveA = glm::perlin(glm::vec3(
            (static_cast<float>(worldX) + seedF * 31.0f) * 0.062f,
            (static_cast<float>(y) - seedF * 13.0f) * 0.070f,
            (static_cast<float>(worldZ) - seedF * 29.0f) * 0.062f));
          float caveB = glm::perlin(glm::vec3(
            (static_cast<float>(worldX) - seedF * 7.0f) * 0.122f,
            (static_cast<float>(y) + seedF * 19.0f) * 0.122f,
            (static_cast<float>(worldZ) + seedF * 23.0f) * 0.122f));
          float tubeShape = std::abs(caveA) + 0.60f * std::abs(caveB);
          float depthT = saturate(static_cast<float>(targetHeight - y) / 54.0f);
          float caveThreshold = (0.050f + 0.018f * depthT) *
                                std::clamp(caveDensity, 0.45f, 1.85f);
          if (tubeShape < caveThreshold) {
            blocks[idx] = kAir;
          }
        }

        float ravineRegion = 0.5f + 0.5f * glm::perlin(glm::vec2(
          (static_cast<float>(worldX) - seedF * 47.0f) * 0.00085f,
          (static_cast<float>(worldZ) + seedF * 53.0f) * 0.00085f));
        float ravineLineA = 1.0f - std::abs(glm::perlin(glm::vec2(
          (static_cast<float>(worldX) + seedF * 61.0f) * 0.0017f,
          (static_cast<float>(worldZ) - seedF * 59.0f) * 0.0017f)));
        float ravineLineB = 1.0f - std::abs(glm::perlin(glm::vec2(
          (static_cast<float>(worldX) - seedF * 23.0f) * 0.0031f,
          (static_cast<float>(worldZ) + seedF * 19.0f) * 0.0031f)));
        float ravineLine = 0.72f * ravineLineA + 0.28f * ravineLineB;
        float ravineCore = smooth01((ravineLine - 0.962f) / 0.038f);
        float ravineChance = smooth01((ravineRegion - 0.72f) / 0.28f);
        float ravineFrequency = tuning.ravineFrequency;
        float ravineMask = std::clamp(ravineCore * ravineChance * ravineFrequency, 0.0f, 1.0f);
        float ravineMaskThreshold = std::clamp(0.18f - 0.07f * (ravineFrequency - 1.0f), 0.08f, 0.30f);

        if (ravineMask > ravineMaskThreshold && targetHeight > 24) {
          float depthNoise = 0.5f + 0.5f * glm::perlin(glm::vec2(
            (static_cast<float>(worldX) - seedF * 13.0f) * 0.0062f,
            (static_cast<float>(worldZ) + seedF * 7.0f) * 0.0062f));
          int ravineDepth = static_cast<int>(14.0f + ravineMask * 36.0f + depthNoise * 12.0f);
          ravineDepth = std::clamp(ravineDepth, 14, 62);
          int bottomY = std::max(4, targetHeight - ravineDepth);
          int topY = std::min(kChunkHeight - 2, targetHeight + 3);

          for (int y = topY; y >= bottomY; --y) {
            size_t idx = static_cast<size_t>(chunkLocalIndex(lx, y, lz));
            if (blocks[idx] == kAir || isWaterBlock(blocks[idx])) {
              continue;
            }

            float depthT = static_cast<float>(topY - y) /
                           static_cast<float>(std::max(1, topY - bottomY));
            float wallThreshold = 0.13f + depthT * 0.62f;
            float wallRough = glm::perlin(glm::vec3(
              (static_cast<float>(worldX) + seedF * 3.0f) * 0.085f,
              (static_cast<float>(y) - seedF * 7.0f) * 0.11f,
              (static_cast<float>(worldZ) - seedF * 5.0f) * 0.085f));
            float carveStrength = ravineMask + wallRough * 0.09f;
            if (carveStrength > wallThreshold) {
              blocks[idx] = kAir;
            }
          }
        }
      }
    }
  }

  // Stage 3: flood fill up to sea level.
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      for (int y = 1; y < kSeaLevel; ++y) {
        size_t idx = static_cast<size_t>(chunkLocalIndex(lx, y, lz));
        if (blocks[idx] == kAir) {
          blocks[idx] = kWater;
        }
      }
    }
  }

  auto getLocalBlock = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize ||
        lz < 0 || lz >= kChunkSize ||
        y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocalBlock = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize ||
        lz < 0 || lz >= kChunkSize ||
        y < 0 || y >= kChunkHeight) {
      return;
    }
    blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto placeOreVein = [&](uint8_t oreType,
                          int startX,
                          int startY,
                          int startZ,
                          int veinSize,
                          uint32_t& rngState) {
    if (veinSize <= 0) {
      return;
    }

    constexpr float kPi = 3.14159265358979323846f;
    float angle = rand01(rngState) * kPi;
    float deltaX = std::sin(angle) * (static_cast<float>(veinSize) / 8.0f);
    float deltaZ = std::cos(angle) * (static_cast<float>(veinSize) / 8.0f);

    float x1 = static_cast<float>(startX) + deltaX;
    float x2 = static_cast<float>(startX) - deltaX;
    float z1 = static_cast<float>(startZ) + deltaZ;
    float z2 = static_cast<float>(startZ) - deltaZ;
    float y1 = static_cast<float>(startY) + static_cast<float>(randIntInclusive(rngState, -2, 2));
    float y2 = static_cast<float>(startY) + static_cast<float>(randIntInclusive(rngState, -2, 2));

    int steps = std::max(1, veinSize);
    for (int step = 0; step < steps; ++step) {
      float t = (steps == 1) ? 0.0f : static_cast<float>(step) / static_cast<float>(steps - 1);
      float cx = x1 + (x2 - x1) * t;
      float cy = y1 + (y2 - y1) * t;
      float cz = z1 + (z2 - z1) * t;

      float sizeNoise = rand01(rngState) * static_cast<float>(veinSize) / 16.0f;
      float radius = ((std::sin(t * kPi) + 1.0f) * sizeNoise + 1.0f) * 0.5f;
      if (radius <= 0.0f) {
        continue;
      }

      int minX = std::max(0, static_cast<int>(std::floor(cx - radius)));
      int maxX = std::min(kChunkSize - 1, static_cast<int>(std::floor(cx + radius)));
      int minY = std::max(1, static_cast<int>(std::floor(cy - radius)));
      int maxY = std::min(kChunkHeight - 2, static_cast<int>(std::floor(cy + radius)));
      int minZ = std::max(0, static_cast<int>(std::floor(cz - radius)));
      int maxZ = std::min(kChunkSize - 1, static_cast<int>(std::floor(cz + radius)));

      for (int x = minX; x <= maxX; ++x) {
        float nx = (static_cast<float>(x) + 0.5f - cx) / radius;
        float nx2 = nx * nx;
        if (nx2 >= 1.0f) {
          continue;
        }
        for (int y = minY; y <= maxY; ++y) {
          float ny = (static_cast<float>(y) + 0.5f - cy) / radius;
          float ny2 = ny * ny;
          if (nx2 + ny2 >= 1.0f) {
            continue;
          }
          for (int z = minZ; z <= maxZ; ++z) {
            float nz = (static_cast<float>(z) + 0.5f - cz) / radius;
            float distance = nx2 + ny2 + nz * nz;
            if (distance >= 1.0f) {
              continue;
            }
            if (getLocalBlock(x, y, z) == kStone) {
              setLocalBlock(x, y, z, oreType);
            }
          }
        }
      }
    }
  };

  struct OreVeinRule {
    uint8_t oreType = kStone;
    uint32_t salt = 0;
    int attemptsPerChunk = 0;
    int minY = 0;
    int maxY = 0;
    int minVeinSize = 0;
    int maxVeinSize = 0;
  };

  std::array<OreVeinRule, 4> oreRules = {{
    {kCoalOre,
     0xC011u,
     tuning.oreAttemptsCoal,
     tuning.oreCoalMinY,
     tuning.oreCoalMaxY,
     tuning.oreVeinCoalMin,
     tuning.oreVeinCoalMax},
    {kIronOre,
     0x1A2Bu,
     tuning.oreAttemptsIron,
     tuning.oreIronMinY,
     tuning.oreIronMaxY,
     tuning.oreVeinIronMin,
     tuning.oreVeinIronMax},
    {kGoldOre,
     0x90D1u,
     tuning.oreAttemptsGold,
     tuning.oreGoldMinY,
     tuning.oreGoldMaxY,
     tuning.oreVeinGoldMin,
     tuning.oreVeinGoldMax},
    {kDiamondOre,
     0xD14Du,
     tuning.oreAttemptsDiamond,
     tuning.oreDiamondMinY,
     tuning.oreDiamondMaxY,
     tuning.oreVeinDiamondMin,
     tuning.oreVeinDiamondMax}
  }};

  for (const OreVeinRule& rule : oreRules) {
    if (rule.attemptsPerChunk <= 0) {
      continue;
    }
    int minOreY = std::clamp(rule.minY, 1, kChunkHeight - 2);
    int maxOreY = std::clamp(rule.maxY, 1, kChunkHeight - 2);
    if (maxOreY < minOreY) {
      std::swap(minOreY, maxOreY);
    }

    int minVeinSize = std::max(1, rule.minVeinSize);
    int maxVeinSize = std::max(minVeinSize, rule.maxVeinSize);

    uint32_t rngState = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, rule.salt));
    for (int attempt = 0; attempt < rule.attemptsPerChunk; ++attempt) {
      int startX = randIntInclusive(rngState, 0, kChunkSize - 1);
      int startZ = randIntInclusive(rngState, 0, kChunkSize - 1);
      int startY = randIntInclusive(rngState, minOreY, maxOreY);
      int veinSize = randIntInclusive(rngState, minVeinSize, maxVeinSize);
      placeOreVein(rule.oreType, startX, startY, startZ, veinSize, rngState);
    }
  }

  std::array<int, kChunkSize * kChunkSize> surfaceHeights{};
  surfaceHeights.fill(-1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      int top = -1;
      for (int y = kChunkHeight - 2; y >= 1; --y) {
        uint8_t type = getLocalBlock(lx, y, lz);
        if (type == kAir || isWaterBlock(type) || type == kLeaves) {
          continue;
        }
        top = y;
        break;
      }
      surfaceHeights[static_cast<size_t>(lx + lz * kChunkSize)] = top;
    }
  }

  // Stage 4: biome surface layers (grass/sand/gravel etc.).
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t index = static_cast<size_t>(lx + lz * kChunkSize);
      int surfaceY = surfaceHeights[index];
      if (surfaceY < 1 || surfaceY >= kChunkHeight - 2) {
        continue;
      }

      BiomeType biome = columnBiomes[index];
      uint8_t topBlock = kGrass;
      uint8_t fillerBlock = kDirt;
      int fillerDepth = 4;

      if (biome == BiomeType::kOcean || biome == BiomeType::kBeach || biome == BiomeType::kDesert) {
        topBlock = kSand;
        fillerBlock = kSand;
        fillerDepth = 4;
      } else if (biome == BiomeType::kMountains) {
        if (surfaceY > kSeaLevel + 26) {
          topBlock = kStone;
          fillerBlock = kStone;
          fillerDepth = 2;
        } else if (surfaceY > kSeaLevel + 18) {
          topBlock = kGrass;
          fillerBlock = kGravel;
          fillerDepth = 3;
        } else {
          topBlock = kGrass;
          fillerBlock = kDirt;
          fillerDepth = 3;
        }
      }

      if (surfaceY <= kSeaLevel + 1) {
        topBlock = kSand;
        fillerBlock = kSand;
        fillerDepth = std::max(fillerDepth, 3);
      }

      setLocalBlock(lx, surfaceY, lz, topBlock);
      for (int depth = 1; depth < fillerDepth; ++depth) {
        int y = surfaceY - depth;
        if (y < 1) {
          break;
        }
        uint8_t current = getLocalBlock(lx, y, lz);
        if (current == kAir || isWaterBlock(current) || current == kLeaves || current == kWood) {
          break;
        }
        if (current == kStone || current == kDirt || current == kGrass ||
            current == kSand || current == kGravel) {
          setLocalBlock(lx, y, lz, fillerBlock);
        }
      }
    }
  }

  // Keep beaches narrow by painting sand only very close to coastlines.
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t index = static_cast<size_t>(lx + lz * kChunkSize);
      int surfaceY = surfaceHeights[index];
      if (surfaceY < kSeaLevel - 1 || surfaceY > kSeaLevel + 3) {
        continue;
      }

      uint8_t surfaceType = getLocalBlock(lx, surfaceY, lz);
      if (surfaceType != kGrass && surfaceType != kDirt && surfaceType != kSand) {
        continue;
      }

      int nearestWaterDistance = 4;
      for (int oz = -3; oz <= 3; ++oz) {
        for (int ox = -3; ox <= 3; ++ox) {
          int distance = std::max(std::abs(ox), std::abs(oz));
          if (distance == 0 || distance > 3) {
            continue;
          }

          int nx = lx + ox;
          int nz = lz + oz;
          if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) {
            continue;
          }

          int neighborTop = surfaceHeights[static_cast<size_t>(nx + nz * kChunkSize)];
          uint8_t seaBlock = getLocalBlock(nx, kSeaLevel - 1, nz);
          if (neighborTop < kSeaLevel && isWaterBlock(seaBlock)) {
            nearestWaterDistance = std::min(nearestWaterDistance, distance);
          }
        }
      }

      if (nearestWaterDistance > 3) {
        continue;
      }

      int sandDepth = std::clamp(4 - nearestWaterDistance, 1, 3);
      for (int depth = 0; depth < sandDepth; ++depth) {
        int y = surfaceY - depth;
        if (y < 1) {
          break;
        }

        uint8_t current = getLocalBlock(lx, y, lz);
        if (current == kAir || isWaterBlock(current) || current == kLeaves || current == kWood) {
          break;
        }
        if (current == kGrass || current == kDirt || current == kSand ||
            current == kGravel || current == kStone) {
          setLocalBlock(lx, y, lz, kSand);
        }
      }
    }
  }

  std::array<uint8_t, kChunkSize * kChunkSize> treePlaced{};
  treePlaced.fill(0);
  constexpr std::array<std::pair<int, int>, 4> kNeighborOffsets = {{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1}
  }};

  for (int lz = 2; lz < kChunkSize - 2; ++lz) {
    for (int lx = 2; lx < kChunkSize - 2; ++lx) {
      size_t surfaceIndex = static_cast<size_t>(lx + lz * kChunkSize);
      int surfaceY = surfaceHeights[surfaceIndex];
      if (surfaceY < 2 || surfaceY >= kChunkHeight - 8) {
        continue;
      }

      BiomeType biome = columnBiomes[surfaceIndex];
      if (biome == BiomeType::kOcean || biome == BiomeType::kBeach || biome == BiomeType::kDesert) {
        continue;
      }
      if (biome == BiomeType::kMountains && surfaceY > kSeaLevel + 24) {
        continue;
      }

      uint8_t ground = getLocalBlock(lx, surfaceY, lz);
      if (ground != kGrass && ground != kDirt) {
        continue;
      }
      if (surfaceY <= kSeaLevel + 1) {
        continue;
      }

      int maxSlope = 0;
      bool neighborsValid = true;
      for (const auto& [ox, oz] : kNeighborOffsets) {
        int nx = lx + ox;
        int nz = lz + oz;
        int nTop = surfaceHeights[static_cast<size_t>(nx + nz * kChunkSize)];
        if (nTop < 0) {
          neighborsValid = false;
          break;
        }
        maxSlope = std::max(maxSlope, std::abs(nTop - surfaceY));
      }
      if (!neighborsValid || maxSlope > 2) {
        continue;
      }

      bool tooCloseToTree = false;
      for (int oz = -1; oz <= 1 && !tooCloseToTree; ++oz) {
        for (int ox = -1; ox <= 1; ++ox) {
          int nx = lx + ox;
          int nz = lz + oz;
          if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) {
            continue;
          }
          if (treePlaced[static_cast<size_t>(nx + nz * kChunkSize)] != 0) {
            tooCloseToTree = true;
            break;
          }
        }
      }
      if (tooCloseToTree) {
        continue;
      }

      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      float biomeCluster = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 17.0f) * 0.028f,
        (static_cast<float>(worldZ) - seedF * 19.0f) * 0.028f));
      float spawnNoise = hashedNoise01(worldX, surfaceY, worldZ, seed, 0x7AEEu);
      float spawnScore = spawnNoise * (0.64f + 0.36f * biomeCluster);

      float treeThreshold = tuning.treeSpawnThreshold;
      if (biome == BiomeType::kForest) {
        treeThreshold -= 0.24f;
      } else if (biome == BiomeType::kPlains) {
        treeThreshold += 0.04f;
      } else if (biome == BiomeType::kMountains) {
        treeThreshold += 0.16f;
      }
      float aridity = columnTemperature[surfaceIndex] - columnHumidity[surfaceIndex];
      treeThreshold += std::max(0.0f, aridity) * 0.06f;
      treeThreshold -= std::max(0.0f, columnContinentalness[surfaceIndex] - 0.58f) * 0.05f;
      treeThreshold -= (columnHumidity[surfaceIndex] - 0.5f) * 0.10f;
      treeThreshold = std::clamp(treeThreshold, 0.45f, 0.96f);
      if (spawnScore < treeThreshold) {
        continue;
      }

      int trunkHeight = 4 + static_cast<int>(
        hashedNoise01(worldX, surfaceY + 11, worldZ, seed, 0x51A7u) * 3.0f);
      trunkHeight = std::clamp(trunkHeight, 4, 6);
      if (surfaceY + trunkHeight + 2 >= kChunkHeight) {
        continue;
      }

      bool clearForTrunk = true;
      for (int y = surfaceY + 1; y <= surfaceY + trunkHeight; ++y) {
        uint8_t type = getLocalBlock(lx, y, lz);
        if (type != kAir && !isWaterBlock(type) && type != kLeaves) {
          clearForTrunk = false;
          break;
        }
      }
      if (!clearForTrunk) {
        continue;
      }

      int topY = surfaceY + trunkHeight;
      for (int dy = -2; dy <= 2; ++dy) {
        int ty = topY + dy;
        if (ty <= surfaceY || ty >= kChunkHeight) {
          continue;
        }

        int radius = (dy <= 0) ? 2 : 1;
        if (dy == 2) {
          radius = 0;
        }

        for (int oz = -radius; oz <= radius; ++oz) {
          for (int ox = -radius; ox <= radius; ++ox) {
            int tx = lx + ox;
            int tz = lz + oz;
            if (tx < 0 || tx >= kChunkSize || tz < 0 || tz >= kChunkSize) {
              continue;
            }

            if (dy == -2 && std::abs(ox) == 2 && std::abs(oz) == 2) {
              continue;
            }
            if (dy == 1 && (std::abs(ox) + std::abs(oz) > 1)) {
              continue;
            }
            if (dy == 2 && (ox != 0 || oz != 0)) {
              continue;
            }

            uint8_t current = getLocalBlock(tx, ty, tz);
            if (current == kAir || isWaterBlock(current) || current == kLeaves) {
              setLocalBlock(tx, ty, tz, kLeaves);
            }
          }
        }
      }

      for (int y = surfaceY + 1; y <= surfaceY + trunkHeight; ++y) {
        setLocalBlock(lx, y, lz, kWood);
      }
      treePlaced[surfaceIndex] = 1;
    }
  }

  uint32_t structureRng = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, 0x5EEDu));
  int structureAttempts = 1;
  float structureChance = (settings.preset == WorldPreset::kClassicFlat) ? 0.005f : 0.013f;

  for (int attempt = 0; attempt < structureAttempts; ++attempt) {
    if (rand01(structureRng) > structureChance) {
      continue;
    }

    int centerX = randIntInclusive(structureRng, 4, kChunkSize - 5);
    int centerZ = randIntInclusive(structureRng, 4, kChunkSize - 5);
    size_t centerIndex = static_cast<size_t>(centerX + centerZ * kChunkSize);
    int surfaceY = surfaceHeights[centerIndex];
    if (surfaceY < 6 || surfaceY >= kChunkHeight - 10) {
      continue;
    }
    if (surfaceY <= kSeaLevel + 2) {
      continue;
    }

    uint8_t centerGround = getLocalBlock(centerX, surfaceY, centerZ);
    if (centerGround != kGrass && centerGround != kDirt && centerGround != kSand) {
      continue;
    }

    bool validSite = true;
    int minTop = surfaceY;
    int maxTop = surfaceY;
    for (int oz = -3; oz <= 3 && validSite; ++oz) {
      for (int ox = -3; ox <= 3; ++ox) {
        int sx = centerX + ox;
        int sz = centerZ + oz;
        if (sx < 0 || sx >= kChunkSize || sz < 0 || sz >= kChunkSize) {
          validSite = false;
          break;
        }

        int topY = surfaceHeights[static_cast<size_t>(sx + sz * kChunkSize)];
        if (topY < 1) {
          validSite = false;
          break;
        }

        minTop = std::min(minTop, topY);
        maxTop = std::max(maxTop, topY);

        uint8_t topType = getLocalBlock(sx, topY, sz);
        if (isWaterBlock(topType)) {
          validSite = false;
          break;
        }

        for (int y = surfaceY + 1; y <= surfaceY + 6; ++y) {
          uint8_t type = getLocalBlock(sx, y, sz);
          if (type != kAir && !isWaterBlock(type) && type != kLeaves) {
            validSite = false;
            break;
          }
        }
        if (!validSite) {
          break;
        }
      }
    }
    if (!validSite || (maxTop - minTop) > 2) {
      continue;
    }

    for (int oz = -3; oz <= 3; ++oz) {
      for (int ox = -3; ox <= 3; ++ox) {
        int sx = centerX + ox;
        int sz = centerZ + oz;
        int topY = surfaceHeights[static_cast<size_t>(sx + sz * kChunkSize)];
        uint8_t topType = getLocalBlock(sx, topY, sz);
        uint8_t fillType = (topType == kSand) ? kSand : kDirt;

        if (topY < surfaceY) {
          for (int y = topY + 1; y <= surfaceY; ++y) {
            setLocalBlock(sx, y, sz, fillType);
          }
        } else if (topY > surfaceY) {
          for (int y = surfaceY + 1; y <= topY; ++y) {
            setLocalBlock(sx, y, sz, kAir);
          }
        }

        int worldX = baseX + sx;
        int worldZ = baseZ + sz;
        float crack = hashedNoise01(worldX, surfaceY, worldZ, seed, 0x5151u);
        uint8_t floorType = crack > 0.78f ? kGravel : kStone;
        if (crack < 0.08f && (std::abs(ox) + std::abs(oz) <= 3)) {
          floorType = fillType;
        }
        setLocalBlock(sx, surfaceY, sz, floorType);
      }
    }

    int doorwaySide = randIntInclusive(structureRng, 0, 3);
    auto isDoorOpening = [&](int ox, int oz) {
      if (doorwaySide == 0) {
        return oz == -3 && std::abs(ox) <= 1;
      }
      if (doorwaySide == 1) {
        return oz == 3 && std::abs(ox) <= 1;
      }
      if (doorwaySide == 2) {
        return ox == -3 && std::abs(oz) <= 1;
      }
      return ox == 3 && std::abs(oz) <= 1;
    };

    for (int oz = -3; oz <= 3; ++oz) {
      for (int ox = -3; ox <= 3; ++ox) {
        bool edge = std::abs(ox) == 3 || std::abs(oz) == 3;
        if (!edge || isDoorOpening(ox, oz)) {
          continue;
        }

        int sx = centerX + ox;
        int sz = centerZ + oz;
        int wallHeight = (rand01(structureRng) < 0.58f) ? 2 : 1;
        if (rand01(structureRng) < 0.25f) {
          wallHeight = 0;
        }
        for (int y = 1; y <= wallHeight; ++y) {
          uint8_t blockType = (rand01(structureRng) < 0.35f) ? kGravel : kStone;
          setLocalBlock(sx, surfaceY + y, sz, blockType);
        }
      }
    }

    constexpr std::array<std::pair<int, int>, 4> kCornerOffsets = {{
      {-2, -2},
      {2, -2},
      {-2, 2},
      {2, 2}
    }};
    for (const auto& [ox, oz] : kCornerOffsets) {
      int sx = centerX + ox;
      int sz = centerZ + oz;
      int pillarHeight = randIntInclusive(structureRng, 4, 6);
      int brokenTop = rand01(structureRng) < 0.55f ? randIntInclusive(structureRng, 0, 2) : 0;
      for (int y = 1; y <= pillarHeight - brokenTop; ++y) {
        uint8_t blockType = (rand01(structureRng) < 0.18f) ? kGravel : kStone;
        setLocalBlock(sx, surfaceY + y, sz, blockType);
      }
    }

    auto placeArch = [&](int ox, int oz, bool enabled) {
      if (!enabled) {
        return;
      }
      int sx = centerX + ox;
      int sz = centerZ + oz;
      for (int y = 3; y <= 4; ++y) {
        uint8_t current = getLocalBlock(sx, surfaceY + y, sz);
        if (current == kAir || isWaterBlock(current) || current == kLeaves) {
          uint8_t archType = (rand01(structureRng) < 0.30f) ? kGravel : kStone;
          setLocalBlock(sx, surfaceY + y, sz, archType);
        }
      }
    };

    bool openNorth = doorwaySide == 0;
    bool openSouth = doorwaySide == 1;
    bool openWest = doorwaySide == 2;
    bool openEast = doorwaySide == 3;

    for (int t = -1; t <= 1; ++t) {
      placeArch(t, -2, !openNorth && rand01(structureRng) < 0.78f);
      placeArch(t, 2, !openSouth && rand01(structureRng) < 0.78f);
      placeArch(-2, t, !openWest && rand01(structureRng) < 0.78f);
      placeArch(2, t, !openEast && rand01(structureRng) < 0.78f);
    }

    int altarHeight = randIntInclusive(structureRng, 1, 2);
    for (int oz = -1; oz <= 1; ++oz) {
      for (int ox = -1; ox <= 1; ++ox) {
        setLocalBlock(centerX + ox, surfaceY + 1, centerZ + oz, kStone);
      }
    }
    for (int y = 2; y <= 1 + altarHeight; ++y) {
      setLocalBlock(centerX, surfaceY + y, centerZ, kStone);
    }
    if (rand01(structureRng) < 0.35f) {
      setLocalBlock(centerX, surfaceY + 2 + altarHeight, centerZ, kWood);
    }

    int rubbleCount = randIntInclusive(structureRng, 8, 14);
    for (int i = 0; i < rubbleCount; ++i) {
      int rx = randIntInclusive(structureRng, -4, 4);
      int rz = randIntInclusive(structureRng, -4, 4);
      if (std::abs(rx) <= 2 && std::abs(rz) <= 2) {
        continue;
      }

      int sx = centerX + rx;
      int sz = centerZ + rz;
      if (sx < 0 || sx >= kChunkSize || sz < 0 || sz >= kChunkSize) {
        continue;
      }

      int topY = surfaceHeights[static_cast<size_t>(sx + sz * kChunkSize)];
      if (topY < 1 || topY >= kChunkHeight - 2) {
        continue;
      }

      uint8_t existing = getLocalBlock(sx, topY + 1, sz);
      if (existing == kAir || isWaterBlock(existing) || existing == kLeaves) {
        uint8_t rubbleType = (rand01(structureRng) < 0.55f) ? kGravel : kStone;
        setLocalBlock(sx, topY + 1, sz, rubbleType);
      }
    }
  }

  return blocks;
}

enum class DebugBiomeId : uint8_t {
  kOcean = 0,
  kBeach = 1,
  kPlains = 2,
  kForest = 3,
  kDesert = 4,
  kMountains = 5,
  kCrash = 6
};

bool isWaterDebugBiome(uint8_t biomeId) {
  return biomeId == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
         biomeId == static_cast<uint8_t>(DebugBiomeId::kBeach);
}

bool isMountainDebugBiome(uint8_t biomeId) {
  return biomeId == static_cast<uint8_t>(DebugBiomeId::kMountains) ||
         biomeId == static_cast<uint8_t>(DebugBiomeId::kCrash);
}

bool isCrashDebugBiome(uint8_t biomeId) {
  return biomeId == static_cast<uint8_t>(DebugBiomeId::kCrash);
}

bool isLowlandDebugBiome(uint8_t biomeId) {
  return biomeId == static_cast<uint8_t>(DebugBiomeId::kPlains) ||
         biomeId == static_cast<uint8_t>(DebugBiomeId::kForest) ||
         biomeId == static_cast<uint8_t>(DebugBiomeId::kDesert);
}

bool isAridTransitionPair(uint8_t a, uint8_t b) {
  if (a == b) {
    return false;
  }

  bool aDesert = a == static_cast<uint8_t>(DebugBiomeId::kDesert);
  bool bDesert = b == static_cast<uint8_t>(DebugBiomeId::kDesert);
  bool aSoftLowland = a == static_cast<uint8_t>(DebugBiomeId::kPlains) ||
                      a == static_cast<uint8_t>(DebugBiomeId::kForest);
  bool bSoftLowland = b == static_cast<uint8_t>(DebugBiomeId::kPlains) ||
                      b == static_cast<uint8_t>(DebugBiomeId::kForest);
  return (aDesert && bSoftLowland) || (bDesert && aSoftLowland);
}

constexpr int kStageSeaLevel = 32;
constexpr int kCarverRegionSizeChunks = 8;
constexpr int kStructureRegionSizeChunks = 32;

struct ClimateAndBiomeSample {
  uint8_t biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
  BiomeClimateSample climate{};
};

ClimateAndBiomeSample sampleClimateAndBiomeAt(int worldX,
                                              int worldZ,
                                              int seed,
                                              const WorldGenSettings& settings) {
  bool aprilMode = isAprilFoolsPreset(settings);
  if (settings.preset == WorldPreset::kClassicFlat) {
    ClimateAndBiomeSample flat{};
    flat.biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
    flat.climate = {
      0.58f, // temperature
      0.52f, // humidity
      0.62f, // continentalness
      0.50f, // erosion
      0.50f, // depth
      0.50f  // weirdness
    };
    return flat;
  }

  double warpX = seededPerlin2D(worldX, worldZ, seed, 101u, 89u, 0.0016, 0.0016);
  double warpZ = seededPerlin2D(worldX, worldZ, seed, 37u, 53u, 0.0016, 0.0016);
  double wx = static_cast<double>(worldX) + warpX * 24.0;
  double wz = static_cast<double>(worldZ) + warpZ * 24.0;

  float temperature = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 17u, 11u, 0.00120, 0.00120));
  float humidity = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 7u, 13u, 0.00130, 0.00130));
  float continentalness = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 11u, 13u, 0.00058, 0.00058));
  float dxFromOrigin = static_cast<float>(worldX);
  float dzFromOrigin = static_cast<float>(worldZ);
  float distFromOrigin = std::sqrt(dxFromOrigin * dxFromOrigin + dzFromOrigin * dzFromOrigin);
  float spawnLandBias = smooth01(1.0f - distFromOrigin / 672.0f) * 0.18f;
  continentalness = std::clamp(continentalness + 0.05f + spawnLandBias, 0.0f, 1.0f);
  float erosion = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 37u, 23u, 0.00148, 0.00148));
  float weirdness = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 61u, 53u, 0.00225, 0.00225));
  float depth = 0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 29u, 31u, 0.00178, 0.00178));

  if (aprilMode) {
    float prankRidge =
      0.5f + 0.5f * static_cast<float>(seededRidge2D(wx, wz, seed, 151u, 157u, 0.0031, 0.0031));
    float prankRelief =
      0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 181u, 191u, 0.0048, 0.0048));
    temperature = std::clamp(temperature * 0.72f + prankRelief * 0.18f + 0.10f, 0.32f, 0.82f);
    humidity = std::clamp(humidity + 0.08f + prankRidge * 0.08f, 0.0f, 1.0f);
    continentalness = std::clamp(continentalness + 0.10f + prankRelief * 0.10f, 0.0f, 1.0f);
    erosion = std::clamp(erosion * 0.74f - 0.10f + prankRidge * 0.04f, 0.0f, 1.0f);
    weirdness = std::clamp(std::max(weirdness, prankRidge) + 0.16f, 0.0f, 1.0f);
    depth = std::clamp(depth + (prankRelief - 0.5f) * 0.36f + 0.10f, 0.0f, 1.0f);
  }

  float crashField = 0.0f;
  if (aprilMode) {
    float crashRidge =
      0.5f + 0.5f * static_cast<float>(seededRidge2D(wx, wz, seed, 211u, 223u, 0.0052, 0.0052));
    float crashScatter =
      0.5f + 0.5f * static_cast<float>(seededPerlin2D(wx, wz, seed, 227u, 229u, 0.0072, 0.0072));
    crashField = crashRidge * 0.70f + crashScatter * 0.30f;
  }

  DebugBiomeId biome = DebugBiomeId::kPlains;
  if (continentalness < 0.22f) {
    biome = DebugBiomeId::kOcean;
  } else if (continentalness < 0.29f) {
    biome = DebugBiomeId::kBeach;
  } else if (aprilMode &&
             crashField > 0.72f &&
             continentalness > 0.46f &&
             weirdness > 0.64f &&
             erosion < 0.58f &&
             depth > 0.42f) {
    biome = DebugBiomeId::kCrash;
  } else if (temperature > 0.72f && humidity < 0.34f) {
    biome = DebugBiomeId::kDesert;
  } else if (weirdness > 0.67f && continentalness > 0.56f) {
    biome = DebugBiomeId::kMountains;
  } else if (humidity > 0.58f) {
    biome = DebugBiomeId::kForest;
  }

  ClimateAndBiomeSample out{};
  out.biome = static_cast<uint8_t>(biome);
  out.climate = {
    temperature,
    humidity,
    continentalness,
    erosion,
    depth,
    weirdness
  };
  return out;
}

TerrainRouterSample sampleTerrainRouterAt(int worldX,
                                          int worldZ,
                                          int seed,
                                          uint8_t biomeId,
                                          const BiomeClimateSample& climate,
                                          int minY,
                                          int maxY) {
  const std::array<std::pair<float, float>, 9> kContinentalSpline = {{
    {0.00f, -26.0f},
    {0.10f, -20.0f},
    {0.18f, -14.0f},
    {0.25f, -7.0f},
    {0.34f, -1.5f},
    {0.46f, 8.0f},
    {0.62f, 20.0f},
    {0.80f, 36.0f},
    {1.00f, 52.0f}
  }};
  const std::array<std::pair<float, float>, 5> kReliefSpline = {{
    {0.00f, 0.56f},
    {0.24f, 0.72f},
    {0.45f, 1.02f},
    {0.72f, 1.42f},
    {1.00f, 1.88f}
  }};
  const std::array<std::pair<float, float>, 5> kErosionSpline = {{
    {0.00f, 1.18f},
    {0.28f, 0.96f},
    {0.52f, 0.74f},
    {0.76f, 0.46f},
    {1.00f, 0.22f}
  }};
  const std::array<std::pair<float, float>, 5> kJaggedSpline = {{
    {0.00f, 0.02f},
    {0.20f, 0.10f},
    {0.45f, 0.24f},
    {0.72f, 0.56f},
    {1.00f, 0.96f}
  }};
  const std::array<std::pair<float, float>, 5> kJaggedErosionSpline = {{
    {0.00f, 1.00f},
    {0.30f, 0.82f},
    {0.58f, 0.56f},
    {0.82f, 0.28f},
    {1.00f, 0.12f}
  }};

  float macro = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 23u, 19u, 0.00145, 0.00145));
  float local = fbmNoise(static_cast<float>(worldX), static_cast<float>(worldZ), seed);
  float ridge = static_cast<float>(seededRidge2D(worldX, worldZ, seed, 73u, 67u, 0.00255, 0.00255));
  float folded = static_cast<float>(seededRidge2D(worldX, worldZ, seed, 41u, 37u, 0.0064, 0.0064));

  float pv = peaksAndValleys(climate.weirdness);
  float peakMask = smooth01((pv + 0.05f) / 0.72f);
  float valleyMask = smooth01((0.06f - pv) / 0.52f);
  float plateauMask = smooth01((climate.continentalness - 0.38f) / 0.28f) *
                      smooth01((0.56f - climate.erosion) / 0.34f);
  float softTerrain = surfaceSofteningMask(biomeId, climate);
  float oceanMask = smooth01((0.33f - climate.continentalness) / 0.17f);
  float shoreMask = smooth01((0.38f - climate.continentalness) / 0.15f) * (1.0f - oceanMask * 0.55f);
  float beachMask = smooth01((climate.continentalness - 0.20f) / 0.10f) *
                    smooth01((0.36f - climate.continentalness) / 0.10f) *
                    (1.0f - oceanMask * 0.60f);
  float desertMask = smooth01((climate.temperature - 0.70f) / 0.10f) *
                     smooth01((0.40f - climate.humidity) / 0.12f) *
                     (1.0f - oceanMask);
  float mountainMask = smooth01((climate.weirdness - 0.60f) / 0.16f) *
                       smooth01((climate.continentalness - 0.52f) / 0.14f) *
                       (1.0f - oceanMask);
  float forestMask = smooth01((climate.humidity - 0.56f) / 0.18f) *
                     smooth01((climate.temperature - 0.26f) / 0.16f) *
                     smooth01((0.78f - climate.temperature) / 0.18f) *
                     (1.0f - desertMask) *
                     (1.0f - oceanMask);
  float lowlandReliefMask = (1.0f - oceanMask) *
                            (1.0f - beachMask * 0.70f) *
                            (1.0f - mountainMask * 0.55f);
  float relief = sampleSplineCurve(kReliefSpline, climate.continentalness);
  float erosionShape = sampleSplineCurve(kErosionSpline, climate.erosion);
  float jaggedness =
    sampleSplineCurve(kJaggedSpline, peakMask) *
    sampleSplineCurve(kJaggedErosionSpline, climate.erosion);
  float depthRelief = (climate.depth - 0.5f) * 11.0f * relief;

  float offset = static_cast<float>(kStageSeaLevel) - 6.0f +
                 sampleSplineCurve(kContinentalSpline, climate.continentalness);
  offset += macro * (4.2f + relief * 4.6f) * erosionShape;
  offset += local * (1.8f + relief * 3.4f) * (0.76f + erosionShape * 0.24f);
  offset += depthRelief;
  offset += peakMask * peakMask * (10.0f + plateauMask * 24.0f + relief * 6.0f);
  offset -= valleyMask * (8.0f + climate.erosion * 10.0f + (1.0f - climate.continentalness) * 5.0f);
  offset += ridge * (1.8f + plateauMask * 7.0f + peakMask * 5.5f);
  offset += folded * jaggedness * (2.0f + plateauMask * 7.0f);
  float rollingMacro = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 57u, 51u, 0.0042, 0.0042));
  float rollingRidge = static_cast<float>(seededRidge2D(worldX, worldZ, seed, 93u, 97u, 0.0085, 0.0085));
  float rollingAmplitude = (4.2f +
                            (1.0f - climate.erosion) * 7.4f +
                            climate.depth * 6.0f +
                            desertMask * 1.2f) * lowlandReliefMask;
  offset += rollingMacro * rollingAmplitude;
  offset += (rollingRidge - 0.5f) * (3.0f + climate.depth * 3.6f) * lowlandReliefMask;

  float oceanTarget = static_cast<float>(kStageSeaLevel) - 18.0f +
                      macro * 2.2f +
                      local * 1.1f -
                      oceanMask * 5.5f;
  float shoreTarget = static_cast<float>(kStageSeaLevel) - 1.0f + macro * 1.0f + local * 0.4f;
  float beachTarget = static_cast<float>(kStageSeaLevel) - 1.5f +
                      macro * 0.9f +
                      local * 0.45f +
                      depthRelief * 0.10f;
  offset = lerpValue(offset, oceanTarget, oceanMask * 0.94f);
  offset = lerpValue(offset, shoreTarget, shoreMask * 0.54f);
  offset = lerpValue(offset, beachTarget, beachMask * 0.78f);

  float riverWarpX = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 149u, 131u, 0.0020, 0.0020));
  float riverWarpZ = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 109u, 127u, 0.0020, 0.0020));
  double riverX = static_cast<double>(worldX) + static_cast<double>(riverWarpX) * 22.0;
  double riverZ = static_cast<double>(worldZ) + static_cast<double>(riverWarpZ) * 22.0;
  float riverA = static_cast<float>(seededRidge2D(riverX, riverZ, seed, 43u, 37u, 0.0029, 0.0029));
  float riverB = static_cast<float>(seededRidge2D(riverX, riverZ, seed, 17u, 19u, 0.0056, 0.0056));
  float riverLine = riverA * 0.74f + riverB * 0.26f;
  float inlandRiverMask =
    smooth01((climate.continentalness - 0.24f) / 0.12f) *
    smooth01((0.84f - climate.continentalness) / 0.22f) *
    (1.0f - oceanMask);
  float riverCore = smooth01((riverLine - 0.84f) / 0.14f) * inlandRiverMask;
  float riverBank = smooth01((riverLine - 0.72f) / 0.20f) * inlandRiverMask;
  float riverTarget = static_cast<float>(kStageSeaLevel) - 3.0f +
                      macro * 0.9f +
                      local * 0.35f -
                      std::max(0.0f, climate.depth - 0.55f) * 2.0f;
  offset = lerpValue(offset, std::min(offset, riverTarget), riverCore * 0.96f);
  offset -= riverBank * (1.2f + climate.humidity * 1.0f);

  offset += desertMask * (1.5f + std::max(0.0f, climate.depth - 0.52f) * 7.0f);
  offset += mountainMask * (5.0f + peakMask * (8.0f + plateauMask * 11.0f));
  jaggedness = lerpValue(jaggedness, jaggedness * 0.18f, oceanMask * 0.96f);
  jaggedness = lerpValue(jaggedness, jaggedness * 0.22f, beachMask * 0.72f);
  jaggedness = lerpValue(jaggedness, jaggedness * 0.64f, desertMask * 0.72f);
  jaggedness = lerpValue(jaggedness,
                         std::min(1.45f, jaggedness * 1.22f + 0.08f),
                         mountainMask * 0.92f);
  relief = lerpValue(relief, relief * 0.44f, oceanMask * 0.96f);
  relief = lerpValue(relief, relief * 0.58f, beachMask * 0.68f);

  float gentleTarget = static_cast<float>(kStageSeaLevel) + 7.0f +
                       macro * (2.8f + climate.depth * 2.4f) +
                       local * (1.4f + climate.depth * 1.6f) +
                       depthRelief * 0.42f -
                       valleyMask * (2.2f + climate.erosion * 2.4f);
  offset = lerpValue(offset, gentleTarget, softTerrain * 0.22f);
  jaggedness = lerpValue(jaggedness, jaggedness * 0.74f, softTerrain * 0.58f);
  relief = lerpValue(relief, relief * 0.92f, softTerrain * 0.36f);

  float crashMask = biomeId == static_cast<uint8_t>(DebugBiomeId::kCrash) ? 1.0f : 0.0f;
  if (crashMask > 0.0f) {
    float crashBase = static_cast<float>(kStageSeaLevel) + 10.0f +
                      macro * 3.2f +
                      local * (2.0f + climate.depth * 1.2f) +
                      depthRelief * 0.22f -
                      valleyMask * 1.8f;
    offset = lerpValue(offset, crashBase, 0.84f);
    relief = lerpValue(relief, 0.82f, 0.74f);
    jaggedness = lerpValue(jaggedness, 0.18f + folded * 0.10f, 0.84f);
  }

  float openTerrain = 0.62f +
                      (1.0f - climate.erosion) * 0.30f +
                      climate.continentalness * 0.16f +
                      relief * 0.18f;
  float detailAmplitude = 1.10f + openTerrain * 1.42f + plateauMask * 0.34f;
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 0.22f, oceanMask * 0.96f);
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 0.42f, beachMask * 0.72f);
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 0.82f, forestMask * 0.55f);
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 0.70f, desertMask * 0.70f);
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 1.20f, mountainMask * 0.88f);
  detailAmplitude = lerpValue(detailAmplitude, detailAmplitude * 0.70f, crashMask * 0.82f);
  detailAmplitude = lerpValue(detailAmplitude, 0.94f + climate.depth * 0.52f, softTerrain * 0.28f);
  offset += sampleSurfaceReliefNoise(worldX, worldZ, seed) * detailAmplitude;

  int heightClampMin = minY + 4;
  int heightClampMax = maxY - 4;
  if (heightClampMax < heightClampMin) {
    heightClampMin = minY;
    heightClampMax = maxY;
  }

  TerrainRouterSample router;
  router.baseHeight = offset;
  router.finalHeight = std::clamp(offset,
                                  static_cast<float>(heightClampMin),
                                  static_cast<float>(heightClampMax));
  router.factor = 0.090f + relief * 0.028f * (0.78f + erosionShape * 0.22f) +
                  peakMask * 0.014f - valleyMask * 0.008f;
  router.factor = lerpValue(router.factor, router.factor * 0.94f, softTerrain * 0.28f);
  router.jaggedness = jaggedness;
  router.peakMask = peakMask;
  router.valleyMask = valleyMask;
  router.plateauMask = plateauMask;
  return router;
}

float computeRouterTargetHeight(int worldX,
                                int worldZ,
                                int seed,
                                uint8_t biomeId,
                                const BiomeClimateSample& climate,
                                int minY,
                                int maxY) {
  return sampleTerrainRouterAt(worldX, worldZ, seed, biomeId, climate, minY, maxY).finalHeight;
}

int computeAquiferLevelAt(int worldX,
                          int worldZ,
                          int seed,
                          const BiomeClimateSample& climate,
                          int minY,
                          int maxY) {
  float aqMacro = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 61u, 59u, 0.0105, 0.0105));
  float aqLocal = static_cast<float>(seededPerlin2D(worldX, worldZ, seed, 17u, 23u, 0.024, 0.024));
  float continentalLift = smooth01((climate.continentalness - 0.40f) / 0.42f);
  float aquiferBase = static_cast<float>(kStageSeaLevel) - 5.0f +
                      aqMacro * 8.0f +
                      aqLocal * 3.0f -
                      continentalLift * (5.0f + climate.depth * 6.0f);

  if (climate.continentalness < 0.24f) {
    aquiferBase = std::max(aquiferBase, static_cast<float>(kStageSeaLevel) - 1.0f);
  } else {
    aquiferBase = std::min(aquiferBase, static_cast<float>(kStageSeaLevel) - 2.0f);
  }

  int aquiferClampMin = minY + 2;
  int aquiferClampMax = maxY - 2;
  if (aquiferClampMax < aquiferClampMin) {
    aquiferClampMin = minY;
    aquiferClampMax = maxY;
  }
  int aquiferLevel = static_cast<int>(std::round(aquiferBase));
  return std::clamp(aquiferLevel, aquiferClampMin, aquiferClampMax);
}

struct CaveFieldSample {
  float cavernA = 0.0f;
  float cavernB = 0.0f;
  float spaghetti = 0.0f;
  float noodles = 0.0f;
  float chamber = 0.0f;
  float pillar = 0.0f;
  float vault = 0.0f;
  float gallery = 0.0f;
};

CaveFieldSample sampleCaveFieldAt(int worldX, int y, int worldZ, int seed) {
  CaveFieldSample sample;
  sample.cavernA = static_cast<float>(seededPerlin3D(worldX, y, worldZ, seed, 41u, 37u, 43u, 0.038, 0.042, 0.038));
  sample.cavernB = static_cast<float>(seededPerlin3D(worldX, y, worldZ, seed, 21u, 19u, 17u, 0.056, 0.061, 0.056));
  sample.spaghetti = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 13u, 29u, 17u, 0.022, 0.029, 0.022));
  sample.noodles = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 61u, 11u, 59u, 0.066, 0.020, 0.066));
  sample.chamber = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 53u, 47u, 49u, 0.013, 0.018, 0.013));
  sample.pillar = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 71u, 5u, 79u, 0.018, 0.035, 0.018));
  sample.vault = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 83u, 67u, 79u, 0.010, 0.014, 0.010));
  sample.gallery = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 97u, 71u, 101u, 0.015, 0.010, 0.015));
  return sample;
}

float sampleDryCavernMaskAt(const CaveFieldSample& caveFields,
                            int y,
                            const WorldGenSettings& settings,
                            const BiomeClimateSample& climate,
                            const TerrainRouterSample& router) {
  float caveScale = std::clamp(settings.caveDensity, 0.25f, 2.5f);
  bool aprilMode = isAprilFoolsPreset(settings);
  if (aprilMode) {
    caveScale = std::clamp(caveScale + 0.55f, 0.25f, 2.5f);
  }
  float caveScaleT = saturate((caveScale - 0.25f) / 2.25f);
  float belowSurface = router.finalHeight - static_cast<float>(y);
  float inlandness = smooth01((climate.continentalness - 0.30f) / 0.34f);
  float enclosed = smooth01((belowSurface - (aprilMode ? 6.0f : 10.0f)) / (aprilMode ? 14.0f : 18.0f));
  float deepness = smooth01((static_cast<float>(kStageSeaLevel + (aprilMode ? 12 : 6) - y)) /
                            (aprilMode ? 40.0f : 34.0f));
  float vaultMask = smooth01((caveFields.vault -
                              std::clamp(0.79f - caveScaleT * 0.18f, 0.60f, 0.82f)) / 0.16f);
  float chamberMask = smooth01((caveFields.chamber -
                                std::clamp(0.73f - caveScaleT * 0.12f, 0.55f, 0.77f)) / 0.15f);
  float galleryMask = smooth01((caveFields.gallery -
                                std::clamp(0.84f - caveScaleT * 0.16f, 0.62f, 0.86f)) / 0.16f);
  float cavernMask = std::max(vaultMask, std::max(chamberMask, galleryMask * (aprilMode ? 1.04f : 0.86f))) *
         inlandness *
         enclosed *
         deepness;
  return aprilMode ? std::min(1.0f, cavernMask * 1.18f) : cavernMask;
}

float sampleDensityRouterWithCaveFields(int worldX,
                                        int y,
                                        int worldZ,
                                        int seed,
                                        const WorldGenSettings& settings,
                                        uint8_t biomeId,
                                        const BiomeClimateSample& climate,
                                        const TerrainRouterSample& router,
                                        int minY,
                                        const CaveFieldSample& caveFields) {
  bool aprilMode = isAprilFoolsPreset(settings);
  float vertical = (router.finalHeight - static_cast<float>(y)) * router.factor;
  float macro3d = static_cast<float>(seededPerlin3D(worldX, y, worldZ, seed, 31u, 17u, 29u, 0.037, 0.043, 0.037));
  float micro3d = static_cast<float>(seededPerlin3D(worldX, y, worldZ, seed, 7u, 13u, 11u, 0.081, 0.094, 0.081));
  float ridge3d = static_cast<float>(seededRidge3D(worldX, y, worldZ, seed, 47u, 53u, 43u, 0.024, 0.029, 0.024));

  float caveScale = std::clamp(settings.caveDensity, 0.25f, 2.5f);
  if (aprilMode) {
    caveScale = std::clamp(caveScale + 0.55f, 0.25f, 2.5f);
  }
  float caveScaleT = saturate((caveScale - 0.25f) / 2.25f);
  float cheese = 1.0f - std::min(1.0f, std::abs(caveFields.cavernA) * 0.76f + std::abs(caveFields.cavernB) * 0.54f);
  float cavernThreshold = std::clamp(0.64f - caveScaleT * 0.19f - router.peakMask * 0.05f, 0.38f, 0.70f);
  float cavernCut = std::max(0.0f, cheese - cavernThreshold) * (aprilMode ? 11.2f : 9.4f);
  float spaghettiThreshold = std::clamp(0.77f - caveScaleT * 0.18f, 0.48f, 0.80f);
  float spaghettiCut = std::max(0.0f, caveFields.spaghetti - spaghettiThreshold) * (aprilMode ? 5.9f : 5.2f);
  float noodleThreshold = std::clamp(0.88f - caveScaleT * 0.14f, 0.64f, 0.90f);
  float noodleDepth = smooth01((static_cast<float>(kStageSeaLevel - 4 - y)) / 46.0f);
  float noodleCut = std::max(0.0f, caveFields.noodles - noodleThreshold) *
                    (aprilMode ? 3.8f : 3.2f) *
                    noodleDepth;
  float chamberThreshold = std::clamp(0.72f - caveScaleT * 0.14f, 0.50f, 0.76f);
  float chamberCut = std::max(0.0f, caveFields.chamber - chamberThreshold) * (aprilMode ? 5.4f : 4.3f);
  float belowSurface = router.finalHeight - static_cast<float>(y);
  float depthFactor = smooth01((static_cast<float>(kStageSeaLevel + (aprilMode ? 36 : 28) - y)) /
                               (aprilMode ? 86.0f : 78.0f));
  float surfaceShield = smooth01((belowSurface - (aprilMode ? -1.0f : 2.0f)) /
                                 (aprilMode ? 8.0f : 10.0f));
  float deepCavernFactor = smooth01((static_cast<float>(kStageSeaLevel + (aprilMode ? 16 : 10) - y)) /
                                    (aprilMode ? 52.0f : 46.0f)) *
                           smooth01((belowSurface - (aprilMode ? 3.0f : 6.0f)) /
                                    (aprilMode ? 14.0f : 16.0f));
  float vaultThreshold = std::clamp(0.80f - caveScaleT * 0.22f - router.valleyMask * 0.04f, 0.54f, 0.82f);
  float vaultCut = std::max(0.0f, caveFields.vault - vaultThreshold) *
                   (aprilMode ? 13.4f : 11.2f) *
                   deepCavernFactor;
  float galleryThreshold = std::clamp(0.84f - caveScaleT * 0.18f, 0.60f, 0.86f);
  float galleryCut = std::max(0.0f, caveFields.gallery - galleryThreshold) *
                     (aprilMode ? 6.0f : 4.8f) *
                     deepCavernFactor;
  float caveCut =
    (cavernCut + spaghettiCut + noodleCut + chamberCut + vaultCut + galleryCut) * depthFactor * surfaceShield;
  if (aprilMode) {
    caveCut *= 1.18f;
  }
  float pillarAnchor = std::max(cavernCut, std::max(chamberCut, vaultCut * 0.74f));
  float pillarMask = smooth01((caveFields.pillar - 0.79f) / 0.21f) *
                     smooth01((pillarAnchor - 0.70f) / 2.0f);
  float pillarBoost = pillarMask * (1.08f + (1.0f - climate.erosion) * 1.02f);
  float ridgeBoost = ridge3d * (0.10f + router.jaggedness * 0.24f) *
                     (0.42f + (1.0f - climate.erosion));
  if (biomeId == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
    ridgeBoost *= 1.42f;
  }

  float density = vertical +
                  macro3d * (0.76f + router.peakMask * 0.16f) +
                  micro3d * (0.24f + router.jaggedness * 0.20f) +
                  ridgeBoost -
                  caveCut +
                  pillarBoost;
  if (y < minY + 2) {
    density += 2.4f;
  }
  return density;
}

float sampleDensityRouterAt(int worldX,
                            int y,
                            int worldZ,
                            int seed,
                            const WorldGenSettings& settings,
                            uint8_t biomeId,
                            const BiomeClimateSample& climate,
                            const TerrainRouterSample& router,
                            int minY) {
  CaveFieldSample caveFields = sampleCaveFieldAt(worldX, y, worldZ, seed);
  return sampleDensityRouterWithCaveFields(worldX,
                                           y,
                                           worldZ,
                                           seed,
                                           settings,
                                           biomeId,
                                           climate,
                                           router,
                                           minY,
                                           caveFields);
}

void computeBiomeClimateMaps(int cx,
                             int cz,
                             int seed,
                             const WorldGenSettings& settings,
                             std::array<uint8_t, kChunkColumnCount>& outBiomeMap,
                             std::array<BiomeClimateSample, kChunkColumnCount>& outClimateMap) {
  outBiomeMap.fill(static_cast<uint8_t>(DebugBiomeId::kPlains));
  outClimateMap.fill(BiomeClimateSample{});

  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      ClimateAndBiomeSample sample = sampleClimateAndBiomeAt(worldX, worldZ, seed, settings);
      outBiomeMap[idx] = sample.biome;
      outClimateMap[idx] = sample.climate;
    }
  }
}

void applyHeightRangeMask(std::vector<uint8_t>& blocks, int minY, int maxY) {
  minY = std::clamp(minY, 0, kChunkHeight - 1);
  maxY = std::clamp(maxY, minY, kChunkHeight - 1);
  if (minY == 0 && maxY == kChunkHeight - 1) {
    return;
  }
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      for (int y = 0; y < minY; ++y) {
        blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kAir;
      }
      for (int y = maxY + 1; y < kChunkHeight; ++y) {
        blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kAir;
      }
    }
  }
}

void runNoiseStage(int cx,
                   int cz,
                   int seed,
                   const WorldGenSettings& settings,
                   const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                   const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                   std::vector<uint8_t>& outBlocks) {
  outBlocks.assign(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  if (settings.preset == WorldPreset::kClassicFlat) {
    int flatTop = std::clamp(kStageSeaLevel + 8, minY, maxY);
    for (int lz = 0; lz < kChunkSize; ++lz) {
      for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int y = minY; y <= flatTop; ++y) {
          outBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kStone;
        }
      }
    }
    applyHeightRangeMask(outBlocks, minY, maxY);
    return;
  }

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t column = static_cast<size_t>(lx + lz * kChunkSize);
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      const BiomeClimateSample& climate = climateMap[column];
      uint8_t biomeId = biomeMap[column];
      TerrainRouterSample router =
        sampleTerrainRouterAt(worldX, worldZ, seed, biomeId, climate, minY, maxY);
      int aquiferLevel = computeAquiferLevelAt(worldX, worldZ, seed, climate, minY, maxY);
      int fluidFillTopY = std::min(aquiferLevel, kStageSeaLevel - 1);
      bool openSeaColumn = router.finalHeight < static_cast<float>(kStageSeaLevel) - 0.5f;
      int openSeaStartY = std::clamp(static_cast<int>(std::floor(router.finalHeight)) + 1,
                                     minY,
                                     kStageSeaLevel - 1);
      for (int y = minY; y <= maxY; ++y) {
        CaveFieldSample caveFields = sampleCaveFieldAt(worldX, y, worldZ, seed);
        float density = sampleDensityRouterWithCaveFields(worldX,
                                                          y,
                                                          worldZ,
                                                          seed,
                                                          settings,
                                                          biomeId,
                                                          climate,
                                                          router,
                                                          minY,
                                                          caveFields);
        uint8_t block = kAir;
        if (density > 0.0f) {
          block = kStone;
        } else if (openSeaColumn && y >= openSeaStartY && y <= kStageSeaLevel - 1) {
          block = kWater;
        } else if (y <= fluidFillTopY) {
          float dryCavernMask = sampleDryCavernMaskAt(caveFields, y, settings, climate, router);
          if (dryCavernMask < 0.52f) {
            block = kWater;
          }
        }
        outBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = block;
      }
    }
  }

  applyHeightRangeMask(outBlocks, minY, maxY);
}

void runSurfaceStage(int cx,
                     int cz,
                     int seed,
                     const WorldGenSettings& settings,
                     const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                     const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                     std::vector<uint8_t>& ioBlocks) {
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);
  bool aprilMode = isAprilFoolsPreset(settings);

  auto getLocalBlock = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocalBlock = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto reshapeColumnTop = [&](int lx, int lz, int currentTopY, int targetTopY) {
    if (currentTopY < minY || currentTopY > maxY || targetTopY == currentTopY) {
      return;
    }

    if (targetTopY < currentTopY) {
      for (int y = currentTopY; y > targetTopY; --y) {
        uint8_t current = getLocalBlock(lx, y, lz);
        if (current != kAir && !isWaterBlock(current) && !isDecorationBlock(current)) {
          setLocalBlock(lx, y, lz, kAir);
        }
      }
      return;
    }

    for (int y = currentTopY + 1; y <= targetTopY; ++y) {
      uint8_t current = getLocalBlock(lx, y, lz);
      if (current == kAir || isWaterBlock(current) || isDecorationBlock(current)) {
        setLocalBlock(lx, y, lz, kStone);
      }
    }
  };

  std::array<int, kChunkColumnCount> topHeights{};
  topHeights.fill(minY - 1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      for (int y = maxY; y >= minY; --y) {
        uint8_t t = getLocalBlock(lx, y, lz);
        if (t != kAir && !isWaterBlock(t)) {
          topHeights[idx] = y;
          break;
        }
      }
    }
  }

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      int topY = topHeights[idx];
      if (topY < minY || topY > maxY) {
        continue;
      }

      uint8_t biome = biomeMap[idx];
      const BiomeClimateSample& climate = climateMap[idx];
      float softTerrain = surfaceSofteningMask(biome, climate);
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      float weightedSum = 0.0f;
      float totalWeight = 0.0f;
      float boundaryWeight = 0.0f;
      float inlandBoundaryWeight = 0.0f;
      float aridBoundaryWeight = 0.0f;
      for (int oz = -2; oz <= 2; ++oz) {
        for (int ox = -2; ox <= 2; ++ox) {
          int sampleX = worldX + ox;
          int sampleZ = worldZ + oz;
          ClimateAndBiomeSample sample = sampleClimateAndBiomeAt(sampleX, sampleZ, seed, settings);
          float targetHeight =
            computeRouterTargetHeight(sampleX,
                                      sampleZ,
                                      seed,
                                      sample.biome,
                                      sample.climate,
                                      minY,
                                      maxY);
          int dist = std::max(std::abs(ox), std::abs(oz));
          float weight = dist == 0 ? 7.0f : (dist == 1 ? 3.5f : 1.4f);
          weightedSum += targetHeight * weight;
          totalWeight += weight;
          if (sample.biome != biome) {
            boundaryWeight += weight;
            bool currentInland = isLowlandDebugBiome(biome);
            bool sampleInland = isLowlandDebugBiome(sample.biome);
            if (currentInland && sampleInland &&
                !isWaterDebugBiome(biome) &&
                !isWaterDebugBiome(sample.biome) &&
                !isMountainDebugBiome(biome) &&
                !isMountainDebugBiome(sample.biome)) {
              inlandBoundaryWeight += weight;
            }
            if (isAridTransitionPair(biome, sample.biome)) {
              aridBoundaryWeight += weight;
            }
          }
        }
      }

      if (totalWeight <= 0.0f) {
        continue;
      }

      float biomeBoundaryBlend = smooth01((boundaryWeight / totalWeight - 0.14f) / 0.28f);
      float inlandBoundaryBlend = smooth01((inlandBoundaryWeight / totalWeight - 0.06f) / 0.22f);
      float aridBoundaryBlend = smooth01((aridBoundaryWeight / totalWeight - 0.04f) / 0.16f);
      float coastBlend = smooth01((0.38f - climate.continentalness) / 0.12f);
      float seamBlend = std::clamp(std::max(inlandBoundaryBlend * 0.86f, aridBoundaryBlend), 0.0f, 1.0f);
      softTerrain = std::max(softTerrain,
                             biomeBoundaryBlend * (0.36f + coastBlend * 0.24f));
      softTerrain = std::max(softTerrain, inlandBoundaryBlend * 0.54f);
      softTerrain = std::max(softTerrain, aridBoundaryBlend * 0.66f);
      if (softTerrain < 0.18f) {
        continue;
      }

      int smoothedHeight = static_cast<int>(std::round(weightedSum / totalWeight));
      int raiseAllowance = std::max(1,
                                    static_cast<int>(std::round(lerpValue(
                                      2.0f + softTerrain * 2.6f,
                                      1.0f,
                                      seamBlend * 0.58f))));
      int lowerAllowance = std::max(2,
                                    static_cast<int>(std::round(lerpValue(
                                      4.0f + softTerrain * 5.6f +
                                        biomeBoundaryBlend * 3.0f +
                                        coastBlend * 1.5f,
                                      2.0f,
                                      seamBlend * 0.62f))));
      int targetTopY = std::clamp(topY,
                                  smoothedHeight - raiseAllowance,
                                  smoothedHeight + lowerAllowance);
      if (targetTopY == topY) {
        continue;
      }

      reshapeColumnTop(lx, lz, topY, targetTopY);
      topHeights[idx] = targetTopY;
    }
  }

  for (int pass = 0; pass < 1; ++pass) {
    std::array<int, kChunkColumnCount> relaxedHeights = topHeights;
    for (int lz = 0; lz < kChunkSize; ++lz) {
      for (int lx = 0; lx < kChunkSize; ++lx) {
        size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
        int topY = topHeights[idx];
        if (topY < minY || topY > maxY) {
          continue;
        }

        uint8_t biome = biomeMap[idx];
        if (!isLowlandDebugBiome(biome) || isMountainDebugBiome(biome) || isWaterDebugBiome(biome)) {
          continue;
        }

        int neighborSum = topY;
        int neighborCount = 1;
        int minNeighbor = topY;
        int maxNeighbor = topY;
        int seamNeighbors = 0;
        int aridNeighbors = 0;
        for (int oz = -1; oz <= 1; ++oz) {
          for (int ox = -1; ox <= 1; ++ox) {
            if (ox == 0 && oz == 0) {
              continue;
            }

            int nx = lx + ox;
            int nz = lz + oz;
            if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) {
              continue;
            }

            size_t nIdx = static_cast<size_t>(nx + nz * kChunkSize);
            int nTop = topHeights[nIdx];
            if (nTop < minY || nTop > maxY) {
              continue;
            }

            uint8_t neighborBiome = biomeMap[nIdx];
            if (isWaterDebugBiome(neighborBiome) || isMountainDebugBiome(neighborBiome)) {
              continue;
            }

            neighborSum += nTop;
            ++neighborCount;
            minNeighbor = std::min(minNeighbor, nTop);
            maxNeighbor = std::max(maxNeighbor, nTop);
            if (neighborBiome != biome) {
              ++seamNeighbors;
              if (isAridTransitionPair(biome, neighborBiome)) {
                ++aridNeighbors;
              }
            }
          }
        }

        if (neighborCount <= 1 || seamNeighbors == 0) {
          continue;
        }

        float seamStrength = std::clamp(static_cast<float>(seamNeighbors) / 5.0f +
                                        static_cast<float>(aridNeighbors) * 0.18f,
                                        0.0f,
                                        1.0f);
        int neighborhoodAverage = static_cast<int>(
          std::round(static_cast<float>(neighborSum) / static_cast<float>(neighborCount)));
        int targetTopY = static_cast<int>(std::round(lerpValue(static_cast<float>(topY),
                                                               static_cast<float>(neighborhoodAverage),
                                                               0.16f + seamStrength * 0.28f)));
        int stepAllowance = aridNeighbors > 0 ? 2 : 3;
        targetTopY = std::clamp(targetTopY,
                                minNeighbor - stepAllowance,
                                maxNeighbor + stepAllowance);
        targetTopY = std::clamp(targetTopY, minY, maxY);
        relaxedHeights[idx] = targetTopY;
      }
    }

    for (int lz = 0; lz < kChunkSize; ++lz) {
      for (int lx = 0; lx < kChunkSize; ++lx) {
        size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
        int oldTopY = topHeights[idx];
        int newTopY = relaxedHeights[idx];
        if (newTopY == oldTopY || oldTopY < minY || oldTopY > maxY) {
          continue;
        }

        reshapeColumnTop(lx, lz, oldTopY, newTopY);
        topHeights[idx] = newTopY;
      }
    }
  }

  std::array<int, kChunkColumnCount> coastDistance{};
  coastDistance.fill(6);
  std::array<int, kChunkColumnCount> slopeMap{};
  slopeMap.fill(0);

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      int topY = topHeights[idx];
      if (topY < minY || topY > maxY) {
        continue;
      }

      int bestCoast = 6;
      int maxSlope = 0;
      for (int oz = -3; oz <= 3; ++oz) {
        for (int ox = -3; ox <= 3; ++ox) {
          int nx = lx + ox;
          int nz = lz + oz;
          if (nx < 0 || nx >= kChunkSize || nz < 0 || nz >= kChunkSize) {
            continue;
          }
          size_t nIdx = static_cast<size_t>(nx + nz * kChunkSize);
          int nTop = topHeights[nIdx];
          if (nTop < minY || nTop > maxY) {
            continue;
          }

          maxSlope = std::max(maxSlope, std::abs(nTop - topY));
          uint8_t nBiome = biomeMap[nIdx];
          bool nearWaterBand =
            nTop <= kStageSeaLevel + 1 ||
            nBiome == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
            nBiome == static_cast<uint8_t>(DebugBiomeId::kBeach);
          if (nearWaterBand) {
            int dist = std::max(std::abs(ox), std::abs(oz));
            bestCoast = std::min(bestCoast, dist);
          }
        }
      }

      if (topY <= kStageSeaLevel + 1) {
        bestCoast = 0;
      }
      coastDistance[idx] = bestCoast;
      slopeMap[idx] = maxSlope;
    }
  }

  struct SurfaceRuleResult {
    uint8_t top = kGrass;
    uint8_t filler = kDirt;
    int fillerDepth = 3;
  };
  enum class SurfaceRuleNode : uint8_t {
    kDefault = 0,
    kCoastline = 1,
    kArid = 2,
    kMountain = 3,
    kSnowCap = 4,
    kWindswept = 5
  };

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      int topY = topHeights[idx];
      if (topY < minY || topY > maxY) {
        continue;
      }

      uint8_t biome = biomeMap[idx];
      const BiomeClimateSample& climate = climateMap[idx];
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      float patchNoise = sampleSurfacePatchNoise01(worldX, worldZ, seed);
      float thicknessNoise = sampleSurfaceThicknessNoise(worldX, worldZ, seed);
      int localSlope = slopeMap[idx];
      int coastDist = coastDistance[idx];
      bool biomeWater = biome == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
                        biome == static_cast<uint8_t>(DebugBiomeId::kBeach);
      bool nearCoastline = coastDist <= 1 || topY <= kStageSeaLevel + 1;
      bool broadCoastBand = coastDist <= 2 || biomeWater;
      bool sharpCliff = localSlope >= 6 ||
                        (topY > kStageSeaLevel + 16 &&
                         localSlope >= 4 &&
                         climate.erosion < 0.40f);

      int snowLine = kStageSeaLevel + 20 +
                     static_cast<int>(std::round((0.36f - climate.temperature) * 34.0f +
                                                 (0.50f - climate.erosion) * 8.0f));
      snowLine = std::clamp(snowLine, kStageSeaLevel + 14, maxY - 2);

      SurfaceRuleNode node = SurfaceRuleNode::kDefault;
      if ((biomeWater && broadCoastBand) || nearCoastline) {
        node = SurfaceRuleNode::kCoastline;
      } else if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert) ||
                 (climate.temperature > 0.72f && climate.humidity < 0.36f)) {
        node = SurfaceRuleNode::kArid;
      } else if (!nearCoastline &&
                 topY > kStageSeaLevel + 14 &&
                 (sharpCliff || (climate.weirdness > 0.62f && climate.erosion < 0.42f))) {
        node = SurfaceRuleNode::kWindswept;
      } else if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains) ||
                 (climate.weirdness > 0.62f && topY > kStageSeaLevel + 18)) {
        node = SurfaceRuleNode::kMountain;
      }
      if (topY >= snowLine && climate.temperature < 0.48f) {
        node = SurfaceRuleNode::kSnowCap;
      }

      SurfaceRuleResult rule{};
      switch (node) {
        case SurfaceRuleNode::kCoastline: {
          bool gravelCoast = climate.erosion < 0.26f || localSlope >= 4;
          rule.top = gravelCoast ? kGravel : kSand;
          if (!biomeWater && coastDist >= 2 && topY >= kStageSeaLevel) {
            rule.top = (climate.temperature > 0.66f || gravelCoast) ? rule.top : kGrass;
          }
          rule.filler = gravelCoast ? kGravel : kSand;
          float coastDepth = 2.0f +
                             std::max(0, 2 - coastDist) +
                             (1.0f - climate.erosion) * 1.8f;
          rule.fillerDepth = std::clamp(static_cast<int>(std::round(coastDepth)), 2, 6);
          break;
        }
        case SurfaceRuleNode::kArid:
          rule.top = kSand;
          rule.filler = (climate.erosion < 0.25f) ? kGravel : kSand;
          rule.fillerDepth = std::clamp(4 + static_cast<int>(std::round(climate.depth * 2.2f)), 4, 8);
          break;
        case SurfaceRuleNode::kWindswept:
          rule.top = (topY > kStageSeaLevel + 26 || localSlope >= 7) ? kStone : kGravel;
          rule.filler = (climate.erosion < 0.30f || localSlope >= 7) ? kStone : kGravel;
          rule.fillerDepth = std::clamp(2 + localSlope / 3, 2, 5);
          break;
        case SurfaceRuleNode::kMountain:
          if (topY > kStageSeaLevel + 30 || climate.weirdness > 0.72f || localSlope >= 6) {
            rule.top = kStone;
            rule.filler = kStone;
            rule.fillerDepth = 2 + (localSlope > 5 ? 1 : 0);
          } else {
            rule.top = (climate.temperature < 0.42f && topY > kStageSeaLevel + 22) ? kStone : kGrass;
            rule.filler = (localSlope > 4) ? kGravel : kStone;
            rule.fillerDepth = 3 + (localSlope > 6 ? 1 : 0);
          }
          break;
        case SurfaceRuleNode::kSnowCap:
          rule.top = kStone;
          rule.filler = (localSlope > 6) ? kGravel : kStone;
          rule.fillerDepth = std::clamp(2 + static_cast<int>(std::round((1.0f - climate.erosion) * 2.0f)), 2, 4);
          break;
        case SurfaceRuleNode::kDefault:
        default:
          rule.top = kGrass;
          rule.filler = (climate.erosion < 0.32f && climate.continentalness > 0.52f) ? kGravel : kDirt;
          if (climate.humidity > 0.68f && climate.temperature > 0.30f && climate.temperature < 0.74f) {
            rule.filler = kDirt;
          }
          rule.fillerDepth = std::clamp(3 +
                                        static_cast<int>(std::round(climate.depth * 1.8f)) +
                                        static_cast<int>(std::round(climate.humidity * 1.1f)),
                                        3,
                                        7);
          break;
      }

      int fillerMin = node == SurfaceRuleNode::kArid ? 3 : 1;
      int fillerMax = node == SurfaceRuleNode::kArid ? 9
                      : (node == SurfaceRuleNode::kCoastline ? 7 : 8);
      float thicknessBias =
        thicknessNoise * 1.35f +
        (patchNoise - 0.5f) * 1.6f +
        climate.humidity * 0.32f -
        smooth01((0.44f - climate.erosion) / 0.36f) * 0.24f;
      rule.fillerDepth = std::clamp(rule.fillerDepth +
                                      static_cast<int>(std::round(thicknessBias)),
                                    fillerMin,
                                    fillerMax);

      float slopeExposure = smooth01((static_cast<float>(localSlope) - 2.0f) / 5.0f);
      float heightExposure = smooth01((static_cast<float>(topY - (kStageSeaLevel + 8))) / 18.0f);
      float erosionExposure = smooth01((0.46f - climate.erosion) / 0.30f);
      float stoneExposure = slopeExposure * 0.38f +
                            heightExposure * 0.24f +
                            erosionExposure * 0.18f +
                            smooth01((patchNoise - 0.56f) / 0.30f) * 0.42f;

      switch (node) {
        case SurfaceRuleNode::kCoastline:
          if (!biomeWater &&
              coastDist >= 2 &&
              climate.temperature < 0.66f &&
              patchNoise < 0.34f &&
              localSlope <= 2) {
            rule.top = kGrass;
            rule.filler = kDirt;
            rule.fillerDepth = std::max(rule.fillerDepth, 3);
          } else if (patchNoise > 0.72f && localSlope >= 3) {
            rule.top = kGravel;
            rule.filler = kGravel;
          }
          break;
        case SurfaceRuleNode::kArid:
          if (patchNoise > 0.82f && topY > kStageSeaLevel + 7) {
            rule.top = kStone;
            rule.filler = (localSlope >= 4) ? kStone : kGravel;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 2, 1, 4);
          }
          break;
        case SurfaceRuleNode::kWindswept:
          if (patchNoise < 0.42f && localSlope <= 4) {
            rule.top = kGravel;
            rule.filler = kGravel;
          } else {
            rule.top = kStone;
            rule.filler = (patchNoise < 0.34f) ? kGravel : kStone;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 1, 1, 4);
          }
          break;
        case SurfaceRuleNode::kMountain:
          if (patchNoise < 0.30f && localSlope <= 3 && climate.temperature > 0.40f) {
            rule.top = kGrass;
            rule.filler = kDirt;
            rule.fillerDepth = std::max(rule.fillerDepth, 3);
          } else if (stoneExposure > 0.62f || localSlope >= 5) {
            rule.top = kStone;
            rule.filler = (patchNoise < 0.38f) ? kGravel : kStone;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 1, 1, 4);
          }
          break;
        case SurfaceRuleNode::kSnowCap:
          if (patchNoise < 0.28f && localSlope <= 3) {
            rule.filler = kStone;
            rule.fillerDepth = std::max(rule.fillerDepth, 3);
          } else {
            rule.top = kStone;
            rule.filler = (patchNoise < 0.36f) ? kGravel : kStone;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 1, 1, 4);
          }
          break;
        case SurfaceRuleNode::kDefault:
        default:
          if (stoneExposure > 0.84f && topY > kStageSeaLevel + 7) {
            rule.top = kStone;
            rule.filler = (localSlope >= 5 || patchNoise < 0.34f) ? kGravel : kStone;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 2, 1, 4);
          } else if (stoneExposure > 0.66f && topY > kStageSeaLevel + 5) {
            if (localSlope >= 4 || patchNoise > 0.70f) {
              rule.top = kStone;
            }
            rule.filler = (patchNoise > 0.62f) ? kStone : rule.filler;
            rule.fillerDepth = std::clamp(rule.fillerDepth - 1, 2, 5);
          } else if (patchNoise < 0.28f && climate.humidity > 0.60f) {
            rule.fillerDepth = std::clamp(rule.fillerDepth + 1, 3, 8);
          }
          break;
      }

      if (aprilMode) {
        float prankRoll = hashedNoise01(worldX, topY, worldZ, seed, 0xA941u);
        float prankMix = hashedNoise01(worldX, topY + 19, worldZ, seed, 0xA942u);
        if (rule.top == kGrass) {
          if (prankRoll > 0.95f) {
            rule.top = kWater;
            rule.filler = prankMix > 0.48f ? kSand : kGravel;
            rule.fillerDepth = 1;
          } else if (prankRoll > 0.74f) {
            rule.top = kSand;
            rule.filler = prankMix > 0.58f ? kGravel : kSand;
            rule.fillerDepth = std::max(rule.fillerDepth, 3);
          } else if (prankRoll > 0.49f) {
            rule.top = kStone;
            rule.filler = prankMix > 0.44f ? kStone : kGravel;
            rule.fillerDepth = std::clamp(rule.fillerDepth, 2, 4);
          }
        } else if (rule.top == kSand && prankRoll < 0.22f) {
          rule.top = kGrass;
          rule.filler = kDirt;
          rule.fillerDepth = std::max(rule.fillerDepth, 3);
        } else if (rule.top == kStone && prankMix > 0.84f) {
          rule.top = kGrass;
          rule.filler = kGravel;
          rule.fillerDepth = std::max(rule.fillerDepth, 2);
        }
      }
      if (aprilMode && isCrashDebugBiome(biome)) {
        float crashGlass = hashedNoise01(worldX, topY + 9, worldZ, seed, 0xC2A5u);
        rule.top = kGrass;
        rule.filler = kDirt;
        rule.fillerDepth = std::clamp(rule.fillerDepth, 2, 4);
        if (crashGlass > 0.84f && localSlope <= 3) {
          rule.top = kSuspiciousGlass;
          rule.filler = kStone;
          rule.fillerDepth = 1;
        }
      }

      setLocalBlock(lx, topY, lz, rule.top);
      for (int d = 1; d <= rule.fillerDepth; ++d) {
        int y = topY - d;
        if (y < minY) {
          break;
        }
        uint8_t current = getLocalBlock(lx, y, lz);
        if (current == kAir || isWaterBlock(current)) {
          break;
        }
        if (current == kStone || current == kDirt || current == kGrass || current == kSand || current == kGravel) {
          setLocalBlock(lx, y, lz, rule.filler);
        }
      }

      if (rule.top == kSand && rule.fillerDepth >= 4) {
        for (int d = rule.fillerDepth + 1; d <= rule.fillerDepth + 3; ++d) {
          int y = topY - d;
          if (y < minY) {
            break;
          }
          uint8_t current = getLocalBlock(lx, y, lz);
          if (current == kStone) {
            setLocalBlock(lx, y, lz, kSand);
          }
        }
      } else if (node == SurfaceRuleNode::kDefault ||
                 node == SurfaceRuleNode::kMountain ||
                 node == SurfaceRuleNode::kWindswept) {
        int deepLayers = std::clamp(1 + localSlope / 4, 1, 3);
        for (int d = rule.fillerDepth + 1; d <= rule.fillerDepth + deepLayers; ++d) {
          int y = topY - d;
          if (y < minY) {
            break;
          }
          uint8_t current = getLocalBlock(lx, y, lz);
          if (current == kStone || current == kDirt || current == kGrass || current == kGravel) {
            setLocalBlock(lx,
                          y,
                          lz,
                          (node == SurfaceRuleNode::kDefault && localSlope <= 3) ? kDirt
                                                                                  : (node == SurfaceRuleNode::kWindswept ? kGravel : kStone));
          }
        }
      } else if (node == SurfaceRuleNode::kSnowCap) {
        for (int d = rule.fillerDepth + 1; d <= rule.fillerDepth + 2; ++d) {
          int y = topY - d;
          if (y < minY) {
            break;
          }
          uint8_t current = getLocalBlock(lx, y, lz);
          if (current == kStone || current == kGravel || current == kDirt) {
            setLocalBlock(lx, y, lz, kStone);
          }
        }
      }

      if (topY < kStageSeaLevel &&
          hashedNoise01(worldX, topY, worldZ, seed, 0xC1A0u) > 0.72f) {
        setLocalBlock(lx, topY, lz, kGravel);
      }
    }
  }
}

struct CarverEvent {
  float startX = 0.0f;
  float startY = 0.0f;
  float startZ = 0.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  float baseRadius = 2.0f;
  float verticalScale = 0.90f;
  float yawJitter = 0.12f;
  float pitchJitter = 0.07f;
  float radiusJitter = 0.18f;
  float branchChance = 0.0f;
  int length = 0;
  bool liquid = false;
};

void collectCarverEvents(int chunkX,
                         int chunkZ,
                         int seed,
                         const WorldGenSettings& settings,
                         bool liquid,
                         std::vector<CarverEvent>& outEvents) {
  outEvents.clear();

  float ravineScale = std::clamp(settings.ravineFrequency, 0.25f, 2.5f);
  int minRegionX = floorDiv(chunkX - 3, kCarverRegionSizeChunks);
  int maxRegionX = floorDiv(chunkX + 3, kCarverRegionSizeChunks);
  int minRegionZ = floorDiv(chunkZ - 3, kCarverRegionSizeChunks);
  int maxRegionZ = floorDiv(chunkZ + 3, kCarverRegionSizeChunks);

  constexpr float kPi = 3.14159265358979323846f;
  for (int rz = minRegionZ; rz <= maxRegionZ; ++rz) {
    for (int rx = minRegionX; rx <= maxRegionX; ++rx) {
      uint32_t state = static_cast<uint32_t>(
        hashChunkSeed(seed, rx, rz, liquid ? 0x1A11u : 0xCA11u));
      int attempts = liquid
        ? std::clamp(static_cast<int>(std::lround(1.0f + ravineScale)), 1, 4)
        : std::clamp(static_cast<int>(std::lround(2.0f + ravineScale * 2.1f)), 2, 7);
      for (int i = 0; i < attempts; ++i) {
        float chance = liquid
          ? (0.18f * (0.72f + ravineScale * 0.38f))
          : (0.36f * (0.72f + ravineScale * 0.50f));
        chance = std::clamp(chance, 0.08f, liquid ? 0.46f : 0.74f);
        if (rand01(state) > chance) {
          continue;
        }

        int startChunkX = rx * kCarverRegionSizeChunks + randIntInclusive(state, 0, kCarverRegionSizeChunks - 1);
        int startChunkZ = rz * kCarverRegionSizeChunks + randIntInclusive(state, 0, kCarverRegionSizeChunks - 1);
        float startX = static_cast<float>(startChunkX * kChunkSize + randIntInclusive(state, 0, kChunkSize - 1));
        float startZ = static_cast<float>(startChunkZ * kChunkSize + randIntInclusive(state, 0, kChunkSize - 1));
        bool canyonStyle = !liquid && rand01(state) < (0.26f + 0.10f * ravineScale);
        float startY = liquid
          ? static_cast<float>(randIntInclusive(state, 3, 24))
          : static_cast<float>(randIntInclusive(state,
                                                canyonStyle ? 18 : 10,
                                                canyonStyle ? (kChunkHeight - 14) : (kChunkHeight - 20)));
        float yaw = rand01(state) * 2.0f * kPi;
        float pitch = (rand01(state) - 0.5f) * (liquid ? 0.36f : (canyonStyle ? 0.22f : 0.30f));
        float radius = liquid
          ? (1.4f + rand01(state) * 2.0f)
          : (canyonStyle ? (2.6f + rand01(state) * 2.6f)
                         : (1.8f + rand01(state) * 3.0f));
        int length = liquid
          ? randIntInclusive(state, 22, std::clamp(static_cast<int>(72.0f * ravineScale), 28, 110))
          : randIntInclusive(state,
                             canyonStyle ? 58 : 42,
                             canyonStyle ? std::clamp(static_cast<int>(170.0f * ravineScale), 80, 236)
                                         : std::clamp(static_cast<int>(126.0f * ravineScale), 60, 184));
        float verticalScale = liquid
          ? (0.68f + rand01(state) * 0.22f)
          : (canyonStyle ? (0.46f + rand01(state) * 0.18f)
                         : (0.74f + rand01(state) * 0.22f));
        float yawJitter = liquid
          ? (0.16f + rand01(state) * 0.12f)
          : (canyonStyle ? (0.09f + rand01(state) * 0.08f)
                         : (0.11f + rand01(state) * 0.11f));
        float pitchJitter = liquid
          ? (0.08f + rand01(state) * 0.05f)
          : (canyonStyle ? (0.04f + rand01(state) * 0.05f)
                         : (0.06f + rand01(state) * 0.05f));
        float radiusJitter = canyonStyle
          ? (0.22f + rand01(state) * 0.20f)
          : (0.12f + rand01(state) * 0.16f);
        float branchChance = liquid
          ? 0.0f
          : std::clamp((canyonStyle ? 0.22f : 0.12f) * ravineScale, 0.08f, 0.36f);
        outEvents.push_back({startX,
                             startY,
                             startZ,
                             yaw,
                             pitch,
                             radius,
                             verticalScale,
                             yawJitter,
                             pitchJitter,
                             radiusJitter,
                             branchChance,
                             length,
                             liquid});
      }
    }
  }
}

void runCarverStage(int chunkX,
                    int chunkZ,
                    int seed,
                    const WorldGenSettings& settings,
                    std::vector<uint8_t>& ioBlocks,
                    bool liquidPass) {
  int baseX = chunkX * kChunkSize;
  int baseZ = chunkZ * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  std::vector<CarverEvent> events;
  collectCarverEvents(chunkX, chunkZ, seed, settings, liquidPass, events);

  std::array<int, kChunkColumnCount> aquiferTopByColumn{};
  aquiferTopByColumn.fill(kStageSeaLevel - 1);
  if (liquidPass) {
    for (int lz = 0; lz < kChunkSize; ++lz) {
      for (int lx = 0; lx < kChunkSize; ++lx) {
        int worldX = baseX + lx;
        int worldZ = baseZ + lz;
        ClimateAndBiomeSample climateSample =
          sampleClimateAndBiomeAt(worldX, worldZ, seed, settings);
        int aq = computeAquiferLevelAt(worldX, worldZ, seed, climateSample.climate, minY, maxY);
        aquiferTopByColumn[static_cast<size_t>(lx + lz * kChunkSize)] =
          std::min(aq, kStageSeaLevel - 1);
      }
    }
  }

  constexpr float kPi = 3.14159265358979323846f;
  for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
    const CarverEvent& event = events[eventIndex];
    uint32_t state = static_cast<uint32_t>(
      hashChunkSeed(seed,
                    static_cast<int>(event.startX),
                    static_cast<int>(event.startZ),
                    liquidPass ? 0x1A4Fu : 0xC4A1u ^ static_cast<uint32_t>(eventIndex)));

    float x = event.startX;
    float y = event.startY;
    float z = event.startZ;
    float yaw = event.yaw;
    float pitch = event.pitch;

    for (int step = 0; step < event.length; ++step) {
      float t = (event.length <= 1) ? 0.0f : static_cast<float>(step) / static_cast<float>(event.length - 1);
      float profile = std::sin(t * kPi);
      float localShape = 0.82f + profile * 0.76f;
      float localJitter = (rand01(state) * 2.0f - 1.0f) * event.radiusJitter;
      float radius = std::max(1.05f, event.baseRadius * std::max(0.35f, localShape + localJitter));
      float verticalRadius = std::max(0.85f, radius * event.verticalScale);

      int minWX = static_cast<int>(std::floor(x - radius)) - 1;
      int maxWX = static_cast<int>(std::floor(x + radius)) + 1;
      int minWZ = static_cast<int>(std::floor(z - radius)) - 1;
      int maxWZ = static_cast<int>(std::floor(z + radius)) + 1;
      int minWY = std::max(minY, static_cast<int>(std::floor(y - verticalRadius)) - 1);
      int maxWY = std::min(maxY, static_cast<int>(std::floor(y + verticalRadius)) + 1);

      if (maxWX >= baseX && minWX <= baseX + kChunkSize - 1 &&
          maxWZ >= baseZ && minWZ <= baseZ + kChunkSize - 1 &&
          maxWY >= minY && minWY <= maxY) {
        for (int wy = minWY; wy <= maxWY; ++wy) {
          for (int wz = minWZ; wz <= maxWZ; ++wz) {
            if (wz < baseZ || wz >= baseZ + kChunkSize) {
              continue;
            }
            for (int wx = minWX; wx <= maxWX; ++wx) {
              if (wx < baseX || wx >= baseX + kChunkSize) {
                continue;
              }

              float nx = (static_cast<float>(wx) + 0.5f - x) / radius;
              float ny = (static_cast<float>(wy) + 0.5f - y) / verticalRadius;
              float nz = (static_cast<float>(wz) + 0.5f - z) / radius;
              float dist = nx * nx + ny * ny + nz * nz;
              if (dist >= 1.0f) {
                continue;
              }
              float shell = 1.0f - dist;
              if (!liquidPass && shell < 0.07f && rand01(state) < 0.22f) {
                continue;
              }

              int lx = wx - baseX;
              int lz = wz - baseZ;
              uint8_t current = getLocal(lx, wy, lz);
              if (current == kAir || isWaterBlock(current) || current == kLeaves || current == kWood) {
                continue;
              }

              if (liquidPass) {
                size_t colIndex = static_cast<size_t>(lx + lz * kChunkSize);
                int aquiferTop = aquiferTopByColumn[colIndex];
                if (wy <= aquiferTop) {
                  setLocal(lx, wy, lz, kWater);
                }
              } else {
                setLocal(lx, wy, lz, kAir);
              }
            }
          }
        }
      }

      float speed = liquidPass ? 1.12f : 1.45f;
      x += std::cos(yaw) * std::cos(pitch) * speed;
      y += std::sin(pitch) * speed;
      z += std::sin(yaw) * std::cos(pitch) * speed;
      float bend = 0.45f + 0.55f * profile;
      yaw += (rand01(state) - 0.5f) * event.yawJitter * bend;
      pitch = std::clamp(pitch + (rand01(state) - 0.5f) * event.pitchJitter * bend, -0.65f, 0.65f);

      if (!liquidPass &&
          event.branchChance > 0.0f &&
          step > event.length / 4 &&
          step < (event.length * 3) / 4 &&
          rand01(state) < event.branchChance * 0.075f) {
        float turn = (rand01(state) < 0.5f ? -1.0f : 1.0f) * (0.42f + rand01(state) * 0.60f);
        yaw += turn;
        pitch *= 0.45f;
      }
    }
  }

  applyHeightRangeMask(ioBlocks, minY, maxY);
}

void placeOreFeatures(int cx,
                      int cz,
                      int seed,
                      const WorldGenSettings& settings,
                      const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                      const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                      std::vector<uint8_t>& ioBlocks) {
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  struct OreRule {
    uint8_t type;
    uint32_t salt;
    int attempts;
    int minY;
    int maxY;
    int minVein;
    int maxVein;
    float primaryCenter;
    float primaryWidth;
    float secondaryCenter;
    float secondaryWidth;
    float secondaryWeight;
  };

  std::array<OreRule, 4> rules = {{
    {kCoalOre, 0xC011u, 34, std::max(minY, 6), maxY, 7, 16, 0.82f, 0.28f, 0.55f, 0.24f, 0.60f},
    {kIronOre, 0x1A2Bu, 28, std::max(minY, 4), std::min(maxY, 92), 5, 10, 0.72f, 0.18f, 0.36f, 0.18f, 0.48f},
    {kGoldOre, 0x90D1u, 10, minY, std::min(maxY, 32), 4, 8, 0.15f, 0.12f, 0.26f, 0.10f, 0.34f},
    {kDiamondOre, 0xD14Du, 14, minY, std::min(maxY, 22), 3, 6, 0.08f, 0.08f, 0.16f, 0.06f, 0.26f}
  }};

  for (const OreRule& rule : rules) {
    uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, rule.salt));
    int oreMinY = std::clamp(rule.minY, minY, maxY);
    int oreMaxY = std::clamp(rule.maxY, oreMinY, maxY);

    for (int attempt = 0; attempt < rule.attempts; ++attempt) {
      int lx = randIntInclusive(state, 0, kChunkSize - 1);
      int lz = randIntInclusive(state, 0, kChunkSize - 1);
      int y = randIntInclusive(state, oreMinY, oreMaxY);
      size_t column = static_cast<size_t>(lx + lz * kChunkSize);
      float heightT = inverseLerp(static_cast<float>(minY),
                                  static_cast<float>(maxY),
                                  static_cast<float>(y));
      float heightWeight =
        std::max(sampleBandCurve(heightT, rule.primaryCenter, rule.primaryWidth),
                 sampleBandCurve(heightT, rule.secondaryCenter, rule.secondaryWidth) * rule.secondaryWeight);
      const BiomeClimateSample& climate = climateMap[column];
      uint8_t biome = biomeMap[column];
      float climateBias = 1.0f;
      if (rule.type == kCoalOre) {
        climateBias *= 0.82f + climate.continentalness * 0.34f;
        if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
          climateBias *= 1.12f;
        }
      } else if (rule.type == kIronOre) {
        climateBias *= 0.86f + (1.0f - climate.erosion) * 0.38f;
        if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
          climateBias *= 1.18f;
        }
      } else if (rule.type == kGoldOre) {
        climateBias *= 0.84f + std::max(0.0f, climate.depth - 0.45f) * 0.28f;
        if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
          climateBias *= 1.15f;
        }
      } else if (rule.type == kDiamondOre) {
        climateBias *= 0.92f + climate.continentalness * 0.18f;
      }

      float spawnWeight = std::clamp(heightWeight * climateBias, 0.0f, 1.0f);
      if (rand01(state) > spawnWeight) {
        continue;
      }

      int vein = randIntInclusive(state, rule.minVein, rule.maxVein);
      if (spawnWeight > 0.72f && rand01(state) > 0.55f) {
        vein = std::min(rule.maxVein + 1, vein + 1);
      }

      int px = lx;
      int py = y;
      int pz = lz;
      for (int step = 0; step < vein; ++step) {
        for (int oz = -1; oz <= 1; ++oz) {
          for (int ox = -1; ox <= 1; ++ox) {
            int tx = px + ox;
            int tz = pz + oz;
            int ty = py + randIntInclusive(state, -1, 1);
            if (ty < oreMinY || ty > oreMaxY) {
              continue;
            }
            if (getLocal(tx, ty, tz) == kStone) {
              setLocal(tx, ty, tz, rule.type);
            }
          }
        }
        px += randIntInclusive(state, -1, 1);
        py += randIntInclusive(state, -1, 1);
        pz += randIntInclusive(state, -1, 1);
      }
    }
  }
}

void placeLakeFeatures(int cx,
                       int cz,
                       int seed,
                       const WorldGenSettings& settings,
                       const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                       const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                       std::vector<uint8_t>& ioBlocks) {
  (void)biomeMap;
  (void)climateMap;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  const float seedF = static_cast<float>(seed);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  std::array<int, kChunkColumnCount> topHeights{};
  topHeights.fill(minY - 1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      for (int y = maxY; y >= minY; --y) {
        uint8_t t = getLocal(lx, y, lz);
        if (t != kAir && !isWaterBlock(t) && !isDecorationBlock(t)) {
          topHeights[idx] = y;
          break;
        }
      }
    }
  }

  auto sampleSurfaceTargetAt = [&](int wx, int wz) -> float {
    ClimateAndBiomeSample sample = sampleClimateAndBiomeAt(wx, wz, seed, settings);
    return computeRouterTargetHeight(wx, wz, seed, sample.biome, sample.climate, minY, maxY);
  };

  constexpr int kLakeRegionSizeChunks = 6;
  int minRegionX = floorDiv(cx - 1, kLakeRegionSizeChunks);
  int maxRegionX = floorDiv(cx + 1, kLakeRegionSizeChunks);
  int minRegionZ = floorDiv(cz - 1, kLakeRegionSizeChunks);
  int maxRegionZ = floorDiv(cz + 1, kLakeRegionSizeChunks);

  constexpr float kPi = 3.14159265358979323846f;
  for (int rz = minRegionZ; rz <= maxRegionZ; ++rz) {
    for (int rx = minRegionX; rx <= maxRegionX; ++rx) {
      uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, rx, rz, 0x1A43u));
      int attempts = rand01(state) < 0.30f ? 2 : 1;
      int regionWorldMinX = rx * kLakeRegionSizeChunks * kChunkSize;
      int regionWorldMinZ = rz * kLakeRegionSizeChunks * kChunkSize;
      int regionWorldSpan = kLakeRegionSizeChunks * kChunkSize;

      for (int attempt = 0; attempt < attempts; ++attempt) {
        int centerX = regionWorldMinX + randIntInclusive(state, 18, regionWorldSpan - 18);
        int centerZ = regionWorldMinZ + randIntInclusive(state, 18, regionWorldSpan - 18);

        ClimateAndBiomeSample centerSample = sampleClimateAndBiomeAt(centerX, centerZ, seed, settings);
        uint8_t biome = centerSample.biome;
        const BiomeClimateSample& climate = centerSample.climate;
        if (biome == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
            biome == static_cast<uint8_t>(DebugBiomeId::kBeach) ||
            biome == static_cast<uint8_t>(DebugBiomeId::kCrash) ||
            biome == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
          continue;
        }

        float spawnChance = 0.0f;
        if (biome == static_cast<uint8_t>(DebugBiomeId::kForest)) {
          spawnChance = 0.72f;
        } else if (biome == static_cast<uint8_t>(DebugBiomeId::kPlains)) {
          spawnChance = 0.46f;
        } else if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
          spawnChance = 0.18f;
        }
        spawnChance *= 0.70f + climate.humidity * 0.70f;
        spawnChance *= 0.76f + climate.erosion * 0.34f;
        if (rand01(state) > std::clamp(spawnChance, 0.0f, 0.80f)) {
          continue;
        }

        int radiusX = randIntInclusive(state, biome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 7 : 6,
                                       biome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 11 : 9);
        int radiusZ = randIntInclusive(state, biome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 7 : 6,
                                       biome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 11 : 9);
        int rimRadiusX = radiusX + 4;
        int rimRadiusZ = radiusZ + 4;
        float rotation = rand01(state) * kPi;
        float cosRot = std::cos(rotation);
        float sinRot = std::sin(rotation);
        int centerHeight = static_cast<int>(std::round(sampleSurfaceTargetAt(centerX, centerZ)));
        if (centerHeight < kStageSeaLevel + 3 || centerHeight > maxY - 10) {
          continue;
        }

        float innerSum = 0.0f;
        float ringSum = 0.0f;
        float ringMin = std::numeric_limits<float>::max();
        int innerCount = 0;
        int ringCount = 0;
        for (int oz = -rimRadiusZ; oz <= rimRadiusZ; oz += 2) {
          for (int ox = -rimRadiusX; ox <= rimRadiusX; ox += 2) {
            float dx = static_cast<float>(ox);
            float dz = static_cast<float>(oz);
            float rxNorm = (dx * cosRot - dz * sinRot) / static_cast<float>(std::max(1, radiusX));
            float rzNorm = (dx * sinRot + dz * cosRot) / static_cast<float>(std::max(1, radiusZ));
            float dist = rxNorm * rxNorm + rzNorm * rzNorm;
            if (dist > 1.55f) {
              continue;
            }

            float height = sampleSurfaceTargetAt(centerX + ox, centerZ + oz);
            if (dist <= 0.38f) {
              innerSum += height;
              ++innerCount;
            } else if (dist >= 0.92f && dist <= 1.45f) {
              ringSum += height;
              ringMin = std::min(ringMin, height);
              ++ringCount;
            }
          }
        }

        if (innerCount < 6 || ringCount < 10) {
          continue;
        }

        float innerAvg = innerSum / static_cast<float>(innerCount);
        float ringAvg = ringSum / static_cast<float>(ringCount);
        float basinDepth = ringAvg - innerAvg;
        if (ringMin < innerAvg + 2.5f || basinDepth < 3.0f) {
          continue;
        }

        int waterLevel = static_cast<int>(std::round(std::min(ringMin - 1.0f,
                                                              innerAvg + 2.2f + climate.humidity * 1.8f)));
        waterLevel = std::clamp(waterLevel, minY + 2, maxY - 5);
        int maxDepth = randIntInclusive(state, 3, 5) +
                       (biome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 1 : 0);

        int lakeMinX = centerX - rimRadiusX - 1;
        int lakeMaxX = centerX + rimRadiusX + 1;
        int lakeMinZ = centerZ - rimRadiusZ - 1;
        int lakeMaxZ = centerZ + rimRadiusZ + 1;
        if (lakeMaxX < baseX || lakeMinX > baseX + kChunkSize - 1 ||
            lakeMaxZ < baseZ || lakeMinZ > baseZ + kChunkSize - 1) {
          continue;
        }

        for (int wz = std::max(baseZ, lakeMinZ); wz <= std::min(baseZ + kChunkSize - 1, lakeMaxZ); ++wz) {
          for (int wx = std::max(baseX, lakeMinX); wx <= std::min(baseX + kChunkSize - 1, lakeMaxX); ++wx) {
            int lx = wx - baseX;
            int lz = wz - baseZ;
            size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
            int localTop = topHeights[idx];
            if (localTop < minY + 1 || localTop > maxY - 2) {
              continue;
            }

            float dx = static_cast<float>(wx - centerX);
            float dz = static_cast<float>(wz - centerZ);
            float rxNorm = (dx * cosRot - dz * sinRot) / static_cast<float>(std::max(1, radiusX));
            float rzNorm = (dx * sinRot + dz * cosRot) / static_cast<float>(std::max(1, radiusZ));
            float dist = rxNorm * rxNorm + rzNorm * rzNorm;
            float edgeNoise = glm::perlin(glm::vec2(
              (static_cast<float>(wx) + seedF * 71.0f) * 0.090f,
              (static_cast<float>(wz) - seedF * 67.0f) * 0.090f));
            dist += edgeNoise * 0.11f;
            if (dist > 1.52f) {
              continue;
            }

            uint8_t rimType = (climate.temperature > 0.70f || rand01(state) < 0.34f) ? kSand : kGravel;
            uint8_t floorType = climate.temperature > 0.70f ? kSand
                                                            : (rand01(state) < 0.55f ? kGravel : kDirt);

            if (dist <= 1.08f) {
              float centerT = std::clamp(1.0f - dist / 1.08f, 0.0f, 1.0f);
              int carveDepth = 2 + static_cast<int>(std::round(std::pow(centerT, 1.45f) * maxDepth));
              int floorY = std::max(minY + 1, waterLevel - carveDepth);

              for (int y = localTop; y > waterLevel; --y) {
                uint8_t current = getLocal(lx, y, lz);
                if (current != kAir && !isWaterBlock(current)) {
                  setLocal(lx, y, lz, kAir);
                }
              }

              int shoreShelf = dist > 0.78f ? 1 : 0;
              floorY = std::min(waterLevel - 1, floorY + shoreShelf);
              for (int y = waterLevel; y > floorY; --y) {
                setLocal(lx, y, lz, kWater);
              }
              setLocal(lx, floorY, lz, floorType);
              if (floorY - 1 >= minY && getLocal(lx, floorY - 1, lz) == kStone) {
                setLocal(lx, floorY - 1, lz, floorType);
              }
              topHeights[idx] = floorY;
              continue;
            }

            if (localTop <= waterLevel + 2) {
              int newTop = localTop;
              if (localTop > waterLevel + 1) {
                for (int y = localTop; y > waterLevel + 1; --y) {
                  uint8_t current = getLocal(lx, y, lz);
                  if (current != kAir && !isWaterBlock(current)) {
                    setLocal(lx, y, lz, kAir);
                  }
                }
                newTop = waterLevel + 1;
              }
              if (newTop >= minY && newTop < kChunkHeight) {
                setLocal(lx, newTop, lz, rimType);
                topHeights[idx] = newTop;
              }
            }
          }
        }
      }
    }
  }
}

void placeCrashIslandFeatures(int cx,
                              int cz,
                              int seed,
                              const WorldGenSettings& settings,
                              std::vector<uint8_t>& ioBlocks) {
  if (!isAprilFoolsPreset(settings)) {
    return;
  }

  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  constexpr int kCrashIslandRegionSizeChunks = 4;
  int minRegionX = floorDiv(cx - 2, kCrashIslandRegionSizeChunks);
  int maxRegionX = floorDiv(cx + 2, kCrashIslandRegionSizeChunks);
  int minRegionZ = floorDiv(cz - 2, kCrashIslandRegionSizeChunks);
  int maxRegionZ = floorDiv(cz + 2, kCrashIslandRegionSizeChunks);

  for (int rz = minRegionZ; rz <= maxRegionZ; ++rz) {
    for (int rx = minRegionX; rx <= maxRegionX; ++rx) {
      uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, rx, rz, 0xC2A5u));
      int islandCount = rand01(state) < 0.58f ? 2 : 1;
      int regionWorldMinX = rx * kCrashIslandRegionSizeChunks * kChunkSize;
      int regionWorldMinZ = rz * kCrashIslandRegionSizeChunks * kChunkSize;
      int regionWorldSpan = kCrashIslandRegionSizeChunks * kChunkSize;

      for (int island = 0; island < islandCount; ++island) {
        int centerX = regionWorldMinX + randIntInclusive(state, 10, regionWorldSpan - 10);
        int centerZ = regionWorldMinZ + randIntInclusive(state, 10, regionWorldSpan - 10);
        ClimateAndBiomeSample centerSample = sampleClimateAndBiomeAt(centerX, centerZ, seed, settings);
        if (!isCrashDebugBiome(centerSample.biome)) {
          continue;
        }

        float spawnChance = std::clamp(0.42f + centerSample.climate.weirdness * 0.40f, 0.0f, 0.86f);
        if (rand01(state) > spawnChance) {
          continue;
        }

        int centerY = kStageSeaLevel + 34 +
                      randIntInclusive(state, -4, 10) +
                      static_cast<int>(std::round((centerSample.climate.depth - 0.5f) * 14.0f +
                                                  (centerSample.climate.weirdness - 0.5f) * 18.0f));
        centerY = std::clamp(centerY, minY + 18, maxY - 10);
        int radiusX = randIntInclusive(state, 7, 13);
        int radiusZ = randIntInclusive(state, 7, 13);
        int capHeight = randIntInclusive(state, 2, 4);
        int bellyDepth = randIntInclusive(state, 6, 10);

        int islandMinX = centerX - radiusX - 2;
        int islandMaxX = centerX + radiusX + 2;
        int islandMinZ = centerZ - radiusZ - 2;
        int islandMaxZ = centerZ + radiusZ + 2;
        if (islandMaxX < baseX || islandMinX > baseX + kChunkSize - 1 ||
            islandMaxZ < baseZ || islandMinZ > baseZ + kChunkSize - 1) {
          continue;
        }

        for (int wz = std::max(baseZ, islandMinZ); wz <= std::min(baseZ + kChunkSize - 1, islandMaxZ); ++wz) {
          for (int wx = std::max(baseX, islandMinX); wx <= std::min(baseX + kChunkSize - 1, islandMaxX); ++wx) {
            int lx = wx - baseX;
            int lz = wz - baseZ;
            float nx = static_cast<float>(wx - centerX) / static_cast<float>(std::max(1, radiusX));
            float nz = static_cast<float>(wz - centerZ) / static_cast<float>(std::max(1, radiusZ));
            float edgeNoise =
              static_cast<float>(seededPerlin2D(wx, wz, seed, 239u, 241u, 0.085, 0.085)) * 0.11f;
            float dist = nx * nx + nz * nz + edgeNoise;
            if (dist > 1.04f) {
              continue;
            }

            float fullness = std::clamp(1.0f - dist / 1.04f, 0.0f, 1.0f);
            int topY = centerY + static_cast<int>(std::round(std::pow(fullness, 1.4f) * capHeight));
            int bottomY = centerY - 1 - static_cast<int>(std::round(std::pow(fullness, 0.58f) * bellyDepth));
            int dirtDepth = fullness > 0.45f ? 2 : 1;
            float glassPatch = hashedNoise01(wx, centerY, wz, seed, 0xC2A6u);

            for (int y = bottomY; y <= topY; ++y) {
              if (y < minY || y > maxY) {
                continue;
              }

              uint8_t type = kStone;
              if (y == topY && fullness > 0.18f) {
                type = glassPatch > 0.86f ? kSuspiciousGlass : kGrass;
              } else if (y >= topY - dirtDepth && fullness > 0.18f) {
                type = kDirt;
              } else if (y >= topY - dirtDepth - 1 && glassPatch > 0.93f) {
                type = kSuspiciousGlass;
              }

              uint8_t current = getLocal(lx, y, lz);
              if (current == kAir || isWaterBlock(current) ||
                  current == kLeaves || current == kWood ||
                  current == kStone || current == kDirt ||
                  current == kGrass || current == kSuspiciousGlass) {
                setLocal(lx, y, lz, type);
              }
            }

            float spikeRoll = hashedNoise01(wx, centerY + 23, wz, seed, 0xC2A7u);
            if (fullness > 0.18f && fullness < 0.72f && spikeRoll > 0.78f) {
              int spikeLen =
                2 + static_cast<int>(std::floor(spikeRoll * 4.0f)) +
                static_cast<int>(std::round((1.0f - fullness) * 3.0f));
              for (int s = 1; s <= spikeLen; ++s) {
                int y = bottomY - s;
                if (y < minY) {
                  break;
                }
                float taperNoise = hashedNoise01(wx, y, wz, seed, 0xC2A8u);
                if (taperNoise + fullness * 0.55f < 0.42f) {
                  break;
                }
                uint8_t current = getLocal(lx, y, lz);
                if (current == kAir || isWaterBlock(current)) {
                  setLocal(lx, y, lz, kStone);
                }
              }
            }
          }
        }
      }
    }
  }
}

void placeTreeFeatures(int cx,
                       int cz,
                       int seed,
                       const WorldGenSettings& settings,
                       const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                       const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                       std::vector<uint8_t>& ioBlocks) {
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);
  bool aprilMode = isAprilFoolsPreset(settings);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto canReplaceTreeBlock = [&](uint8_t type) -> bool {
    return type == kAir || isWaterBlock(type) || type == kLeaves;
  };

  auto isStoneTreeAt = [&](int worldX, int topY, int worldZ) -> bool {
    if (!aprilMode) {
      return false;
    }
    return hashedNoise01(worldX, topY, worldZ, seed, 0xA611u) < 0.20f;
  };

  auto treeFoliageBlockAt = [&](int worldX, int topY, int worldZ, bool stoneTree) -> uint8_t {
    if (!aprilMode) {
      return kLeaves;
    }
    if (stoneTree) {
      return kLeaves;
    }
    float prank = hashedNoise01(worldX, topY + 17, worldZ, seed, 0xA91Fu);
    if (prank > 0.84f) {
      return kSuspiciousGlass;
    }
    if (prank > 0.58f) {
      return kWood;
    }
    return kLeaves;
  };

  auto tryPlaceDryTree = [&](int lx, int topY, int lz, uint32_t& rngState, bool stoneTree) -> bool {
    static constexpr std::array<glm::ivec2, 4> kCardinalDirs = {{
      {1, 0},
      {-1, 0},
      {0, 1},
      {0, -1}
    }};

    int trunkHeight = randIntInclusive(rngState, 3, 5);
    int leanDirIndex = randIntInclusive(rngState, 0, 3);
    glm::ivec2 leanDir = kCardinalDirs[static_cast<size_t>(leanDirIndex)];
    int leanStart = trunkHeight >= 4 ? randIntInclusive(rngState, trunkHeight - 2, trunkHeight - 1)
                                     : trunkHeight;
    int branchCount = (trunkHeight >= 4 && rand01(rngState) < 0.58f) ? 2 : 1;
    int primaryBranchIndex = randIntInclusive(rngState, 0, 3);
    int secondaryBranchIndex = (primaryBranchIndex + ((rand01(rngState) < 0.5f) ? 1 : 3)) % 4;
    int primaryBranchLength = randIntInclusive(rngState, 1, 2);
    int secondaryBranchLength = randIntInclusive(rngState, 1, 2);

    std::array<glm::ivec3, 8> trunkBlocks{};
    int currentX = lx;
    int currentZ = lz;
    for (int segment = 1; segment <= trunkHeight; ++segment) {
      if (segment == leanStart) {
        currentX += leanDir.x;
        currentZ += leanDir.y;
      }
      trunkBlocks[static_cast<size_t>(segment)] = {currentX, topY + segment, currentZ};
    }

    std::vector<glm::ivec3> woodBlocks;
    woodBlocks.reserve(10);
    for (int segment = 1; segment <= trunkHeight; ++segment) {
      woodBlocks.push_back(trunkBlocks[static_cast<size_t>(segment)]);
    }

    glm::ivec3 topBlock = trunkBlocks[static_cast<size_t>(trunkHeight)];
    woodBlocks.push_back({topBlock.x, topBlock.y + 1, topBlock.z});

    auto appendBranch = [&](int branchDirIndex, int branchLength, int branchBaseOffset) {
      glm::ivec2 dir = kCardinalDirs[static_cast<size_t>(branchDirIndex)];
      int baseSegment = std::max(2, trunkHeight - branchBaseOffset);
      glm::ivec3 base = trunkBlocks[static_cast<size_t>(baseSegment)];
      for (int step = 1; step <= branchLength; ++step) {
        int tx = base.x + dir.x * step;
        int tz = base.z + dir.y * step;
        int ty = base.y + (step == branchLength ? 1 : 0);
        woodBlocks.push_back({tx, ty, tz});
      }
    };

    appendBranch(primaryBranchIndex, primaryBranchLength, 1);
    if (branchCount > 1) {
      appendBranch(secondaryBranchIndex, secondaryBranchLength, 2);
    }

    for (const glm::ivec3& block : woodBlocks) {
      if (block.x < 0 || block.x >= kChunkSize || block.z < 0 || block.z >= kChunkSize ||
          block.y < minY || block.y > maxY) {
        return false;
      }
      uint8_t current = getLocal(block.x, block.y, block.z);
      if (!canReplaceTreeBlock(current)) {
        return false;
      }
    }

    for (const glm::ivec3& block : woodBlocks) {
      setLocal(block.x, block.y, block.z, stoneTree ? kStone : kWood);
    }
    return true;
  };

  std::array<int, kChunkColumnCount> topHeights{};
  topHeights.fill(minY - 1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      for (int y = maxY; y >= minY; --y) {
        uint8_t t = getLocal(lx, y, lz);
        if (t != kAir && !isWaterBlock(t)) {
          topHeights[idx] = y;
          break;
        }
      }
    }
  }

  uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, 0x7AEEu));
  int attempts = 16;
  for (int i = 0; i < attempts; ++i) {
    int lx = randIntInclusive(state, 1, kChunkSize - 2);
    int lz = randIntInclusive(state, 1, kChunkSize - 2);
    size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
    int topY = topHeights[idx];
    if (topY < minY + 1 || topY > maxY - 8) {
      continue;
    }

    uint8_t ground = getLocal(lx, topY, lz);
    if (ground != kGrass && ground != kDirt && ground != kSand) {
      continue;
    }

    uint8_t biome = biomeMap[idx];
    const BiomeClimateSample& climate = climateMap[idx];
    bool desertBiome = biome == static_cast<uint8_t>(DebugBiomeId::kDesert);
    bool hotDryClimate = climate.temperature > 0.74f && climate.humidity < 0.38f;
    bool desertLike = desertBiome || (ground == kSand && hotDryClimate);
    bool dryTreeCandidate = desertLike && ground == kSand;
    float chance = 0.10f;
    if (biome == static_cast<uint8_t>(DebugBiomeId::kForest)) {
      chance = 0.34f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kPlains)) {
      chance = 0.08f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
      chance = 0.016f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kBeach) ||
               biome == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
      chance = 0.0f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
      chance = 0.05f;
    } else if (isCrashDebugBiome(biome)) {
      chance = 0.18f;
    }
    if (dryTreeCandidate) {
      chance = 0.014f;
    }
    chance *= (0.68f + climate.humidity * 0.64f);
    chance *= (0.74f + (1.0f - climate.erosion) * 0.42f);
    if (ground == kSand && !dryTreeCandidate) {
      chance *= 0.10f;
    }
    if (hotDryClimate) {
      chance *= dryTreeCandidate ? 0.72f : 0.18f;
    }
    chance = std::clamp(chance, 0.0f, 0.62f);

    if (rand01(state) > chance) {
      continue;
    }

    int worldX = baseX + lx;
    int worldZ = baseZ + lz;
    bool stoneTree = isStoneTreeAt(worldX, topY, worldZ);

    if (dryTreeCandidate) {
      (void)tryPlaceDryTree(lx, topY, lz, state, stoneTree);
      continue;
    }

    int trunkHeight = randIntInclusive(state, 4, 6);
    if (climate.humidity > 0.64f && climate.temperature > 0.36f) {
      trunkHeight += 1;
    }
    if (climate.erosion < 0.34f) {
      trunkHeight += 1;
    }
    trunkHeight = std::clamp(trunkHeight, 4, 7);
    bool clear = true;
    for (int y = topY + 1; y <= topY + trunkHeight + 2; ++y) {
      if (getLocal(lx, y, lz) != kAir) {
        clear = false;
        break;
      }
    }
    if (!clear) {
      continue;
    }

    uint8_t trunkType = stoneTree ? kStone : kWood;
    uint8_t foliageType = treeFoliageBlockAt(worldX, topY, worldZ, stoneTree);

    for (int y = topY + 1; y <= topY + trunkHeight; ++y) {
      setLocal(lx, y, lz, trunkType);
    }

    int crownY = topY + trunkHeight;
    for (int dy = -2; dy <= 2; ++dy) {
      int y = crownY + dy;
      if (y < minY || y > maxY) {
        continue;
      }
      int radius = (dy <= 0) ? 2 : 1;
      if (dy == 2) {
        radius = 0;
      }
      for (int oz = -radius; oz <= radius; ++oz) {
        for (int ox = -radius; ox <= radius; ++ox) {
          int tx = lx + ox;
          int tz = lz + oz;
          if (tx < 0 || tx >= kChunkSize || tz < 0 || tz >= kChunkSize) {
            continue;
          }
          if (std::abs(ox) == 2 && std::abs(oz) == 2 && dy <= 0) {
            continue;
          }
          uint8_t current = getLocal(tx, y, tz);
          if (current == kAir || isWaterBlock(current)) {
            setLocal(tx, y, tz, foliageType);
          }
        }
      }
    }

    if (hashedNoise01(worldX, topY, worldZ, seed, 0x4C41u) > 0.82f && topY + trunkHeight + 1 <= maxY) {
      setLocal(lx, topY + trunkHeight + 1, lz, foliageType);
    }
  }
}

void placeUnderwaterPlantFeatures(int cx,
                                  int cz,
                                  int seed,
                                  const WorldGenSettings& settings,
                                  const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                                  std::vector<uint8_t>& ioBlocks) {
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);
  const float seedF = static_cast<float>(seed);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, 0xA91Fu));

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      int topY = minY - 1;
      for (int y = maxY - 1; y >= minY; --y) {
        uint8_t t = getLocal(lx, y, lz);
        if (t != kAir && !isWaterBlock(t) && !isDecorationBlock(t)) {
          topY = y;
          break;
        }
      }

      if (topY < minY || topY + 1 > maxY) {
        continue;
      }

      uint8_t ground = getLocal(lx, topY, lz);
      bool canPlaceSeagrass = canSupportUnderwaterPlant(kSeagrass, ground);
      bool canPlaceCoral = canSupportUnderwaterPlant(kCoral, ground);
      if (!canPlaceSeagrass && !canPlaceCoral) {
        continue;
      }

      if (!isWaterBlock(getLocal(lx, topY + 1, lz))) {
        continue;
      }

      int waterDepth = 0;
      for (int y = topY + 1; y <= maxY; ++y) {
        if (!isWaterBlock(getLocal(lx, y, lz))) {
          break;
        }
        ++waterDepth;
      }
      if (waterDepth <= 0) {
        continue;
      }

      uint8_t biome = biomeMap[idx];
      float chance = 0.06f;
      if (biome == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
        chance = 0.22f;
      } else if (biome == static_cast<uint8_t>(DebugBiomeId::kBeach)) {
        chance = 0.14f;
      }

      if (ground == kSand || ground == kGravel) {
        chance += 0.04f;
      }
      if (waterDepth >= 4) {
        chance += 0.03f;
      } else if (waterDepth <= 1) {
        chance *= 0.35f;
      }

      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      float cluster = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 71.0f) * 0.036f,
        (static_cast<float>(worldZ) - seedF * 67.0f) * 0.036f));
      chance *= (0.70f + cluster * 0.65f);
      chance = std::clamp(chance, 0.0f, 0.58f);
      if (rand01(state) > chance) {
        continue;
      }

      float coralChance = canPlaceCoral ? 0.06f : 0.0f;
      if (ground == kGravel || ground == kStone) {
        coralChance += 0.18f;
      }
      if (waterDepth >= 4) {
        coralChance += 0.06f;
      }
      coralChance = std::clamp(coralChance, 0.0f, 0.42f);
      if (canPlaceCoral && rand01(state) < coralChance) {
        setLocal(lx, topY + 1, lz, kCoral);
        continue;
      }

      if (!canPlaceSeagrass) {
        continue;
      }

      int maxPlantHeight = std::min(waterDepth, 4);
      int plantHeight = 1;
      if (waterDepth >= 3 && rand01(state) < 0.42f) {
        plantHeight = std::min(maxPlantHeight, randIntInclusive(state, 2, 4));
      }

      for (int h = 0; h < plantHeight; ++h) {
        int py = topY + 1 + h;
        if (!isWaterBlock(getLocal(lx, py, lz))) {
          break;
        }
        setLocal(lx, py, lz, kSeagrass);
      }
    }
  }
}

void placeRockOutcropFeatures(int cx,
                              int cz,
                              int seed,
                              const WorldGenSettings& settings,
                              const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                              const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                              std::vector<uint8_t>& ioBlocks) {
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  std::array<int, kChunkColumnCount> topHeights{};
  topHeights.fill(minY - 1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      for (int y = maxY; y >= minY; --y) {
        uint8_t t = getLocal(lx, y, lz);
        if (t != kAir && !isWaterBlock(t) && !isDecorationBlock(t)) {
          topHeights[idx] = y;
          break;
        }
      }
    }
  }

  uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, 0xB04Du));
  int attempts = 9;
  for (int i = 0; i < attempts; ++i) {
    int lx = randIntInclusive(state, 2, kChunkSize - 3);
    int lz = randIntInclusive(state, 2, kChunkSize - 3);
    size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
    int topY = topHeights[idx];
    if (topY < minY + 1 || topY > maxY - 5) {
      continue;
    }

    uint8_t biome = biomeMap[idx];
    if (isCrashDebugBiome(biome)) {
      continue;
    }
    const BiomeClimateSample& climate = climateMap[idx];
    float chance = 0.04f;
    if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
      chance = 0.30f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
      chance = 0.12f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kPlains)) {
      chance = 0.05f;
    }
    chance *= (0.70f + (1.0f - climate.erosion) * 0.75f);
    chance = std::clamp(chance, 0.0f, 0.52f);
    if (rand01(state) > chance) {
      continue;
    }

    uint8_t ground = getLocal(lx, topY, lz);
    if (ground == kAir || isWaterBlock(ground) || isDecorationBlock(ground)) {
      continue;
    }

    int radius = randIntInclusive(state, 1, biome == static_cast<uint8_t>(DebugBiomeId::kMountains) ? 3 : 2);
    int height = randIntInclusive(state, 1, 2 + radius);
    for (int oy = 0; oy <= height; ++oy) {
      for (int oz = -radius; oz <= radius; ++oz) {
        for (int ox = -radius; ox <= radius; ++ox) {
          int tx = lx + ox;
          int tz = lz + oz;
          int ty = topY + oy;
          if (tx < 0 || tx >= kChunkSize || tz < 0 || tz >= kChunkSize || ty > maxY) {
            continue;
          }

          float nx = static_cast<float>(ox) / static_cast<float>(std::max(1, radius));
          float nz = static_cast<float>(oz) / static_cast<float>(std::max(1, radius));
          float ny = static_cast<float>(oy) / static_cast<float>(std::max(1, height));
          float dist = nx * nx + nz * nz + ny * ny * 1.3f;
          float rough = rand01(state) * 0.22f;
          if (dist > 1.0f + rough) {
            continue;
          }

          uint8_t cur = getLocal(tx, ty, tz);
          if (cur == kAir || isWaterBlock(cur) || cur == kLeaves || cur == kWood ||
              cur == kGrass || cur == kDirt || cur == kSand || cur == kGravel || cur == kStone) {
            uint8_t type = (rand01(state) < 0.24f) ? kGravel : kStone;
            setLocal(tx, ty, tz, type);
          }
        }
      }
    }
  }
}

void placeShorelineDriftFeatures(int cx,
                                 int cz,
                                 int seed,
                                 const WorldGenSettings& settings,
                                 const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                                 std::vector<uint8_t>& ioBlocks) {
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  auto getLocal = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  auto setLocal = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  std::array<int, kChunkColumnCount> topHeights{};
  topHeights.fill(minY - 1);
  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
      for (int y = maxY; y >= minY; --y) {
        uint8_t t = getLocal(lx, y, lz);
        if (t != kAir && !isWaterBlock(t) && !isDecorationBlock(t)) {
          topHeights[idx] = y;
          break;
        }
      }
    }
  }

  uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, 0xD12Fu));
  int attempts = 7;
  for (int i = 0; i < attempts; ++i) {
    int lx = randIntInclusive(state, 1, kChunkSize - 2);
    int lz = randIntInclusive(state, 1, kChunkSize - 2);
    size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
    int topY = topHeights[idx];
    if (topY < minY + 1 || topY > maxY - 3) {
      continue;
    }

    if (topY < kStageSeaLevel - 2 || topY > kStageSeaLevel + 2) {
      continue;
    }

    uint8_t biome = biomeMap[idx];
    float chance = 0.02f;
    if (biome == static_cast<uint8_t>(DebugBiomeId::kBeach)) {
      chance = 0.22f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
      chance = 0.10f;
    }
    if (rand01(state) > chance) {
      continue;
    }

    uint8_t ground = getLocal(lx, topY, lz);
    if (ground != kSand && ground != kGravel && ground != kDirt) {
      continue;
    }

    bool alongX = rand01(state) < 0.5f;
    int length = randIntInclusive(state, 2, 4);
    for (int t = 0; t < length; ++t) {
      int tx = lx + (alongX ? t : 0);
      int tz = lz + (alongX ? 0 : t);
      if (tx < 0 || tx >= kChunkSize || tz < 0 || tz >= kChunkSize) {
        break;
      }
      uint8_t at = getLocal(tx, topY + 1, tz);
      if (at != kAir && !isWaterBlock(at)) {
        continue;
      }
      setLocal(tx, topY + 1, tz, kWood);
    }
  }
}

void placeRegionalStructureFeatures(int cx,
                                    int cz,
                                    int seed,
                                    const WorldGenSettings& settings,
                                    std::vector<uint8_t>& ioBlocks) {
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  int minY = std::clamp(settings.minY, 0, kChunkHeight - 1);
  int maxY = std::clamp(settings.maxY, minY, kChunkHeight - 1);

  auto setWorldIfInChunk = [&](int wx, int y, int wz, uint8_t type) {
    if (y < minY || y > maxY) {
      return;
    }
    int lx = wx - baseX;
    int lz = wz - baseZ;
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize) {
      return;
    }
    ioBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto pickMasonry = [&](uint32_t& state) {
    return rand01(state) < 0.28f ? kGravel : kStone;
  };

  enum class StructureType : uint8_t {
    kRuinRing = 0,
    kPillarSanctum = 1,
    kWatchtowerRemnant = 2
  };

  struct StructureCandidate {
    int centerX = 0;
    int centerZ = 0;
    int foundationY = 0;
    int avgSurfaceY = 0;
    int minSurfaceY = 0;
    int maxSurfaceY = 0;
    int extent = 5;
    float score = 0.0f;
    uint8_t biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
    BiomeClimateSample climate{};
    TerrainRouterSample router{};
    StructureType type = StructureType::kRuinRing;
  };

  auto estimateSurfaceAt = [&](int wx,
                               int wz,
                               uint8_t* outBiome,
                               BiomeClimateSample* outClimate,
                               TerrainRouterSample* outRouter) {
    ClimateAndBiomeSample sample = sampleClimateAndBiomeAt(wx, wz, seed, settings);
    TerrainRouterSample router =
      sampleTerrainRouterAt(wx, wz, seed, sample.biome, sample.climate, minY, maxY);
    if (outBiome) {
      *outBiome = sample.biome;
    }
    if (outClimate) {
      *outClimate = sample.climate;
    }
    if (outRouter) {
      *outRouter = router;
    }
    return std::clamp(static_cast<int>(std::round(router.finalHeight)), minY, maxY);
  };

  auto chooseStructureCandidate = [&](int centerX,
                                      int centerZ,
                                      StructureCandidate& outCandidate) {
    uint8_t centerBiome = static_cast<uint8_t>(DebugBiomeId::kPlains);
    BiomeClimateSample centerClimate{};
    TerrainRouterSample centerRouter{};
    int centerSurfaceY = estimateSurfaceAt(centerX,
                                           centerZ,
                                           &centerBiome,
                                           &centerClimate,
                                           &centerRouter);
    if (centerBiome == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
        centerBiome == static_cast<uint8_t>(DebugBiomeId::kCrash)) {
      return false;
    }

    int minSurface = centerSurfaceY;
    int maxSurface = centerSurfaceY;
    int surfaceSum = 0;
    int sampleCount = 0;
    int coastalSamples = 0;
    int mountainSamples = 0;
    int oceanSamples = 0;
    int steepSamples = 0;

    for (int oz = -4; oz <= 4; oz += 2) {
      for (int ox = -4; ox <= 4; ox += 2) {
        uint8_t sampleBiome = static_cast<uint8_t>(DebugBiomeId::kPlains);
        BiomeClimateSample sampleClimate{};
        TerrainRouterSample sampleRouter{};
        int sampleY = estimateSurfaceAt(centerX + ox,
                                        centerZ + oz,
                                        &sampleBiome,
                                        &sampleClimate,
                                        &sampleRouter);
        minSurface = std::min(minSurface, sampleY);
        maxSurface = std::max(maxSurface, sampleY);
        surfaceSum += sampleY;
        ++sampleCount;

        if (sampleBiome == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
          ++oceanSamples;
        }
        if (sampleBiome == static_cast<uint8_t>(DebugBiomeId::kBeach) ||
            sampleY <= kStageSeaLevel + 2) {
          ++coastalSamples;
        }
        if (sampleBiome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
          ++mountainSamples;
        }
        if (std::abs(sampleY - centerSurfaceY) >= 4) {
          ++steepSamples;
        }
      }
    }

    if (sampleCount == 0 || oceanSamples > 3) {
      return false;
    }

    int avgSurfaceY = static_cast<int>(std::lround(static_cast<float>(surfaceSum) /
                                                   static_cast<float>(sampleCount)));
    int slopeRange = maxSurface - minSurface;
    float coastalness = static_cast<float>(coastalSamples) / static_cast<float>(sampleCount);
    float mountainness = static_cast<float>(mountainSamples) / static_cast<float>(sampleCount);
    float steepness = static_cast<float>(steepSamples) / static_cast<float>(sampleCount);
    float flatness = 1.0f - saturate((static_cast<float>(slopeRange) - 1.0f) / 7.0f);
    float ruggedness = saturate((static_cast<float>(slopeRange) - 2.0f) / 8.0f);
    float lowland = smooth01((static_cast<float>(kStageSeaLevel + 7 - avgSurfaceY)) / 16.0f);
    float highland = smooth01((static_cast<float>(avgSurfaceY - (kStageSeaLevel + 12))) / 24.0f);

    float ruinScore = 0.10f +
                      coastalness * 0.56f +
                      lowland * 0.18f +
                      flatness * 0.18f +
                      (centerBiome == static_cast<uint8_t>(DebugBiomeId::kBeach) ? 0.22f : 0.0f) +
                      (centerBiome == static_cast<uint8_t>(DebugBiomeId::kDesert) ? 0.10f : 0.0f) -
                      ruggedness * 0.30f;

    float sanctumShape = 1.0f - std::abs(flatness - 0.58f) * 1.55f;
    sanctumShape = saturate(sanctumShape);
    float sanctumScore = 0.08f +
                         highland * 0.24f +
                         centerRouter.peakMask * 0.34f +
                         centerRouter.plateauMask * 0.22f +
                         sanctumShape * 0.14f +
                         mountainness * 0.16f -
                         coastalness * 0.40f;

    float towerScore = 0.10f +
                       flatness * 0.42f +
                       centerRouter.plateauMask * 0.18f +
                       (centerBiome == static_cast<uint8_t>(DebugBiomeId::kPlains) ? 0.14f : 0.0f) +
                       (centerBiome == static_cast<uint8_t>(DebugBiomeId::kForest) ? 0.10f : 0.0f) +
                       (centerBiome == static_cast<uint8_t>(DebugBiomeId::kDesert) ? 0.16f : 0.0f) +
                       highland * 0.08f -
                       coastalness * 0.18f -
                       steepness * 0.32f;

    StructureType type = StructureType::kRuinRing;
    float bestScore = ruinScore;
    int extent = 5;
    if (sanctumScore > bestScore) {
      type = StructureType::kPillarSanctum;
      bestScore = sanctumScore;
      extent = 6;
    }
    if (towerScore > bestScore) {
      type = StructureType::kWatchtowerRemnant;
      bestScore = towerScore;
      extent = 5;
    }

    bool tooSteep = (type == StructureType::kWatchtowerRemnant && slopeRange > 5) ||
                    (type == StructureType::kRuinRing && slopeRange > 6) ||
                    (type == StructureType::kPillarSanctum && slopeRange > 8);
    if (tooSteep || bestScore < 0.34f) {
      return false;
    }

    int foundationY = avgSurfaceY;
    if (type == StructureType::kPillarSanctum) {
      foundationY += 1;
    }
    foundationY = std::clamp(foundationY, minY + 2, maxY - 12);

    outCandidate.centerX = centerX;
    outCandidate.centerZ = centerZ;
    outCandidate.foundationY = foundationY;
    outCandidate.avgSurfaceY = avgSurfaceY;
    outCandidate.minSurfaceY = minSurface;
    outCandidate.maxSurfaceY = maxSurface;
    outCandidate.extent = extent;
    outCandidate.score = bestScore;
    outCandidate.biome = centerBiome;
    outCandidate.climate = centerClimate;
    outCandidate.router = centerRouter;
    outCandidate.type = type;
    return true;
  };

  auto footprintIntersectsChunk = [&](const StructureCandidate& candidate) {
    return !(candidate.centerX + candidate.extent < baseX ||
             candidate.centerX - candidate.extent > baseX + kChunkSize - 1 ||
             candidate.centerZ + candidate.extent < baseZ ||
             candidate.centerZ - candidate.extent > baseZ + kChunkSize - 1);
  };

  int regionX = floorDiv(cx, kStructureRegionSizeChunks);
  int regionZ = floorDiv(cz, kStructureRegionSizeChunks);
  for (int rz = regionZ - 1; rz <= regionZ + 1; ++rz) {
    for (int rx = regionX - 1; rx <= regionX + 1; ++rx) {
      uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, rx, rz, 0x5EEDu));
      float regionChance = 0.14f +
                           hashedNoise01(rx * 37, 0, rz * 37, seed, 0x5EEDu) * 0.14f;
      if (rand01(state) > regionChance) {
        continue;
      }

      StructureCandidate bestCandidate{};
      bool foundCandidate = false;
      for (int attempt = 0; attempt < 7; ++attempt) {
        int startChunkX =
          rx * kStructureRegionSizeChunks + randIntInclusive(state, 0, kStructureRegionSizeChunks - 1);
        int startChunkZ =
          rz * kStructureRegionSizeChunks + randIntInclusive(state, 0, kStructureRegionSizeChunks - 1);
        int centerX = startChunkX * kChunkSize + randIntInclusive(state, 4, kChunkSize - 5);
        int centerZ = startChunkZ * kChunkSize + randIntInclusive(state, 4, kChunkSize - 5);

        StructureCandidate candidate{};
        if (!chooseStructureCandidate(centerX, centerZ, candidate)) {
          continue;
        }
        if (!foundCandidate || candidate.score > bestCandidate.score) {
          bestCandidate = candidate;
          foundCandidate = true;
        }
      }

      if (!foundCandidate || !footprintIntersectsChunk(bestCandidate)) {
        continue;
      }

      int centerX = bestCandidate.centerX;
      int centerZ = bestCandidate.centerZ;
      int foundationY = bestCandidate.foundationY;
      StructureType type = bestCandidate.type;
      ClimateAndBiomeSample sample{};
      sample.biome = bestCandidate.biome;
      sample.climate = bestCandidate.climate;

      auto sampledSurfaceY = [&](int wx, int wz) {
        return estimateSurfaceAt(wx, wz, nullptr, nullptr, nullptr);
      };

      auto placeFoundation = [&](int radius) {
        for (int oz = -radius; oz <= radius; ++oz) {
          for (int ox = -radius; ox <= radius; ++ox) {
            float dist = std::sqrt(static_cast<float>(ox * ox + oz * oz));
            if (dist > static_cast<float>(radius) + 0.40f) {
              continue;
            }
            int wx = centerX + ox;
            int wz = centerZ + oz;
            uint8_t floorType = pickMasonry(state);
            if (dist > static_cast<float>(radius) - 0.35f && rand01(state) < 0.24f) {
              floorType = kGravel;
            }
            int surfaceY = sampledSurfaceY(wx, wz);
            int fillStart = std::clamp(std::min(surfaceY - 1, foundationY - 2), minY, foundationY);
            for (int y = fillStart; y <= foundationY; ++y) {
              setWorldIfInChunk(wx, y, wz, y == foundationY ? floorType : pickMasonry(state));
            }
          }
        }
      };

      auto placePillar = [&](int ox, int oz, int minH, int maxH, uint8_t material) {
        int h = randIntInclusive(state, minH, maxH);
        for (int y = 1; y <= h; ++y) {
          setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, material);
        }
        return h;
      };

      auto placeLootCache = [&](int ox, int oy, int oz) {
        setWorldIfInChunk(centerX + ox, foundationY + oy, centerZ + oz, kLootCache);
      };

      int foundationRadius = std::clamp(bestCandidate.extent - 1, 4, 5);
      placeFoundation(foundationRadius);
      for (int dz = -4; dz <= 4; ++dz) {
        for (int dx = -4; dx <= 4; ++dx) {
          if (std::abs(dx) != 4 && std::abs(dz) != 4) {
            continue;
          }
          if (rand01(state) < 0.14f) {
            continue;
          }
          setWorldIfInChunk(centerX + dx, foundationY + 1, centerZ + dz, pickMasonry(state));
        }
      }

      if (type == StructureType::kRuinRing) {
        int ruinVariant = randIntInclusive(state, 0, 2);
        if (sample.biome == static_cast<uint8_t>(DebugBiomeId::kBeach) && ruinVariant == 2) {
          ruinVariant = 1;
        }

        if (ruinVariant == 0) {
          int doorwaySide = randIntInclusive(state, 0, 3);
          auto isDoor = [&](int ox, int oz) {
            if (doorwaySide == 0) {
              return oz == -3 && std::abs(ox) <= 1;
            }
            if (doorwaySide == 1) {
              return oz == 3 && std::abs(ox) <= 1;
            }
            if (doorwaySide == 2) {
              return ox == -3 && std::abs(oz) <= 1;
            }
            return ox == 3 && std::abs(oz) <= 1;
          };
          for (int oz = -3; oz <= 3; ++oz) {
            for (int ox = -3; ox <= 3; ++ox) {
              if ((std::abs(ox) != 3 && std::abs(oz) != 3) || isDoor(ox, oz)) {
                continue;
              }
              int height = randIntInclusive(state, 1, 3);
              for (int y = 1; y <= height; ++y) {
                if (rand01(state) < 0.14f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, pickMasonry(state));
              }
            }
          }
          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              setWorldIfInChunk(centerX + ox, foundationY + 1, centerZ + oz, kStone);
            }
          }
        } else if (ruinVariant == 1) {
          bool openNorthSouth = rand01(state) < 0.5f;
          for (int oz = -3; oz <= 3; ++oz) {
            for (int ox = -3; ox <= 3; ++ox) {
              bool border = std::abs(ox) == 3 || std::abs(oz) == 3;
              if (!border) {
                continue;
              }
              bool opening = openNorthSouth
                ? (std::abs(ox) <= 1 && (oz == -3 || oz == 3))
                : (std::abs(oz) <= 1 && (ox == -3 || ox == 3));
              if (opening) {
                continue;
              }
              int height = randIntInclusive(state, 2, 4);
              for (int y = 1; y <= height; ++y) {
                if (y > 2 && rand01(state) < 0.24f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, pickMasonry(state));
              }
            }
          }
          for (const auto& corner : std::array<std::pair<int, int>, 4>{{{-3, -3}, {3, -3}, {-3, 3}, {3, 3}}}) {
            int h = randIntInclusive(state, 3, 5);
            for (int y = 1; y <= h; ++y) {
              setWorldIfInChunk(centerX + corner.first, foundationY + y, centerZ + corner.second, kStone);
            }
          }
          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              if (rand01(state) < 0.18f) {
                continue;
              }
              setWorldIfInChunk(centerX + ox, foundationY + 1, centerZ + oz, kStone);
            }
          }
        } else {
          for (int t = -4; t <= 4; ++t) {
            if (t % 2 != 0) {
              continue;
            }
            if (rand01(state) < 0.10f) {
              continue;
            }
            int h = randIntInclusive(state, 2, 4);
            for (int y = 1; y <= h; ++y) {
              setWorldIfInChunk(centerX + t, foundationY + y, centerZ - 4, pickMasonry(state));
              setWorldIfInChunk(centerX + t, foundationY + y, centerZ + 4, pickMasonry(state));
              setWorldIfInChunk(centerX - 4, foundationY + y, centerZ + t, pickMasonry(state));
              setWorldIfInChunk(centerX + 4, foundationY + y, centerZ + t, pickMasonry(state));
            }
          }
          for (int t = -2; t <= 2; ++t) {
            if (rand01(state) < 0.28f) {
              continue;
            }
            setWorldIfInChunk(centerX + t, foundationY + 1, centerZ, kStone);
            setWorldIfInChunk(centerX, foundationY + 1, centerZ + t, kStone);
          }
          int altarOx = randIntInclusive(state, -1, 1);
          int altarOz = randIntInclusive(state, -1, 1);
          setWorldIfInChunk(centerX + altarOx, foundationY + 1, centerZ + altarOz, kStone);
          setWorldIfInChunk(centerX + altarOx, foundationY + 2, centerZ + altarOz,
                            rand01(state) < 0.30f ? kWater : kStone);
        }
        placeLootCache(0, 1, 0);
      } else if (type == StructureType::kPillarSanctum) {
        int sanctumVariant = randIntInclusive(state, 0, 2);
        if (sanctumVariant == 0) {
          int hNW = placePillar(-2, -2, 4, 7, kStone);
          int hNE = placePillar(2, -2, 4, 7, kStone);
          int hSW = placePillar(-2, 2, 4, 7, kStone);
          int hSE = placePillar(2, 2, 4, 7, kStone);
          int bridgeY = foundationY + std::max({hNW, hNE, hSW, hSE}) - 1;
          for (int t = -2; t <= 2; ++t) {
            if (rand01(state) < 0.22f) {
              continue;
            }
            setWorldIfInChunk(centerX + t, bridgeY, centerZ - 2, kStone);
            setWorldIfInChunk(centerX + t, bridgeY, centerZ + 2, kStone);
            setWorldIfInChunk(centerX - 2, bridgeY, centerZ + t, kStone);
            setWorldIfInChunk(centerX + 2, bridgeY, centerZ + t, kStone);
          }
          setWorldIfInChunk(centerX, foundationY + 1, centerZ, kStone);
          setWorldIfInChunk(centerX, foundationY + 2, centerZ, rand01(state) < 0.32f ? kWater : kStone);
          placeLootCache(0, 2, 0);
        } else if (sanctumVariant == 1) {
          std::array<std::pair<int, int>, 8> ring = {{
            {-3, -1}, {-3, 1}, {3, -1}, {3, 1},
            {-1, -3}, {1, -3}, {-1, 3}, {1, 3}
          }};
          int topY = foundationY + 3;
          for (const auto& [ox, oz] : ring) {
            int h = randIntInclusive(state, 3, 6);
            for (int y = 1; y <= h; ++y) {
              setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, kStone);
            }
            topY = std::max(topY, foundationY + h);
          }
          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              setWorldIfInChunk(centerX + ox, foundationY + 1, centerZ + oz, kStone);
              if (rand01(state) < 0.78f) {
                setWorldIfInChunk(centerX + ox, topY, centerZ + oz, kStone);
              }
            }
          }
          setWorldIfInChunk(centerX, foundationY + 2, centerZ, rand01(state) < 0.38f ? kWater : kStone);
          placeLootCache(0, 2, 0);
        } else {
          for (int oz = -3; oz <= 3; oz += 3) {
            for (int ox = -2; ox <= 2; ox += 2) {
              int h = randIntInclusive(state, 4, 6);
              for (int y = 1; y <= h; ++y) {
                if (y > 2 && rand01(state) < 0.12f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, kStone);
              }
            }
          }
          int roofY = foundationY + randIntInclusive(state, 5, 7);
          for (int oz = -3; oz <= 3; ++oz) {
            for (int ox = -2; ox <= 2; ++ox) {
              if (std::abs(ox) != 2 && std::abs(oz) != 3) {
                continue;
              }
              if (rand01(state) < 0.32f) {
                continue;
              }
              setWorldIfInChunk(centerX + ox, roofY, centerZ + oz, kStone);
            }
          }
          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              setWorldIfInChunk(centerX + ox, foundationY + 1, centerZ + oz, kStone);
            }
          }
          placeLootCache(0, 2, 0);
        }
      } else {
        uint8_t support = (sample.biome == static_cast<uint8_t>(DebugBiomeId::kDesert))
          ? kStone
          : kWood;
        int towerVariant = randIntInclusive(state, 0, 2);
        if (towerVariant == 0) {
          int towerH = randIntInclusive(state, 5, 8);
          std::array<std::pair<int, int>, 4> corners = {{
            {-2, -2}, {2, -2}, {-2, 2}, {2, 2}
          }};
          for (const auto& [ox, oz] : corners) {
            for (int y = 1; y <= towerH; ++y) {
              if (rand01(state) < 0.08f && y > 2) {
                continue;
              }
              setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, support);
            }
          }
          for (int level = 2; level <= towerH; level += 2) {
            for (int oz = -1; oz <= 1; ++oz) {
              for (int ox = -1; ox <= 1; ++ox) {
                if (rand01(state) < 0.18f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + level, centerZ + oz,
                                  (rand01(state) < 0.30f) ? kStone : support);
              }
            }
          }
          if (towerH > 4) {
            for (int oz = -2; oz <= 2; ++oz) {
              for (int ox = -2; ox <= 2; ++ox) {
                if (std::abs(ox) != 2 && std::abs(oz) != 2) {
                  continue;
                }
                if (rand01(state) < 0.22f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + towerH, centerZ + oz, kLeaves);
              }
            }
          }
          placeLootCache(0, 1, 0);
        } else if (towerVariant == 1) {
          int leftH = randIntInclusive(state, 4, 7);
          int rightH = randIntInclusive(state, 4, 7);
          for (int y = 1; y <= leftH; ++y) {
            setWorldIfInChunk(centerX - 2, foundationY + y, centerZ, support);
            setWorldIfInChunk(centerX - 1, foundationY + y, centerZ - 1, support);
            setWorldIfInChunk(centerX - 1, foundationY + y, centerZ + 1, support);
          }
          for (int y = 1; y <= rightH; ++y) {
            setWorldIfInChunk(centerX + 2, foundationY + y, centerZ, support);
            setWorldIfInChunk(centerX + 1, foundationY + y, centerZ - 1, support);
            setWorldIfInChunk(centerX + 1, foundationY + y, centerZ + 1, support);
          }
          int bridgeY = foundationY + std::min(leftH, rightH) - 1;
          for (int x = -1; x <= 1; ++x) {
            if (rand01(state) < 0.16f) {
              continue;
            }
            setWorldIfInChunk(centerX + x, bridgeY, centerZ, kStone);
          }
          for (int x = -2; x <= 2; ++x) {
            setWorldIfInChunk(centerX + x, foundationY + 1, centerZ, kStone);
          }
          placeLootCache(0, 1, 0);
        } else {
          int wallH = randIntInclusive(state, 3, 5);
          int doorSide = randIntInclusive(state, 0, 3);
          for (int oz = -2; oz <= 2; ++oz) {
            for (int ox = -2; ox <= 2; ++ox) {
              bool edge = std::abs(ox) == 2 || std::abs(oz) == 2;
              if (!edge) {
                continue;
              }
              bool doorway = (doorSide == 0 && oz == -2 && std::abs(ox) <= 0) ||
                             (doorSide == 1 && oz == 2 && std::abs(ox) <= 0) ||
                             (doorSide == 2 && ox == -2 && std::abs(oz) <= 0) ||
                             (doorSide == 3 && ox == 2 && std::abs(oz) <= 0);
              for (int y = 1; y <= wallH; ++y) {
                if (doorway && y <= 2) {
                  continue;
                }
                if (y == wallH && rand01(state) < 0.26f) {
                  continue;
                }
                setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz,
                                  rand01(state) < 0.24f ? kGravel : support);
              }
            }
          }
          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              if (rand01(state) < 0.20f) {
                continue;
              }
              setWorldIfInChunk(centerX + ox, foundationY + 1, centerZ + oz, kStone);
            }
          }
          placeLootCache(0, 1, 0);
        }
      }
    }
  }
}

void runFeatureStage(int cx,
                     int cz,
                     int seed,
                     const WorldGenSettings& settings,
                     const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                     const std::array<BiomeClimateSample, kChunkColumnCount>& climateMap,
                     std::vector<uint8_t>& ioBlocks) {
  placeOreFeatures(cx, cz, seed, settings, biomeMap, climateMap, ioBlocks);
  placeRockOutcropFeatures(cx, cz, seed, settings, biomeMap, climateMap, ioBlocks);
  placeLakeFeatures(cx, cz, seed, settings, biomeMap, climateMap, ioBlocks);
  if (settings.generateStructures) {
    placeRegionalStructureFeatures(cx, cz, seed, settings, ioBlocks);
  }
  placeCrashIslandFeatures(cx, cz, seed, settings, ioBlocks);
  placeTreeFeatures(cx, cz, seed, settings, biomeMap, climateMap, ioBlocks);
  placeUnderwaterPlantFeatures(cx, cz, seed, settings, biomeMap, ioBlocks);
  placeShorelineDriftFeatures(cx, cz, seed, settings, biomeMap, ioBlocks);
  applyHeightRangeMask(ioBlocks, settings.minY, settings.maxY);
}

struct GeneratedChunkData {
  std::vector<uint8_t> blocks;
  std::array<uint8_t, kChunkColumnCount> biomeMap{};
  std::array<BiomeClimateSample, kChunkColumnCount> climateMap{};
  ChunkGenStatus status = ChunkGenStatus::kEmpty;
};

GeneratedChunkData generateChunkDataToStatus(int cx,
                                             int cz,
                                             int seed,
                                             const WorldGenSettings& settings,
                                             ChunkGenStatus targetStatus) {
  GeneratedChunkData out;
  out.blocks.resize(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  out.biomeMap.fill(static_cast<uint8_t>(DebugBiomeId::kPlains));
  out.climateMap.fill(BiomeClimateSample{});

  auto clampStatus = [](ChunkGenStatus status) {
    if (status < ChunkGenStatus::kEmpty) {
      return ChunkGenStatus::kEmpty;
    }
    if (status > ChunkGenStatus::kFull) {
      return ChunkGenStatus::kFull;
    }
    return status;
  };
  targetStatus = clampStatus(targetStatus);
  out.status = ChunkGenStatus::kEmpty;

  if (targetStatus >= ChunkGenStatus::kStructureStarts) {
    out.status = ChunkGenStatus::kStructureStarts;
  }
  if (targetStatus >= ChunkGenStatus::kStructureReferences) {
    out.status = ChunkGenStatus::kStructureReferences;
  }
  if (targetStatus < ChunkGenStatus::kBiomes) {
    return out;
  }

  computeBiomeClimateMaps(cx, cz, seed, settings, out.biomeMap, out.climateMap);
  out.status = ChunkGenStatus::kBiomes;

  if (targetStatus < ChunkGenStatus::kNoise) {
    return out;
  }
  runNoiseStage(cx, cz, seed, settings, out.biomeMap, out.climateMap, out.blocks);
  out.status = ChunkGenStatus::kNoise;

  if (targetStatus < ChunkGenStatus::kSurface) {
    return out;
  }
  runSurfaceStage(cx, cz, seed, settings, out.biomeMap, out.climateMap, out.blocks);
  out.status = ChunkGenStatus::kSurface;

  if (targetStatus < ChunkGenStatus::kCarvers) {
    return out;
  }
  runCarverStage(cx, cz, seed, settings, out.blocks, false);
  out.status = ChunkGenStatus::kCarvers;

  if (targetStatus < ChunkGenStatus::kLiquidCarvers) {
    return out;
  }
  runCarverStage(cx, cz, seed, settings, out.blocks, true);
  out.status = ChunkGenStatus::kLiquidCarvers;

  if (targetStatus < ChunkGenStatus::kFeatures) {
    return out;
  }
  runFeatureStage(cx, cz, seed, settings, out.biomeMap, out.climateMap, out.blocks);
  out.status = ChunkGenStatus::kFeatures;

  if (targetStatus >= ChunkGenStatus::kLight) {
    out.status = ChunkGenStatus::kLight;
  }
  if (targetStatus >= ChunkGenStatus::kSpawn) {
    out.status = ChunkGenStatus::kSpawn;
  }
  if (targetStatus >= ChunkGenStatus::kHeightmaps) {
    out.status = ChunkGenStatus::kHeightmaps;
  }
  if (targetStatus >= ChunkGenStatus::kFull) {
    out.status = ChunkGenStatus::kFull;
  }

  return out;
}

} // namespace

World::World(int initialChunksXIn, int initialChunksZIn, int seedIn)
    : initialChunksX(initialChunksXIn),
      initialChunksZ(initialChunksZIn),
      seed(seedIn) {
  int maxDim = std::max(initialChunksX, initialChunksZ);
  initialRadius = std::max(1, maxDim / 2);
  setGenerationSettings(genSettings);
  startMeshWorkers();
  startChunkWorkers();
}

World::~World() {
  stopMeshWorkers();
  stopChunkWorkers();
}

void World::setGenerationSettings(const WorldGenSettings& settings) {
  genSettings = settings;
  genSettings.caveDensity = std::clamp(genSettings.caveDensity, 0.25f, 2.5f);
  genSettings.ravineFrequency = std::clamp(genSettings.ravineFrequency, 0.25f, 2.5f);
  genSettings.startInventoryMode = genSettings.startInventoryMode <= 1 ? genSettings.startInventoryMode : 0;
  genSettings.minY = std::clamp(genSettings.minY, 0, kChunkHeight - 1);
  genSettings.maxY = std::clamp(genSettings.maxY, genSettings.minY, kChunkHeight - 1);
}

World::RuntimeStats World::collectRuntimeStats() const {
  RuntimeStats stats{};
  stats.loadedChunks = chunks.size();
  stats.pendingSectionRebuilds = pendingSectionRebuildQueue.size();
  stats.pendingMeshUploads = pendingMeshUploadQueue.size();
  stats.pendingRemovedMeshes = pendingRemovedMeshKeys.size();

  for (const auto& entry : chunks) {
    const Chunk& chunk = entry.second;
    if (chunk.generating) {
      ++stats.generatingChunks;
    }
    for (const SectionRenderData& section : chunk.sectionMeshes) {
      if (section.dirty) {
        ++stats.dirtySections;
      }
      if (section.queued) {
        ++stats.queuedSections;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    stats.sectionWorkerQueue = sectionRebuildQueue.size();
    stats.sectionWorkerResults = sectionRebuildResults.size();
  }

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    stats.generationQueue = generationQueue.size();
    stats.generationResults = generationResults.size();
  }

  return stats;
}

int World::generationTaskPriority(int cx, int cz, ChunkGenStatus targetStatus) const {
  int dist = std::max(std::abs(cx - generationFocusChunkX), std::abs(cz - generationFocusChunkZ));
  int clampedDist = std::min(dist, 255);
  int statusWeight = static_cast<int>(std::clamp(targetStatus, ChunkGenStatus::kNoise, ChunkGenStatus::kFull));
  return statusWeight * 512 - clampedDist;
}

void World::requestChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus) {
  if (targetStatus <= ChunkGenStatus::kEmpty) {
    return;
  }

  pumpChunkGeneration();
  (void)ensureChunk(chunkX, chunkZ, targetStatus);
}

void World::generateChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus) {
  if (targetStatus <= ChunkGenStatus::kEmpty) {
    return;
  }

  pumpChunkGeneration();
  Chunk& chunk = ensureChunk(chunkX, chunkZ, targetStatus);
  if (!chunk.generating && chunk.generatedStatus >= targetStatus) {
    return;
  }

  if (chunk.generating) {
    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::milliseconds(10);
    while (chunk.generating && clock::now() < deadline) {
      pumpChunkGeneration();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  if (chunk.generating || chunk.generatedStatus < targetStatus) {
    generateChunk(chunk, targetStatus);
    {
      std::lock_guard<std::mutex> lock(generationMutex);
      pendingGenerationEpochByKey.erase(chunkKey(chunkX, chunkZ));
    }
    markChunkSectionsDirty(chunk, true);
    markNeighborChunksDirty(chunk.cx, chunk.cz);
    meshDirty = true;
  }
}

ChunkGenStatus World::getChunkGenerationStatus(int chunkX, int chunkZ) const {
  const Chunk* chunk = findChunk(chunkX, chunkZ);
  if (!chunk) {
    return ChunkGenStatus::kEmpty;
  }
  return chunk->generatedStatus;
}

void World::startMeshWorkers() {
  size_t workerCount = 1;

  sectionRebuildWorkers.reserve(workerCount);
  for (size_t i = 0; i < workerCount; ++i) {
    sectionRebuildWorkers.emplace_back([this]() {
      while (true) {
        SectionRebuildTask task;
        {
          std::unique_lock<std::mutex> lock(sectionRebuildMutex);
          sectionRebuildCv.wait(lock, [this]() {
            return stopSectionRebuildWorkers || !sectionRebuildQueue.empty();
          });
          if (stopSectionRebuildWorkers && sectionRebuildQueue.empty()) {
            return;
          }
          task = std::move(sectionRebuildQueue.front());
          sectionRebuildQueue.pop();
        }

        SectionRebuildResult result;
        result.sectionKey = task.sectionKey;
        result.chunkLookupKey = task.chunkLookupKey;
        result.sectionY = task.sectionY;
        result.version = task.version;
        buildSectionMeshFromSamples(task.cx,
                                    task.cz,
                                    task.sectionY,
                                    task.samples,
                                    task.skyLightSamples,
                                    task.aprilMode,
                                    task.overlayActive,
                                    task.overlayBlock,
                                    task.overlayStage,
                                    result.vertices,
                                    result.indices);

        {
          std::lock_guard<std::mutex> lock(sectionRebuildMutex);
          sectionRebuildResults.push(std::move(result));
        }
      }
    });
  }
}

void World::stopMeshWorkers() {
  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    stopSectionRebuildWorkers = true;
    while (!sectionRebuildQueue.empty()) {
      sectionRebuildQueue.pop();
    }
    while (!sectionRebuildResults.empty()) {
      sectionRebuildResults.pop();
    }
  }
  sectionRebuildCv.notify_all();

  for (std::thread& worker : sectionRebuildWorkers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  sectionRebuildWorkers.clear();
}

void World::startChunkWorkers() {
  size_t cpuCount = static_cast<size_t>(std::thread::hardware_concurrency());
  if (cpuCount == 0) {
    cpuCount = 4;
  }

  size_t workerCount = 1;
  if (cpuCount >= 8) {
    workerCount = 4;
  } else if (cpuCount >= 6) {
    workerCount = 3;
  } else if (cpuCount >= 3) {
    workerCount = 2;
  }

  generationWorkers.reserve(workerCount);
  for (size_t i = 0; i < workerCount; ++i) {
    generationWorkers.emplace_back([this]() {
      while (true) {
        ChunkGenerationTask task;
        {
          std::unique_lock<std::mutex> lock(generationMutex);
          generationCv.wait(lock, [this]() {
            return stopGenerationWorkers || !generationQueue.empty();
          });

          if (stopGenerationWorkers && generationQueue.empty()) {
            return;
          }

          task = generationQueue.top();
          generationQueue.pop();

          auto pendingIt = pendingGenerationEpochByKey.find(task.key);
          if (pendingIt == pendingGenerationEpochByKey.end() || pendingIt->second != task.epoch) {
            continue;
          }
        }

        ChunkGenerationResult result;
        result.key = task.key;
        result.epoch = task.epoch;
        GeneratedChunkData generated = generateChunkDataToStatus(task.cx,
                                                                 task.cz,
                                                                 task.seed,
                                                                 task.settings,
                                                                 task.targetStatus);
        result.blocks = std::move(generated.blocks);
        result.biomeMap = generated.biomeMap;
        result.climateMap = generated.climateMap;
        result.status = generated.status;

        {
          std::lock_guard<std::mutex> lock(generationMutex);
          generationResults.push(std::move(result));
        }
      }
    });
  }
}

void World::stopChunkWorkers() {
  {
    std::lock_guard<std::mutex> lock(generationMutex);
    stopGenerationWorkers = true;
    while (!generationQueue.empty()) {
      generationQueue.pop();
    }
  }
  generationCv.notify_all();

  for (std::thread& worker : generationWorkers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  generationWorkers.clear();
}

int World::localIndex(int lx, int ly, int lz) const {
  return lx + lz * kChunkSize + ly * kChunkSize * kChunkSize;
}

uint64_t World::chunkKey(int cx, int cz) const {
  uint64_t ux = static_cast<uint32_t>(cx);
  uint64_t uz = static_cast<uint32_t>(cz);
  return (ux << 32) | uz;
}

uint64_t World::sectionKey(int cx, int cz, int sectionY) const {
  return packSectionKey(cx, cz, sectionY);
}

bool World::decodeSectionKey(uint64_t key, int& outCx, int& outCz, int& outSectionY) const {
  return unpackSectionKey(key, outCx, outCz, outSectionY);
}

bool World::isChunkMeshReady(const Chunk& chunk) const {
  return !chunk.generating && chunk.generatedStatus >= ChunkGenStatus::kNoise;
}

int World::sectionIndexForY(int y) const {
  if (y < 0) {
    return 0;
  }
  if (y >= kChunkHeight) {
    return kChunkSectionCount - 1;
  }
  return y / kChunkSectionSize;
}

World::Chunk* World::findChunk(int cx, int cz) {
  auto it = chunks.find(chunkKey(cx, cz));
  if (it == chunks.end()) {
    return nullptr;
  }
  return &it->second;
}

const World::Chunk* World::findChunk(int cx, int cz) const {
  auto it = chunks.find(chunkKey(cx, cz));
  if (it == chunks.end()) {
    return nullptr;
  }
  return &it->second;
}

void World::rebuildChunkTorchIndices(Chunk& chunk) {
  chunk.torchLocalIndices.clear();
  chunk.torchLocalIndices.reserve(8);
  for (int y = 0; y < kChunkHeight; ++y) {
    for (int z = 0; z < kChunkSize; ++z) {
      for (int x = 0; x < kChunkSize; ++x) {
        uint8_t type = chunk.blocks[static_cast<size_t>(localIndex(x, y, z))];
        if (!isTorchBlock(type)) {
          continue;
        }
        chunk.torchLocalIndices.push_back(static_cast<uint16_t>(localIndex(x, y, z)));
      }
    }
  }
}

void World::queueChunkGeneration(int cx, int cz, uint32_t epoch, ChunkGenStatus targetStatus) {
  uint64_t key = chunkKey(cx, cz);
  {
    std::lock_guard<std::mutex> lock(generationMutex);
    auto pendingIt = pendingGenerationEpochByKey.find(key);
    if (pendingIt != pendingGenerationEpochByKey.end() && pendingIt->second == epoch) {
      return;
    }
    pendingGenerationEpochByKey[key] = epoch;

    ChunkGenerationTask task;
    task.cx = cx;
    task.cz = cz;
    task.seed = seed;
    task.settings = genSettings;
    task.epoch = epoch;
    task.key = key;
    task.targetStatus = targetStatus;
    task.priority = generationTaskPriority(cx, cz, targetStatus);
    task.sequence = ++generationTaskSequence;
    generationQueue.push(std::move(task));
  }
  generationCv.notify_one();
}

void World::resetChunkGeneration() {
  ++generationEpoch;
  generationTaskSequence = 0;

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    pendingGenerationEpochByKey.clear();
    while (!generationQueue.empty()) {
      generationQueue.pop();
    }
    while (!generationResults.empty()) {
      generationResults.pop();
    }
  }
}

void World::resetMeshRebuildQueues() {
  pendingRemovedMeshKeys.clear();
  pendingSectionRebuildQueue.clear();
  pendingSectionRebuildSet.clear();
  pendingMeshUploadQueue.clear();
  pendingMeshUploadSet.clear();

  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    while (!sectionRebuildQueue.empty()) {
      sectionRebuildQueue.pop();
    }
    while (!sectionRebuildResults.empty()) {
      sectionRebuildResults.pop();
    }
  }
}

void World::markChunkSectionDirty(Chunk& chunk, int sectionY, bool queueRebuild) {
  if (sectionY < 0 || sectionY >= kChunkSectionCount) {
    return;
  }

  SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(sectionY)];
  section.dirty = true;
  ++section.version;
  if (section.version == 0) {
    section.version = 1;
  }
  chunk.dirty = true;
  meshDirty = true;

  if (queueRebuild && isChunkMeshReady(chunk)) {
    enqueueSectionRebuild(sectionKey(chunk.cx, chunk.cz, sectionY));
  }
}

void World::markChunkSectionsDirty(Chunk& chunk, bool queueRebuild) {
  for (int sectionY = 0; sectionY < kChunkSectionCount; ++sectionY) {
    markChunkSectionDirty(chunk, sectionY, queueRebuild);
  }
}

void World::markSectionAndNeighborsDirty(int cx,
                                         int cz,
                                         int lx,
                                         int y,
                                         int lz,
                                         bool queueRebuild) {
  int sectionY = sectionIndexForY(y);

  auto mark = [&](int tcx, int tcz, int tsy) {
    Chunk* target = findChunk(tcx, tcz);
    if (!target) {
      return;
    }
    markChunkSectionDirty(*target, tsy, queueRebuild);
  };

  mark(cx, cz, sectionY);
  if ((y % kChunkSectionSize) == 0) {
    mark(cx, cz, sectionY - 1);
  }
  if ((y % kChunkSectionSize) == (kChunkSectionSize - 1)) {
    mark(cx, cz, sectionY + 1);
  }
  if (lx == 0) {
    mark(cx - 1, cz, sectionY);
  }
  if (lx == kChunkSize - 1) {
    mark(cx + 1, cz, sectionY);
  }
  if (lz == 0) {
    mark(cx, cz - 1, sectionY);
  }
  if (lz == kChunkSize - 1) {
    mark(cx, cz + 1, sectionY);
  }
}

void World::markNeighborChunksDirty(int cx, int cz) {
  constexpr std::array<std::pair<int, int>, 4> offsets = {{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1}
  }};

  for (const auto& [ox, oz] : offsets) {
    Chunk* neighbor = findChunk(cx + ox, cz + oz);
    if (neighbor) {
      markChunkSectionsDirty(*neighbor, true);
    }
  }
}

void World::markChunkBoundaryNeighborsDirty(int cx, int cz) {
  markNeighborChunksDirty(cx, cz);
}

void World::enqueueSectionRebuild(uint64_t key) {
  if (pendingSectionRebuildSet.find(key) != pendingSectionRebuildSet.end()) {
    return;
  }
  pendingSectionRebuildSet.insert(key);
  pendingSectionRebuildQueue.push_back(key);
}

bool World::queueSectionRebuildTask(Chunk& chunk, int sectionY) {
  if (!isChunkMeshReady(chunk)) {
    return false;
  }
  if (sectionY < 0 || sectionY >= kChunkSectionCount) {
    return false;
  }

  SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(sectionY)];
  if (!section.dirty || section.queued) {
    return false;
  }

  SectionRebuildTask task;
  task.sectionKey = sectionKey(chunk.cx, chunk.cz, sectionY);
  task.chunkLookupKey = chunkKey(chunk.cx, chunk.cz);
  task.cx = chunk.cx;
  task.cz = chunk.cz;
  task.sectionY = sectionY;
  task.version = section.version;
  task.aprilMode = isAprilFoolsPreset(genSettings);
  task.overlayActive = breakOverlay.active && breakOverlay.stage > 0;
  task.overlayBlock = breakOverlay.block;
  task.overlayStage = breakOverlay.stage;

  int baseX = chunk.cx * kChunkSize;
  int baseY = sectionY * kChunkSectionSize;
  int baseZ = chunk.cz * kChunkSize;
  Chunk* neighborGrid[3][3] = {{nullptr, nullptr, nullptr},
                               {nullptr, nullptr, nullptr},
                               {nullptr, nullptr, nullptr}};
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dx = -1; dx <= 1; ++dx) {
      Chunk* neighbor = findChunk(chunk.cx + dx, chunk.cz + dz);
      if (neighbor && isChunkMeshReady(*neighbor)) {
        neighborGrid[dz + 1][dx + 1] = neighbor;
      }
    }
  }

  auto sampleBlockFast = [&](int wx, int wy, int wz) -> uint8_t {
    if (wy < genSettings.minY || wy > genSettings.maxY) {
      return kAir;
    }
    if (wy < 0 || wy >= kChunkHeight) {
      return kAir;
    }

    int lx = wx - baseX;
    int lz = wz - baseZ;
    int chunkOffsetX = 1;
    int chunkOffsetZ = 1;
    if (lx < 0) {
      lx += kChunkSize;
      chunkOffsetX = 0;
    } else if (lx >= kChunkSize) {
      lx -= kChunkSize;
      chunkOffsetX = 2;
    }
    if (lz < 0) {
      lz += kChunkSize;
      chunkOffsetZ = 0;
    } else if (lz >= kChunkSize) {
      lz -= kChunkSize;
      chunkOffsetZ = 2;
    }

    Chunk* source = neighborGrid[chunkOffsetZ][chunkOffsetX];
    if (!source) {
      return (chunkOffsetX != 1 || chunkOffsetZ != 1) ? kStone : kAir;
    }
    return source->blocks[static_cast<size_t>(localIndex(lx, wy, lz))];
  };

  for (int sz = 0; sz < kSectionSampleSize; ++sz) {
    for (int sx = 0; sx < kSectionSampleSize; ++sx) {
      for (int sy = 0; sy < kSectionSampleSize; ++sy) {
        int wx = baseX + (sx - 1);
        int wy = baseY + (sy - 1);
        int wz = baseZ + (sz - 1);
        task.samples[static_cast<size_t>(sectionSampleIndex(sx, sy, sz))] =
          sampleBlockFast(wx, wy, wz);
      }
    }
  }

  computeSectionSkyLightSamples(
    baseX,
    baseY,
    baseZ,
    [&](int wx, int wy, int wz) {
      return sampleBlockFast(wx, wy, wz);
    },
    [this](int wx, int wz) {
      return sampleSurfaceHeightAt(wx, wz);
    },
    task.skyLightSamples);

  section.queued = true;
  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    sectionRebuildQueue.push(std::move(task));
  }
  sectionRebuildCv.notify_one();
  return true;
}

void World::enqueueChunkMeshUpload(uint64_t key) {
  if (pendingMeshUploadSet.find(key) != pendingMeshUploadSet.end()) {
    return;
  }
  pendingMeshUploadSet.insert(key);
  pendingMeshUploadQueue.push_back(key);
}

void World::pumpMeshRebuildResults() {
  std::vector<SectionRebuildResult> ready;
  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    while (!sectionRebuildResults.empty()) {
      ready.push_back(std::move(sectionRebuildResults.front()));
      sectionRebuildResults.pop();
    }
  }

  if (ready.empty()) {
    return;
  }

  for (SectionRebuildResult& result : ready) {
    auto chunkIt = chunks.find(result.chunkLookupKey);
    if (chunkIt == chunks.end()) {
      continue;
    }

    Chunk& chunk = chunkIt->second;
    if (result.sectionY < 0 || result.sectionY >= kChunkSectionCount) {
      continue;
    }

    SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(result.sectionY)];
    section.queued = false;
    if (!isChunkMeshReady(chunk)) {
      continue;
    }

    if (!section.dirty || section.version != result.version) {
      if (section.dirty) {
        enqueueSectionRebuild(result.sectionKey);
      }
      continue;
    }

    section.vertices = std::move(result.vertices);
    section.indices = std::move(result.indices);
    section.dirty = false;
    enqueueChunkMeshUpload(result.sectionKey);

    chunk.dirty = false;
    for (const SectionRenderData& s : chunk.sectionMeshes) {
      if (s.dirty || s.queued) {
        chunk.dirty = true;
        break;
      }
    }
  }
}

void World::pumpChunkGeneration() {
  std::vector<ChunkGenerationResult> ready;

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    while (!generationResults.empty()) {
      ready.push_back(std::move(generationResults.front()));
      generationResults.pop();
    }
  }

  if (ready.empty()) {
    return;
  }

  for (ChunkGenerationResult& result : ready) {
    auto pendingIt = pendingGenerationEpochByKey.find(result.key);
    if (pendingIt != pendingGenerationEpochByKey.end() && pendingIt->second == result.epoch) {
      pendingGenerationEpochByKey.erase(pendingIt);
    }

    auto chunkIt = chunks.find(result.key);
    if (chunkIt == chunks.end()) {
      continue;
    }

    Chunk& chunk = chunkIt->second;
    if (!chunk.generating || chunk.generationEpoch != result.epoch) {
      continue;
    }

    chunk.blocks = std::move(result.blocks);
    rebuildChunkTorchIndices(chunk);
    chunk.biomeMap = result.biomeMap;
    chunk.climateMap = result.climateMap;
    chunk.generatedStatus = result.status;
    chunk.generationTargetStatus = result.status;
    chunk.generating = false;
    markChunkSectionsDirty(chunk, true);
    markNeighborChunksDirty(chunk.cx, chunk.cz);
    meshDirty = true;
  }
}

World::Chunk& World::ensureChunk(int cx, int cz, ChunkGenStatus targetStatus) {
  uint64_t key = chunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    Chunk& existing = it->second;
    ChunkGenStatus clampedTarget =
      std::clamp(targetStatus, ChunkGenStatus::kNoise, ChunkGenStatus::kFull);
    if (!existing.generating && existing.generatedStatus < clampedTarget) {
      existing.generating = true;
      existing.generationEpoch = generationEpoch++;
      existing.generationTargetStatus = clampedTarget;
      queueChunkGeneration(cx, cz, existing.generationEpoch, clampedTarget);
    } else if (existing.generating && existing.generationTargetStatus < clampedTarget) {
      existing.generationEpoch = generationEpoch++;
      existing.generationTargetStatus = clampedTarget;
      queueChunkGeneration(cx, cz, existing.generationEpoch, clampedTarget);
    }
    return it->second;
  }

  Chunk chunk;
  chunk.cx = cx;
  chunk.cz = cz;
  chunk.blocks.resize(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  chunk.biomeMap.fill(2);
  chunk.climateMap.fill(BiomeClimateSample{});
  chunk.dirty = true;

  auto savedIt = savedChunks.find(key);
  if (savedIt != savedChunks.end()) {
    chunk.blocks = savedIt->second;
    computeBiomeClimateMaps(cx, cz, seed, genSettings, chunk.biomeMap, chunk.climateMap);
    rebuildChunkTorchIndices(chunk);
    chunk.modified = true;
    chunk.generatedStatus = ChunkGenStatus::kFull;
    chunk.generationTargetStatus = ChunkGenStatus::kFull;
    savedChunks.erase(savedIt);
  } else {
    chunk.generating = true;
    chunk.generationEpoch = generationEpoch;
    chunk.generationTargetStatus = std::clamp(targetStatus, ChunkGenStatus::kNoise, ChunkGenStatus::kFull);
  }

  auto insertResult = chunks.emplace(key, std::move(chunk));
  if (insertResult.first->second.generating) {
    queueChunkGeneration(cx,
                         cz,
                         insertResult.first->second.generationEpoch,
                         insertResult.first->second.generationTargetStatus);
  } else {
    markChunkSectionsDirty(insertResult.first->second, true);
    markNeighborChunksDirty(cx, cz);
  }
  meshDirty = true;
  return insertResult.first->second;
}

void World::generateChunk(World::Chunk& chunk, ChunkGenStatus targetStatus) {
  GeneratedChunkData generated =
    generateChunkDataToStatus(chunk.cx, chunk.cz, seed, genSettings, targetStatus);
  chunk.blocks = std::move(generated.blocks);
  rebuildChunkTorchIndices(chunk);
  chunk.biomeMap = generated.biomeMap;
  chunk.climateMap = generated.climateMap;
  chunk.generatedStatus = generated.status;
  chunk.generationTargetStatus = generated.status;
  chunk.generating = false;
  chunk.generationEpoch = generationEpoch;
}

bool World::inBounds(int x, int y, int z) const {
  (void)x;
  (void)z;
  return y >= genSettings.minY && y <= genSettings.maxY;
}

uint8_t World::getBlock(int x, int y, int z) const {
  if (!inBounds(x, y, z)) {
    return kAir;
  }
  int cx = floorDiv(x, kChunkSize);
  int cz = floorDiv(z, kChunkSize);
  int lx = positiveMod(x, kChunkSize);
  int lz = positiveMod(z, kChunkSize);
  const Chunk* chunk = findChunk(cx, cz);
  if (!chunk) {
    return kAir;
  }
  if (chunk->generatedStatus < ChunkGenStatus::kNoise) {
    return kAir;
  }
  return chunk->blocks[static_cast<size_t>(localIndex(lx, y, lz))];
}

bool World::sampleBiomeClimateAt(int x, int z, uint8_t& outBiome, BiomeClimateSample& outClimate) const {
  int cx = floorDiv(x, kChunkSize);
  int cz = floorDiv(z, kChunkSize);
  int lx = positiveMod(x, kChunkSize);
  int lz = positiveMod(z, kChunkSize);
  const Chunk* chunk = findChunk(cx, cz);
  if (chunk &&
      !chunk->generating &&
      chunk->generatedStatus >= ChunkGenStatus::kBiomes) {
    size_t idx = static_cast<size_t>(lx + lz * kChunkSize);
    outBiome = chunk->biomeMap[idx];
    outClimate = chunk->climateMap[idx];
    return true;
  }

  ClimateAndBiomeSample sampled = sampleClimateAndBiomeAt(x, z, seed, genSettings);
  outBiome = sampled.biome;
  outClimate = sampled.climate;
  return false;
}

int World::sampleSurfaceHeightAt(int x, int z) const {
  int minY = genSettings.minY;
  int maxY = genSettings.maxY;
  for (int y = maxY; y >= minY; --y) {
    uint8_t t = getBlock(x, y, z);
    if (t != kAir && !isWaterBlock(t) && !isDecorationBlock(t)) {
      return y;
    }
  }

  uint8_t biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
  BiomeClimateSample climate{};
  sampleBiomeClimateAt(x, z, biome, climate);
  float target = computeRouterTargetHeight(x, z, seed, biome, climate, minY, maxY);
  return std::clamp(static_cast<int>(std::round(target)), minY, maxY);
}

float World::sampleDensityAt(int x, int y, int z) const {
  int minY = genSettings.minY;
  int maxY = genSettings.maxY;
  int sampleY = std::clamp(y, minY, maxY);
  uint8_t biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
  BiomeClimateSample climate{};
  sampleBiomeClimateAt(x, z, biome, climate);
  TerrainRouterSample router = sampleTerrainRouterAt(x, z, seed, biome, climate, minY, maxY);
  return sampleDensityRouterAt(x,
                               sampleY,
                               z,
                               seed,
                               genSettings,
                               biome,
                               climate,
                               router,
                               minY);
}

int World::sampleAquiferLevelAt(int x, int z) const {
  uint8_t biome = static_cast<uint8_t>(DebugBiomeId::kPlains);
  BiomeClimateSample climate{};
  sampleBiomeClimateAt(x, z, biome, climate);
  return computeAquiferLevelAt(x, z, seed, climate, genSettings.minY, genSettings.maxY);
}

std::vector<glm::ivec3> World::collectTorchBlocks() const {
  std::vector<glm::ivec3> torches;
  for (const auto& entry : chunks) {
    const Chunk& chunk = entry.second;
    if (chunk.generatedStatus < ChunkGenStatus::kNoise) {
      continue;
    }
    torches.reserve(torches.size() + chunk.torchLocalIndices.size());
    for (uint16_t local : chunk.torchLocalIndices) {
      int y = local / static_cast<uint16_t>(kChunkSize * kChunkSize);
      int rem = local - y * kChunkSize * kChunkSize;
      int z = rem / kChunkSize;
      int x = rem % kChunkSize;
      torches.push_back({chunk.cx * kChunkSize + x, y, chunk.cz * kChunkSize + z});
    }
  }
  return torches;
}

void World::setBlock(int x, int y, int z, uint8_t type) {
  pumpChunkGeneration();

  if (!inBounds(x, y, z)) {
    return;
  }
  int cx = floorDiv(x, kChunkSize);
  int cz = floorDiv(z, kChunkSize);
  int lx = positiveMod(x, kChunkSize);
  int lz = positiveMod(z, kChunkSize);

  Chunk* chunk = findChunk(cx, cz);
  if (!chunk) {
    if (type == kAir) {
      return;
    }
    chunk = &ensureChunk(cx, cz);
  }

  if (chunk->generating) {
    generateChunk(*chunk, ChunkGenStatus::kFull);
    {
      std::lock_guard<std::mutex> lock(generationMutex);
      pendingGenerationEpochByKey.erase(chunkKey(cx, cz));
    }
    markChunkSectionsDirty(*chunk, true);
    markNeighborChunksDirty(cx, cz);
  }

  size_t idx = static_cast<size_t>(localIndex(lx, y, lz));
  if (chunk->blocks[idx] == type) {
    return;
  }
  chunk->blocks[idx] = type;
  rebuildChunkTorchIndices(*chunk);

  auto pruneUnsupportedTorch = [&](int tx, int ty, int tz) {
    uint8_t torchType = getBlock(tx, ty, tz);
    if (!isTorchBlock(torchType)) {
      return;
    }
    glm::ivec3 supportOffset = torchSupportOffset(torchType);
    uint8_t support = getBlock(tx + supportOffset.x, ty + supportOffset.y, tz + supportOffset.z);
    if (support == kAir || isWaterBlock(support) || isDecorationBlock(support)) {
      setBlock(tx, ty, tz, kAir);
    }
  };

  pruneUnsupportedTorch(x, y, z);
  pruneUnsupportedTorch(x, y + 1, z);
  pruneUnsupportedTorch(x + 1, y, z);
  pruneUnsupportedTorch(x - 1, y, z);
  pruneUnsupportedTorch(x, y, z + 1);
  pruneUnsupportedTorch(x, y, z - 1);

  markSectionAndNeighborsDirty(cx, cz, lx, y, lz, true);
  chunk->modified = true;
  meshDirty = true;
}

glm::vec3 World::blockColor(uint8_t type) const {
  return blockColorForMesh(type, isAprilFoolsPreset(genSettings));
}

void World::generate() {
  resetChunkGeneration();
  resetMeshRebuildQueues();
  chunks.clear();
  savedChunks.clear();
  breakOverlay.active = false;
  breakOverlay.stage = 0;
  meshDirty = true;
  updateActiveChunks(0, 0, initialRadius);
}

void World::updateActiveChunks(int centerChunkX, int centerChunkZ, int radius) {
  pumpChunkGeneration();

  if (radius < 0) {
    return;
  }

  generationFocusChunkX = centerChunkX;
  generationFocusChunkZ = centerChunkZ;

  // Keep a small hysteresis ring to avoid visible world cutoffs while new chunks
  // are still generating near the view boundary.
  const int loadRadius = radius + 1;
  const int unloadRadius = radius + 2;
  const int fullGenRadius = std::max(3, radius / 2);
  const int featureGenRadius = std::max(fullGenRadius + 1, radius - 1);

  for (int cz = centerChunkZ - loadRadius; cz <= centerChunkZ + loadRadius; ++cz) {
    for (int cx = centerChunkX - loadRadius; cx <= centerChunkX + loadRadius; ++cx) {
      int dist = std::max(std::abs(cx - centerChunkX), std::abs(cz - centerChunkZ));
      ChunkGenStatus targetStatus = ChunkGenStatus::kSurface;
      if (dist <= fullGenRadius) {
        targetStatus = ChunkGenStatus::kFull;
      } else if (dist <= featureGenRadius) {
        targetStatus = ChunkGenStatus::kFeatures;
      }
      (void)ensureChunk(cx, cz, targetStatus);
    }
  }

  for (auto it = chunks.begin(); it != chunks.end();) {
    int cx = it->second.cx;
    int cz = it->second.cz;
    int dist = std::max(std::abs(cx - centerChunkX), std::abs(cz - centerChunkZ));
    if (dist > unloadRadius) {
      for (int sectionY = 0; sectionY < kChunkSectionCount; ++sectionY) {
        uint64_t key = sectionKey(cx, cz, sectionY);
        pendingRemovedMeshKeys.push_back(key);
        pendingMeshUploadSet.erase(key);
        pendingSectionRebuildSet.erase(key);
      }
      if (it->second.modified) {
        savedChunks[it->first] = it->second.blocks;
      }
      markChunkBoundaryNeighborsDirty(cx, cz);
      it = chunks.erase(it);
      meshDirty = true;
    } else {
      ++it;
    }
  }
}

bool World::waitForChunkRegion(int centerChunkX,
                               int centerChunkZ,
                               int radius,
                               int maxWaitMs,
                               ChunkGenStatus minStatus) {
  using clock = std::chrono::steady_clock;
  auto deadline = clock::now() + std::chrono::milliseconds(std::max(1, maxWaitMs));
  ChunkGenStatus requiredStatus =
    std::clamp(minStatus, ChunkGenStatus::kNoise, ChunkGenStatus::kFull);

  while (clock::now() < deadline) {
    pumpChunkGeneration();

    bool ready = true;
    for (int cz = centerChunkZ - radius; cz <= centerChunkZ + radius && ready; ++cz) {
      for (int cx = centerChunkX - radius; cx <= centerChunkX + radius; ++cx) {
        Chunk* chunk = findChunk(cx, cz);
        if (!chunk || chunk->generating || chunk->generatedStatus < requiredStatus) {
          ready = false;
          break;
        }
      }
    }

    if (ready) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  pumpChunkGeneration();

  for (int cz = centerChunkZ - radius; cz <= centerChunkZ + radius; ++cz) {
    for (int cx = centerChunkX - radius; cx <= centerChunkX + radius; ++cx) {
      Chunk* chunk = findChunk(cx, cz);
      if (!chunk || chunk->generating || chunk->generatedStatus < requiredStatus) {
        return false;
      }
    }
  }
  return true;
}

void World::simulateWater(int centerX, int centerZ, int radiusXZ, int maxUpdates) {
  pumpChunkGeneration();

  if (radiusXZ <= 0 || maxUpdates <= 0) {
    return;
  }

  int minX = centerX - radiusXZ;
  int maxX = centerX + radiusXZ;
  int minZ = centerZ - radiusXZ;
  int maxZ = centerZ + radiusXZ;

  int minChunkX = floorDiv(minX, kChunkSize);
  int maxChunkX = floorDiv(maxX, kChunkSize);
  int minChunkZ = floorDiv(minZ, kChunkSize);
  int maxChunkZ = floorDiv(maxZ, kChunkSize);

  struct WaterCell {
    int x = 0;
    int y = 0;
    int z = 0;
    uint8_t next = kAir;
    uint8_t current = kAir;
    int priority = 0;
  };

  std::vector<WaterCell> updates;
  updates.reserve(static_cast<size_t>(maxUpdates * 6));
  const size_t candidateBudget = static_cast<size_t>(maxUpdates * 32);
  std::unordered_set<uint64_t> touched;
  touched.reserve(candidateBudget);

  auto posHash = [](int x, int y, int z) -> uint64_t {
    uint64_t hx = static_cast<uint64_t>(mixBits(static_cast<uint32_t>(x)));
    uint64_t hy = static_cast<uint64_t>(mixBits(static_cast<uint32_t>(y)));
    uint64_t hz = static_cast<uint64_t>(mixBits(static_cast<uint32_t>(z)));
    return (hx << 32) ^ (hy << 16) ^ hz;
  };

  auto chunkReadyAt = [&](int x, int z) -> bool {
    int cx = floorDiv(x, kChunkSize);
    int cz = floorDiv(z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    return chunk && !chunk->generating && chunk->generatedStatus >= ChunkGenStatus::kNoise;
  };

  auto getLoadedBlock = [&](int x, int y, int z) -> uint8_t {
    if (!inBounds(x, y, z) || !chunkReadyAt(x, z)) {
      return kAir;
    }
    return getBlock(x, y, z);
  };

  auto computeDesired = [&](int x, int y, int z, uint8_t current) -> uint8_t {
    if (!inBounds(x, y, z) || !chunkReadyAt(x, z)) {
      return current;
    }

    if (current == kWater) {
      return kWater; // Source block never decays.
    }
    if (current != kAir && !isWaterBlock(current)) {
      return current;
    }

    uint8_t below = getLoadedBlock(x, y - 1, z);
    bool solidBelow = below != kAir && !isWaterBlock(below) && !isDecorationBlock(below);
    uint8_t above = getLoadedBlock(x, y + 1, z);
    if (isWaterBlock(above)) {
      return kWaterFlow1; // Vertical flow keeps strongest level.
    }

    constexpr std::array<std::pair<int, int>, 4> kSideOffsets = {{
      {1, 0},
      {-1, 0},
      {0, 1},
      {0, -1}
    }};

    int bestLevel = 99;
    int waterNeighbors = 0;
    int sourceNeighbors = 0;
    int strongNeighbors = 0;
    for (const auto& [ox, oz] : kSideOffsets) {
      uint8_t neighbor = getLoadedBlock(x + ox, y, z + oz);
      if (!isWaterBlock(neighbor)) {
        continue;
      }
      ++waterNeighbors;
      int level = static_cast<int>(waterLevelFromBlock(neighbor));
      if (level == 0) {
        ++sourceNeighbors;
        ++strongNeighbors;
        bestLevel = std::min(bestLevel, 1);
      } else if (level < 7) {
        if (level <= 2) {
          ++strongNeighbors;
        }
        bestLevel = std::min(bestLevel, level + 1);
      }
    }

    if (solidBelow && sourceNeighbors >= 2) {
      return kWater;
    }
    if (solidBelow && sourceNeighbors >= 1 && strongNeighbors >= 3 && bestLevel <= 2) {
      return kWater;
    }
    if (solidBelow && current == kAir && waterNeighbors >= 3 && bestLevel <= 2) {
      return kWater;
    }

    if (bestLevel <= 7) {
      return blockFromWaterLevel(static_cast<uint8_t>(bestLevel));
    }
    return kAir;
  };

  auto addCandidate = [&](int x, int y, int z) {
    if (updates.size() >= candidateBudget) {
      return;
    }
    if (!inBounds(x, y, z)) {
      return;
    }
    if (x < minX || x > maxX || z < minZ || z > maxZ) {
      return;
    }
    if (!chunkReadyAt(x, z)) {
      return;
    }

    uint64_t key = posHash(x, y, z);
    if (!touched.insert(key).second) {
      return;
    }

    uint8_t current = getBlock(x, y, z);
    if (current != kAir && !isWaterBlock(current)) {
      return;
    }

    uint8_t desired = computeDesired(x, y, z, current);
    if (desired == current) {
      return;
    }

    int priority = 2;
    if (current == kAir && isWaterBlock(desired)) {
      priority = (desired == kWaterFlow1 ? 0 : 1);
    } else if (isWaterBlock(current) && desired == kAir) {
      priority = 3;
    }

    updates.push_back({x, y, z, desired, current, priority});
  };

  constexpr std::array<std::pair<int, int>, 4> kSideOffsets = {{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1}
  }};

  bool reachedCandidateBudget = false;
  for (int cz = minChunkZ; cz <= maxChunkZ && !reachedCandidateBudget; ++cz) {
    for (int cx = minChunkX; cx <= maxChunkX && !reachedCandidateBudget; ++cx) {
      Chunk* chunk = findChunk(cx, cz);
      if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
        continue;
      }

      int baseX = cx * kChunkSize;
      int baseZ = cz * kChunkSize;
      int lxStart = std::clamp(minX - baseX, 0, kChunkSize - 1);
      int lxEnd = std::clamp(maxX - baseX, 0, kChunkSize - 1);
      int lzStart = std::clamp(minZ - baseZ, 0, kChunkSize - 1);
      int lzEnd = std::clamp(maxZ - baseZ, 0, kChunkSize - 1);

      for (int lz = lzStart; lz <= lzEnd && !reachedCandidateBudget; ++lz) {
        for (int lx = lxStart; lx <= lxEnd && !reachedCandidateBudget; ++lx) {
          int x = baseX + lx;
          int z = baseZ + lz;
          for (int y = 1; y < kChunkHeight - 1; ++y) {
            uint8_t block = chunk->blocks[static_cast<size_t>(localIndex(lx, y, lz))];
            if (!isWaterBlock(block)) {
              continue;
            }

            addCandidate(x, y, z);
            addCandidate(x, y - 1, z);
            addCandidate(x, y + 1, z);
            for (const auto& [ox, oz] : kSideOffsets) {
              addCandidate(x + ox, y, z + oz);
            }

            if (updates.size() >= candidateBudget) {
              reachedCandidateBudget = true;
              break;
            }
          }
        }
      }
    }
  }

  if (updates.empty()) {
    return;
  }

  std::stable_sort(updates.begin(),
                   updates.end(),
                   [](const WaterCell& a, const WaterCell& b) {
                     if (a.priority != b.priority) {
                       return a.priority < b.priority;
                     }
                     if (a.y != b.y) {
                       return a.y < b.y;
                     }
                     if (a.x != b.x) {
                       return a.x < b.x;
                     }
                     return a.z < b.z;
                   });

  int applied = 0;
  for (const WaterCell& cell : updates) {
    if (applied >= maxUpdates) {
      break;
    }
    if (!chunkReadyAt(cell.x, cell.z)) {
      continue;
    }

    int cx = floorDiv(cell.x, kChunkSize);
    int cz = floorDiv(cell.z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
      continue;
    }

    int lx = positiveMod(cell.x, kChunkSize);
    int lz = positiveMod(cell.z, kChunkSize);
    size_t idx = static_cast<size_t>(localIndex(lx, cell.y, lz));
    uint8_t current = chunk->blocks[idx];
    if (current != cell.current) {
      continue;
    }
    if (current == cell.next) {
      continue;
    }

    chunk->blocks[idx] = cell.next;
    markSectionAndNeighborsDirty(cx, cz, lx, cell.y, lz, true);
    chunk->modified = true;
    ++applied;
    meshDirty = true;
  }
}

void World::simulateFallingBlocks(int centerX, int centerZ, int radiusXZ, int maxUpdates) {
  pumpChunkGeneration();

  if (radiusXZ <= 0 || maxUpdates <= 0) {
    return;
  }

  int minX = centerX - radiusXZ;
  int maxX = centerX + radiusXZ;
  int minZ = centerZ - radiusXZ;
  int maxZ = centerZ + radiusXZ;

  int minChunkX = floorDiv(minX, kChunkSize);
  int maxChunkX = floorDiv(maxX, kChunkSize);
  int minChunkZ = floorDiv(minZ, kChunkSize);
  int maxChunkZ = floorDiv(maxZ, kChunkSize);

  auto chunkReadyAt = [&](int x, int z) -> bool {
    int cx = floorDiv(x, kChunkSize);
    int cz = floorDiv(z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    return chunk && !chunk->generating && chunk->generatedStatus >= ChunkGenStatus::kNoise;
  };

  struct FallingMove {
    int fromX = 0;
    int fromY = 0;
    int fromZ = 0;
    int toX = 0;
    int toY = 0;
    int toZ = 0;
    uint8_t type = kAir;
  };

  std::vector<FallingMove> moves;
  moves.reserve(static_cast<size_t>(maxUpdates * 3));

  for (int cz = minChunkZ; cz <= maxChunkZ; ++cz) {
    for (int cx = minChunkX; cx <= maxChunkX; ++cx) {
      Chunk* chunk = findChunk(cx, cz);
      if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
        continue;
      }

      int baseX = cx * kChunkSize;
      int baseZ = cz * kChunkSize;
      int lxStart = std::clamp(minX - baseX, 0, kChunkSize - 1);
      int lxEnd = std::clamp(maxX - baseX, 0, kChunkSize - 1);
      int lzStart = std::clamp(minZ - baseZ, 0, kChunkSize - 1);
      int lzEnd = std::clamp(maxZ - baseZ, 0, kChunkSize - 1);

      for (int y = 1; y < kChunkHeight; ++y) {
        for (int lz = lzStart; lz <= lzEnd; ++lz) {
          for (int lx = lxStart; lx <= lxEnd; ++lx) {
            size_t idx = static_cast<size_t>(localIndex(lx, y, lz));
            uint8_t type = chunk->blocks[idx];
            if (type != kSand) {
              continue;
            }

            int x = baseX + lx;
            int z = baseZ + lz;
            int belowY = y - 1;
            if (!inBounds(x, belowY, z) || !chunkReadyAt(x, z)) {
              continue;
            }

            uint8_t below = getBlock(x, belowY, z);
            if (below != kAir && !isWaterBlock(below)) {
              continue;
            }

            moves.push_back({x, y, z, x, belowY, z, type});
            if (static_cast<int>(moves.size()) >= maxUpdates * 4) {
              break;
            }
          }
          if (static_cast<int>(moves.size()) >= maxUpdates * 4) {
            break;
          }
        }
        if (static_cast<int>(moves.size()) >= maxUpdates * 4) {
          break;
        }
      }
    }
  }

  if (moves.empty()) {
    return;
  }

  std::stable_sort(moves.begin(),
                   moves.end(),
                   [](const FallingMove& a, const FallingMove& b) {
                     if (a.fromY != b.fromY) {
                       return a.fromY < b.fromY;
                     }
                     if (a.fromX != b.fromX) {
                       return a.fromX < b.fromX;
                     }
                     return a.fromZ < b.fromZ;
                   });

  struct CellRef {
    Chunk* chunk = nullptr;
    size_t idx = 0;
    int cx = 0;
    int cz = 0;
    int y = 0;
    int lx = 0;
    int lz = 0;
  };

  auto resolveCell = [&](int x, int y, int z, CellRef& out) -> bool {
    if (!inBounds(x, y, z)) {
      return false;
    }
    int cx = floorDiv(x, kChunkSize);
    int cz = floorDiv(z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
      return false;
    }
    int lx = positiveMod(x, kChunkSize);
    int lz = positiveMod(z, kChunkSize);
    out.chunk = chunk;
    out.idx = static_cast<size_t>(localIndex(lx, y, lz));
    out.cx = cx;
    out.cz = cz;
    out.y = y;
    out.lx = lx;
    out.lz = lz;
    return true;
  };

  auto markChunkEdited = [&](const CellRef& cell) {
    markSectionAndNeighborsDirty(cell.cx, cell.cz, cell.lx, cell.y, cell.lz, true);
    cell.chunk->modified = true;
  };

  int applied = 0;
  for (const FallingMove& move : moves) {
    if (applied >= maxUpdates) {
      break;
    }
    CellRef fromCell;
    CellRef toCell;
    if (!resolveCell(move.fromX, move.fromY, move.fromZ, fromCell) ||
        !resolveCell(move.toX, move.toY, move.toZ, toCell)) {
      continue;
    }

    uint8_t fromNow = fromCell.chunk->blocks[fromCell.idx];
    if (fromNow != move.type) {
      continue;
    }

    uint8_t toNow = toCell.chunk->blocks[toCell.idx];
    if (toNow != kAir && !isWaterBlock(toNow)) {
      continue;
    }

    toCell.chunk->blocks[toCell.idx] = move.type;
    if (isWaterBlock(toNow)) {
      fromCell.chunk->blocks[fromCell.idx] = toNow;
    } else {
      fromCell.chunk->blocks[fromCell.idx] = kAir;
    }
    markChunkEdited(fromCell);
    markChunkEdited(toCell);
    ++applied;
  }

  if (applied > 0) {
    meshDirty = true;
  }
}

bool World::consumeMeshDirty() {
  pumpChunkGeneration();
  pumpMeshRebuildResults();

  if (!meshDirty) {
    return false;
  }
  meshDirty = false;
  return true;
}

void World::setBreakOverlay(const glm::ivec3& block, int stage) {
  if (stage <= 0) {
    clearBreakOverlay();
    return;
  }
  int clampedStage = std::clamp(stage, 1, kBreakStages);
  if (breakOverlay.active && breakOverlay.block == block && breakOverlay.stage == clampedStage) {
    return;
  }

  auto markOverlayDirty = [&](const glm::ivec3& pos) {
    if (!inBounds(pos.x, pos.y, pos.z)) {
      return;
    }
    int cx = floorDiv(pos.x, kChunkSize);
    int cz = floorDiv(pos.z, kChunkSize);
    int lx = positiveMod(pos.x, kChunkSize);
    int lz = positiveMod(pos.z, kChunkSize);
    markSectionAndNeighborsDirty(cx, cz, lx, pos.y, lz, true);
    meshDirty = true;
  };

  if (breakOverlay.active) {
    markOverlayDirty(breakOverlay.block);
  }

  breakOverlay.active = true;
  breakOverlay.block = block;
  breakOverlay.stage = clampedStage;
  markOverlayDirty(block);
}

void World::clearBreakOverlay() {
  if (!breakOverlay.active) {
    return;
  }
  glm::ivec3 prevBlock = breakOverlay.block;
  breakOverlay.active = false;
  breakOverlay.stage = 0;
  if (inBounds(prevBlock.x, prevBlock.y, prevBlock.z)) {
    int cx = floorDiv(prevBlock.x, kChunkSize);
    int cz = floorDiv(prevBlock.z, kChunkSize);
    int lx = positiveMod(prevBlock.x, kChunkSize);
    int lz = positiveMod(prevBlock.z, kChunkSize);
    markSectionAndNeighborsDirty(cx, cz, lx, prevBlock.y, lz, true);
  }
  meshDirty = true;
}

bool World::buildChunkSectionMeshNow(Chunk& chunk, int sectionY) {
  if (!isChunkMeshReady(chunk)) {
    return false;
  }
  if (sectionY < 0 || sectionY >= kChunkSectionCount) {
    return false;
  }

  SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(sectionY)];
  std::array<uint8_t, kSectionSampleCount> samples{};
  std::array<float, kSectionSampleCount> skyLightSamples{};
  skyLightSamples.fill(0.0f);

  int baseX = chunk.cx * kChunkSize;
  int baseY = sectionY * kChunkSectionSize;
  int baseZ = chunk.cz * kChunkSize;
  Chunk* neighborGrid[3][3] = {{nullptr, nullptr, nullptr},
                               {nullptr, nullptr, nullptr},
                               {nullptr, nullptr, nullptr}};
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dx = -1; dx <= 1; ++dx) {
      Chunk* neighbor = findChunk(chunk.cx + dx, chunk.cz + dz);
      if (neighbor && isChunkMeshReady(*neighbor)) {
        neighborGrid[dz + 1][dx + 1] = neighbor;
      }
    }
  }

  auto sampleBlockForMesh = [&](int wx, int wy, int wz) -> uint8_t {
    if (wy < genSettings.minY || wy > genSettings.maxY) {
      return kAir;
    }
    if (wy < 0 || wy >= kChunkHeight) {
      return kAir;
    }

    int lx = wx - baseX;
    int lz = wz - baseZ;
    int chunkOffsetX = 1;
    int chunkOffsetZ = 1;
    if (lx < 0) {
      lx += kChunkSize;
      chunkOffsetX = 0;
    } else if (lx >= kChunkSize) {
      lx -= kChunkSize;
      chunkOffsetX = 2;
    }
    if (lz < 0) {
      lz += kChunkSize;
      chunkOffsetZ = 0;
    } else if (lz >= kChunkSize) {
      lz -= kChunkSize;
      chunkOffsetZ = 2;
    }

    Chunk* source = neighborGrid[chunkOffsetZ][chunkOffsetX];
    if (!source) {
      return (chunkOffsetX != 1 || chunkOffsetZ != 1) ? kStone : kAir;
    }
    return source->blocks[static_cast<size_t>(localIndex(lx, wy, lz))];
  };

  for (int sz = 0; sz < kSectionSampleSize; ++sz) {
    for (int sx = 0; sx < kSectionSampleSize; ++sx) {
      for (int sy = 0; sy < kSectionSampleSize; ++sy) {
        int wx = baseX + (sx - 1);
        int wy = baseY + (sy - 1);
        int wz = baseZ + (sz - 1);
        samples[static_cast<size_t>(sectionSampleIndex(sx, sy, sz))] = sampleBlockForMesh(wx, wy, wz);
      }
    }
  }
  computeSectionSkyLightSamples(
    baseX,
    baseY,
    baseZ,
    [&sampleBlockForMesh](int wx, int wy, int wz) {
      return sampleBlockForMesh(wx, wy, wz);
    },
    [this](int wx, int wz) {
      return sampleSurfaceHeightAt(wx, wz);
    },
    skyLightSamples);

  buildSectionMeshFromSamples(chunk.cx,
                              chunk.cz,
                              sectionY,
                              samples,
                              skyLightSamples,
                              isAprilFoolsPreset(genSettings),
                              breakOverlay.active && breakOverlay.stage > 0,
                              breakOverlay.block,
                              breakOverlay.stage,
                              section.vertices,
                              section.indices);
  section.dirty = false;
  section.queued = false;
  enqueueChunkMeshUpload(sectionKey(chunk.cx, chunk.cz, sectionY));

  chunk.dirty = false;
  for (const SectionRenderData& s : chunk.sectionMeshes) {
    if (s.dirty || s.queued) {
      chunk.dirty = true;
      break;
    }
  }
  return true;
}

void World::buildChunkMesh(World::Chunk& chunk) {
  if (!isChunkMeshReady(chunk)) {
    return;
  }
  for (int sectionY = 0; sectionY < kChunkSectionCount; ++sectionY) {
    SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(sectionY)];
    if (section.dirty || section.queued) {
      buildChunkSectionMeshNow(chunk, sectionY);
    }
  }
}

void World::buildMesh(std::vector<Vertex>& outVertices,
                      std::vector<uint32_t>& outIndices) {
  pumpChunkGeneration();
  pumpMeshRebuildResults();

  for (auto& entry : chunks) {
    Chunk& chunk = entry.second;
    if (!isChunkMeshReady(chunk)) {
      continue;
    }
    buildChunkMesh(chunk);
  }

  outVertices.clear();
  outIndices.clear();

  size_t totalVertexCount = 0;
  size_t totalIndexCount = 0;
  for (const auto& entry : chunks) {
    const Chunk& chunk = entry.second;
    if (!isChunkMeshReady(chunk)) {
      continue;
    }
    for (const SectionRenderData& section : chunk.sectionMeshes) {
      totalVertexCount += section.vertices.size();
      totalIndexCount += section.indices.size();
    }
  }

  outVertices.reserve(totalVertexCount);
  outIndices.reserve(totalIndexCount);
  for (const auto& entry : chunks) {
    const Chunk& chunk = entry.second;
    if (!isChunkMeshReady(chunk)) {
      continue;
    }
    for (const SectionRenderData& section : chunk.sectionMeshes) {
      uint32_t vertexOffset = static_cast<uint32_t>(outVertices.size());
      outVertices.insert(outVertices.end(), section.vertices.begin(), section.vertices.end());
      for (uint32_t idx : section.indices) {
        outIndices.push_back(idx + vertexOffset);
      }
    }
  }
}

void World::snapshotChunkMeshes(std::vector<ChunkMeshUpload>& outUploads) {
  pumpChunkGeneration();
  pumpMeshRebuildResults();

  resetMeshRebuildQueues();

  outUploads.clear();
  outUploads.reserve(chunks.size() * static_cast<size_t>(kChunkSectionCount));

  for (auto& entry : chunks) {
    Chunk& chunk = entry.second;
    if (!isChunkMeshReady(chunk)) {
      continue;
    }

    for (int sectionY = 0; sectionY < kChunkSectionCount; ++sectionY) {
      SectionRenderData& section = chunk.sectionMeshes[static_cast<size_t>(sectionY)];
      section.queued = false;
      if (section.dirty) {
        buildChunkSectionMeshNow(chunk, sectionY);
      }

      ChunkMeshUpload upload;
      upload.key = sectionKey(chunk.cx, chunk.cz, sectionY);
      upload.vertices = section.vertices;
      upload.indices = section.indices;
      outUploads.push_back(std::move(upload));
    }
  }

  pendingMeshUploadQueue.clear();
  pendingMeshUploadSet.clear();
  meshDirty = false;
}

bool World::consumeChunkMeshUpdates(std::vector<ChunkMeshUpload>& outUploads,
                                    std::vector<uint64_t>& outRemoved,
                                    int buildBudget,
                                    int uploadBudget) {
  pumpChunkGeneration();
  pumpMeshRebuildResults();

  outUploads.clear();
  outRemoved.clear();

  if (!pendingRemovedMeshKeys.empty()) {
    outRemoved = std::move(pendingRemovedMeshKeys);
    pendingRemovedMeshKeys.clear();
  }

  int remainingBuildBudget = std::max(0, buildBudget);
  while (remainingBuildBudget > 0 && !pendingSectionRebuildQueue.empty()) {
    uint64_t key = pendingSectionRebuildQueue.front();
    pendingSectionRebuildQueue.pop_front();
    pendingSectionRebuildSet.erase(key);

    int cx = 0;
    int cz = 0;
    int sectionY = 0;
    if (!decodeSectionKey(key, cx, cz, sectionY)) {
      continue;
    }

    Chunk* chunk = findChunk(cx, cz);
    if (!chunk || !isChunkMeshReady(*chunk)) {
      continue;
    }

    if (!queueSectionRebuildTask(*chunk, sectionY)) {
      continue;
    }
    --remainingBuildBudget;
  }

  pumpMeshRebuildResults();

  int remainingUploadBudget = std::max(0, uploadBudget);
  while (remainingUploadBudget > 0 && !pendingMeshUploadQueue.empty()) {
    uint64_t key = pendingMeshUploadQueue.front();
    pendingMeshUploadQueue.pop_front();
    pendingMeshUploadSet.erase(key);

    int cx = 0;
    int cz = 0;
    int sectionY = 0;
    if (!decodeSectionKey(key, cx, cz, sectionY)) {
      continue;
    }

    Chunk* chunk = findChunk(cx, cz);
    if (!chunk || !isChunkMeshReady(*chunk) ||
        sectionY < 0 || sectionY >= kChunkSectionCount) {
      continue;
    }
    SectionRenderData& section = chunk->sectionMeshes[static_cast<size_t>(sectionY)];
    if (section.dirty || section.queued) {
      continue;
    }

    ChunkMeshUpload upload;
    upload.key = key;
    upload.vertices = section.vertices;
    upload.indices = section.indices;
    outUploads.push_back(std::move(upload));
    --remainingUploadBudget;
  }

  bool hasWorkerBacklog = false;
  {
    std::lock_guard<std::mutex> lock(sectionRebuildMutex);
    hasWorkerBacklog = !sectionRebuildQueue.empty() || !sectionRebuildResults.empty();
  }

  bool hasPending = !pendingSectionRebuildQueue.empty() ||
                    !pendingMeshUploadQueue.empty() ||
                    !pendingRemovedMeshKeys.empty() ||
                    hasWorkerBacklog;
  meshDirty = hasPending;

  return !outUploads.empty() || !outRemoved.empty();
}

bool World::save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }

  auto writeBytes = [&](const void* data, std::streamsize size) {
    out.write(reinterpret_cast<const char*>(data), size);
    return out.good();
  };

  const char magic[4] = {'C', 'U', 'B', '2'};
  uint32_t version = 10;
  uint32_t cs = static_cast<uint32_t>(kChunkSize);
  uint32_t ch = static_cast<uint32_t>(kChunkHeight);
  uint32_t seedValue = static_cast<uint32_t>(seed);
  uint8_t presetValue = static_cast<uint8_t>(genSettings.preset);
  uint8_t structuresValue = genSettings.generateStructures ? 1u : 0u;
  float caveDensityValue = genSettings.caveDensity;
  float ravineFrequencyValue = genSettings.ravineFrequency;
  uint8_t startInventoryModeValue = genSettings.startInventoryMode;
  uint8_t cheatsEnabledValue = genSettings.cheatsEnabled ? 1u : 0u;
  int32_t minYValue = genSettings.minY;
  int32_t maxYValue = genSettings.maxY;

  uint32_t modifiedCount = 0;
  for (const auto& entry : chunks) {
    if (entry.second.modified) {
      ++modifiedCount;
    }
  }
  uint32_t storedCount = static_cast<uint32_t>(savedChunks.size()) + modifiedCount;

  if (!writeBytes(magic, 4) ||
      !writeBytes(&version, sizeof(version)) ||
      !writeBytes(&cs, sizeof(cs)) ||
      !writeBytes(&ch, sizeof(ch)) ||
      !writeBytes(&seedValue, sizeof(seedValue)) ||
      !writeBytes(&presetValue, sizeof(presetValue)) ||
      !writeBytes(&structuresValue, sizeof(structuresValue)) ||
      !writeBytes(&caveDensityValue, sizeof(caveDensityValue)) ||
      !writeBytes(&ravineFrequencyValue, sizeof(ravineFrequencyValue)) ||
      !writeBytes(&startInventoryModeValue, sizeof(startInventoryModeValue)) ||
      !writeBytes(&cheatsEnabledValue, sizeof(cheatsEnabledValue)) ||
      !writeBytes(&minYValue, sizeof(minYValue)) ||
      !writeBytes(&maxYValue, sizeof(maxYValue)) ||
      !writeBytes(&storedCount, sizeof(storedCount))) {
    return false;
  }

  for (const auto& entry : savedChunks) {
    uint64_t key = entry.first;
    int32_t cx = static_cast<int32_t>(static_cast<uint32_t>(key >> 32));
    int32_t cz = static_cast<int32_t>(static_cast<uint32_t>(key & 0xffffffffu));
    if (!writeBytes(&cx, sizeof(cx)) ||
        !writeBytes(&cz, sizeof(cz)) ||
        !writeBytes(entry.second.data(), static_cast<std::streamsize>(entry.second.size()))) {
      return false;
    }
  }

  for (const auto& entry : chunks) {
    if (!entry.second.modified) {
      continue;
    }
    int32_t cx = static_cast<int32_t>(entry.second.cx);
    int32_t cz = static_cast<int32_t>(entry.second.cz);
    if (!writeBytes(&cx, sizeof(cx)) ||
        !writeBytes(&cz, sizeof(cz)) ||
        !writeBytes(entry.second.blocks.data(),
                    static_cast<std::streamsize>(entry.second.blocks.size()))) {
      return false;
    }
  }

  out.flush();
  return out.good();
}

bool World::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }

  auto readBytes = [&](void* data, std::streamsize size) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(data), size));
  };

  char magic[4] = {};
  uint32_t version = 0;
  uint32_t cs = 0;
  uint32_t ch = 0;
  uint32_t seedValue = 0;
  uint8_t presetValue = static_cast<uint8_t>(WorldPreset::kMinecraftStyle);
  uint8_t structuresValue = 1;
  float caveDensityValue = 1.0f;
  float ravineFrequencyValue = 1.0f;
  uint8_t startInventoryModeValue = 0;
  uint8_t cheatsEnabledValue = 0;
  int32_t minYValue = 0;
  int32_t maxYValue = kChunkHeight - 1;
  uint32_t storedCount = 0;

  if (!readBytes(magic, 4) ||
      !readBytes(&version, sizeof(version)) ||
      !readBytes(&cs, sizeof(cs)) ||
      !readBytes(&ch, sizeof(ch)) ||
      !readBytes(&seedValue, sizeof(seedValue))) {
    return false;
  }

  if (std::strncmp(magic, "CUB2", 4) != 0 ||
      (version != 7 && version != 8 && version != 9 && version != 10) ||
      cs != static_cast<uint32_t>(kChunkSize) ||
      ch != static_cast<uint32_t>(kChunkHeight)) {
    return false;
  }

  if (version >= 8) {
    if (!readBytes(&presetValue, sizeof(presetValue)) ||
        !readBytes(&structuresValue, sizeof(structuresValue)) ||
        !readBytes(&caveDensityValue, sizeof(caveDensityValue)) ||
        !readBytes(&ravineFrequencyValue, sizeof(ravineFrequencyValue)) ||
        !readBytes(&startInventoryModeValue, sizeof(startInventoryModeValue))) {
      return false;
    }
    if (version >= 10) {
      if (!readBytes(&cheatsEnabledValue, sizeof(cheatsEnabledValue))) {
        return false;
      }
    }
    if (version >= 9) {
      if (!readBytes(&minYValue, sizeof(minYValue)) ||
          !readBytes(&maxYValue, sizeof(maxYValue))) {
        return false;
      }
    }
  }
  if (!readBytes(&storedCount, sizeof(storedCount))) {
    return false;
  }

  seed = static_cast<int>(seedValue);
  if (presetValue > static_cast<uint8_t>(WorldPreset::kAprilFools)) {
    presetValue = static_cast<uint8_t>(WorldPreset::kMinecraftStyle);
  }
  WorldGenSettings loadedSettings{};
  loadedSettings.preset = static_cast<WorldPreset>(presetValue);
  loadedSettings.generateStructures = structuresValue != 0;
  loadedSettings.caveDensity = caveDensityValue;
  loadedSettings.ravineFrequency = ravineFrequencyValue;
  loadedSettings.startInventoryMode = startInventoryModeValue;
  loadedSettings.cheatsEnabled = cheatsEnabledValue != 0;
  loadedSettings.minY = static_cast<int>(minYValue);
  loadedSettings.maxY = static_cast<int>(maxYValue);
  setGenerationSettings(loadedSettings);
  resetChunkGeneration();
  resetMeshRebuildQueues();
  chunks.clear();
  savedChunks.clear();
  breakOverlay.active = false;
  breakOverlay.stage = 0;

  size_t blocksSize = static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight);
  for (uint32_t i = 0; i < storedCount; ++i) {
    int32_t cx = 0;
    int32_t cz = 0;
    if (!readBytes(&cx, sizeof(cx)) ||
        !readBytes(&cz, sizeof(cz))) {
      return false;
    }
    std::vector<uint8_t> blocks(blocksSize);
    if (!readBytes(blocks.data(), static_cast<std::streamsize>(blocks.size()))) {
      return false;
    }
    savedChunks[chunkKey(cx, cz)] = std::move(blocks);
  }

  meshDirty = true;
  return true;
}
