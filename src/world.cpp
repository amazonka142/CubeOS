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

float fbmNoise(float x, float z, int seed) {
  float amplitude = 1.0f;
  float frequency = 0.008f;
  float total = 0.0f;
  float maxValue = 0.0f;
  float seedX = static_cast<float>(seed) * 0.13f;
  float seedZ = static_cast<float>(seed) * 0.17f;
  for (int i = 0; i < 4; ++i) {
    glm::vec2 p((x + seedX) * frequency, (z + seedZ) * frequency);
    total += glm::perlin(p) * amplitude;
    maxValue += amplitude;
    amplitude *= 0.5f;
    frequency *= 2.0f;
  }
  if (maxValue <= 0.0f) {
    return 0.0f;
  }
  return total / maxValue;
}

float saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

float smooth01(float v) {
  float t = saturate(v);
  return t * t * (3.0f - 2.0f * t);
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
  int oreVeinCoalMin = 12;
  int oreVeinCoalMax = 17;
  int oreVeinIronMin = 6;
  int oreVeinIronMax = 9;
  int oreVeinGoldMin = 6;
  int oreVeinGoldMax = 9;
  int oreCoalMinY = 0;
  int oreCoalMaxY = kChunkHeight - 1;
  int oreIronMinY = 0;
  int oreIronMaxY = 63;
  int oreGoldMinY = 0;
  int oreGoldMaxY = 31;
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
  }

  return tuning;
}

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

  std::array<OreVeinRule, 3> oreRules = {{
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
     tuning.oreVeinGoldMax}
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
  kMountains = 5
};

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
  const float seedF = static_cast<float>(seed);

  auto columnIndex = [](int lx, int lz) {
    return static_cast<size_t>(lx + lz * kChunkSize);
  };

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      size_t idx = columnIndex(lx, lz);

      if (settings.preset == WorldPreset::kClassicFlat) {
        outBiomeMap[idx] = static_cast<uint8_t>(DebugBiomeId::kPlains);
        outClimateMap[idx] = {
          0.58f, // temperature
          0.52f, // humidity
          0.62f, // continentalness
          0.50f, // erosion
          0.50f, // depth
          0.50f  // weirdness
        };
        continue;
      }

      float warpX = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 101.0f) * 0.0016f,
        (static_cast<float>(worldZ) - seedF * 89.0f) * 0.0016f));
      float warpZ = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) - seedF * 37.0f) * 0.0016f,
        (static_cast<float>(worldZ) + seedF * 53.0f) * 0.0016f));
      float wx = static_cast<float>(worldX) + warpX * 24.0f;
      float wz = static_cast<float>(worldZ) + warpZ * 24.0f;

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
      float spawnLandBias = smooth01(1.0f - distFromOrigin / 768.0f) * 0.24f;
      continentalness = std::clamp(continentalness + 0.10f + spawnLandBias, 0.0f, 1.0f);
      float erosion = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx - seedF * 37.0f) * 0.00120f,
        (wz + seedF * 23.0f) * 0.00120f));
      float weirdness = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx + seedF * 61.0f) * 0.00185f,
        (wz - seedF * 53.0f) * 0.00185f));
      float depth = 0.5f + 0.5f * glm::perlin(glm::vec2(
        (wx - seedF * 29.0f) * 0.00135f,
        (wz + seedF * 31.0f) * 0.00135f));

      DebugBiomeId biome = DebugBiomeId::kPlains;
      if (continentalness < 0.18f) {
        biome = DebugBiomeId::kOcean;
      } else if (continentalness < 0.24f) {
        biome = DebugBiomeId::kBeach;
      } else if (temperature > 0.72f && humidity < 0.34f) {
        biome = DebugBiomeId::kDesert;
      } else if (weirdness > 0.67f && continentalness > 0.56f) {
        biome = DebugBiomeId::kMountains;
      } else if (humidity > 0.58f) {
        biome = DebugBiomeId::kForest;
      }

      outBiomeMap[idx] = static_cast<uint8_t>(biome);
      outClimateMap[idx] = {
        temperature,
        humidity,
        continentalness,
        erosion,
        depth,
        weirdness
      };
    }
  }
}

constexpr int kStageSeaLevel = 32;
constexpr int kCarverRegionSizeChunks = 8;
constexpr int kStructureRegionSizeChunks = 32;

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
  const float seedF = static_cast<float>(seed);
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

  auto setLocalBlock = [&](int lx, int y, int lz, uint8_t type) {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return;
    }
    outBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
  };

  auto getLocalBlock = [&](int lx, int y, int lz) -> uint8_t {
    if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize || y < 0 || y >= kChunkHeight) {
      return kAir;
    }
    return outBlocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))];
  };

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      size_t column = static_cast<size_t>(lx + lz * kChunkSize);
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
      const BiomeClimateSample& climate = climateMap[column];
      uint8_t biomeId = biomeMap[column];

      float continentalness = climate.continentalness;
      float erosion = climate.erosion;
      float weirdness = climate.weirdness;
      float mountainLift = std::max(0.0f, weirdness - 0.55f);
      float broad = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 23.0f) * 0.0017f,
        (static_cast<float>(worldZ) - seedF * 19.0f) * 0.0017f));
      float detail = fbmNoise(static_cast<float>(worldX), static_cast<float>(worldZ), seed);

      float targetHeight = static_cast<float>(kStageSeaLevel) - 8.0f +
                           continentalness * 56.0f +
                           (1.0f - erosion) * 10.0f +
                           mountainLift * 40.0f +
                           broad * 7.0f +
                           detail * 4.0f;
      if (biomeId == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
        targetHeight -= 12.0f;
      } else if (biomeId == static_cast<uint8_t>(DebugBiomeId::kBeach)) {
        targetHeight -= 4.0f;
      } else if (biomeId == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
        targetHeight += 2.0f;
      } else if (biomeId == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
        targetHeight += 12.0f;
      }
      int heightClampMin = minY + 4;
      int heightClampMax = maxY - 4;
      if (heightClampMax < heightClampMin) {
        heightClampMin = minY;
        heightClampMax = maxY;
      }
      targetHeight = std::clamp(targetHeight,
                                static_cast<float>(heightClampMin),
                                static_cast<float>(heightClampMax));

      for (int y = minY; y <= maxY; ++y) {
        float vertical = (targetHeight - static_cast<float>(y)) * 0.11f;
        float macro3d = glm::perlin(glm::vec3(
          (static_cast<float>(worldX) + seedF * 31.0f) * 0.041f,
          (static_cast<float>(y) - seedF * 17.0f) * 0.048f,
          (static_cast<float>(worldZ) - seedF * 29.0f) * 0.041f));
        float micro3d = glm::perlin(glm::vec3(
          (static_cast<float>(worldX) - seedF * 7.0f) * 0.089f,
          (static_cast<float>(y) + seedF * 13.0f) * 0.098f,
          (static_cast<float>(worldZ) + seedF * 11.0f) * 0.089f));
        float cheese = std::abs(glm::perlin(glm::vec3(
          (static_cast<float>(worldX) + seedF * 41.0f) * 0.064f,
          (static_cast<float>(y) - seedF * 37.0f) * 0.066f,
          (static_cast<float>(worldZ) - seedF * 43.0f) * 0.064f)));
        float spaghetti = 1.0f - std::abs(glm::perlin(glm::vec3(
          (static_cast<float>(worldX) - seedF * 13.0f) * 0.023f,
          (static_cast<float>(y) + seedF * 29.0f) * 0.031f,
          (static_cast<float>(worldZ) + seedF * 17.0f) * 0.023f)));

        float caveSignal = std::max(1.0f - cheese * 1.28f, spaghetti * 0.84f);
        float caveThreshold = std::clamp(0.78f - settings.caveDensity * 0.10f, 0.48f, 0.78f);
        float rawCaveCut = caveSignal > caveThreshold
          ? (caveSignal - caveThreshold) * 16.0f
          : 0.0f;
        float depthFactor = smooth01((static_cast<float>(kStageSeaLevel + 16 - y)) / 56.0f);
        float belowSurface = targetHeight - static_cast<float>(y);
        float surfaceFactor = std::clamp(belowSurface / 6.0f, 0.0f, 1.0f);
        float caveCut = rawCaveCut * depthFactor * surfaceFactor;

        float density = vertical + macro3d * 0.86f + micro3d * 0.34f - caveCut;
        if (y < minY + 2) {
          density += 2.4f;
        }
        if (density > 0.0f) {
          setLocalBlock(lx, y, lz, kStone);
        } else {
          setLocalBlock(lx, y, lz, kAir);
        }
      }

      float aqNoise = glm::perlin(glm::vec2(
        (static_cast<float>(worldX) + seedF * 61.0f) * 0.0105f,
        (static_cast<float>(worldZ) - seedF * 59.0f) * 0.0105f));
      int aquiferLevel = kStageSeaLevel - 4 + static_cast<int>(std::round(aqNoise * 8.0f));
      if (continentalness < 0.24f) {
        aquiferLevel = std::max(aquiferLevel, kStageSeaLevel);
      } else {
        aquiferLevel = std::min(aquiferLevel, kStageSeaLevel - 2);
      }
      int aquiferClampMin = minY + 2;
      int aquiferClampMax = maxY - 2;
      if (aquiferClampMax < aquiferClampMin) {
        aquiferClampMin = minY;
        aquiferClampMax = maxY;
      }
      aquiferLevel = std::clamp(aquiferLevel, aquiferClampMin, aquiferClampMax);

      for (int y = minY; y <= maxY; ++y) {
        uint8_t current = getLocalBlock(lx, y, lz);
        if (current != kAir) {
          continue;
        }

        int fluidFillTopY = std::min(aquiferLevel, kStageSeaLevel);
        if (y <= fluidFillTopY) {
          setLocalBlock(lx, y, lz, kWater);
        }
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
      uint8_t top = kGrass;
      uint8_t filler = kDirt;
      int fillerDepth = 3;

      if (biome == static_cast<uint8_t>(DebugBiomeId::kOcean) ||
          biome == static_cast<uint8_t>(DebugBiomeId::kBeach)) {
        top = kSand;
        filler = kSand;
        fillerDepth = 4;
      } else if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert)) {
        top = kSand;
        filler = kSand;
        fillerDepth = 5;
      } else if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
        if (topY > kStageSeaLevel + 30) {
          top = kStone;
          filler = kStone;
          fillerDepth = 2;
        } else {
          top = kGravel;
          filler = kStone;
          fillerDepth = 3;
        }
      }

      if (topY <= kStageSeaLevel + 1) {
        top = kSand;
        filler = kSand;
        fillerDepth = std::max(fillerDepth, 3);
      }
      if (climate.temperature < 0.28f && topY > kStageSeaLevel + 20) {
        top = kStone;
        filler = kStone;
      }

      setLocalBlock(lx, topY, lz, top);
      for (int d = 1; d <= fillerDepth; ++d) {
        int y = topY - d;
        if (y < minY) {
          break;
        }
        uint8_t current = getLocalBlock(lx, y, lz);
        if (current == kAir || isWaterBlock(current)) {
          break;
        }
        if (current == kStone || current == kDirt || current == kGrass || current == kSand || current == kGravel) {
          setLocalBlock(lx, y, lz, filler);
        }
      }

      if (top == kSand && fillerDepth >= 4) {
        for (int d = fillerDepth + 1; d <= fillerDepth + 3; ++d) {
          int y = topY - d;
          if (y < minY) {
            break;
          }
          uint8_t current = getLocalBlock(lx, y, lz);
          if (current == kStone) {
            setLocalBlock(lx, y, lz, kSand);
          }
        }
      }

      int worldX = baseX + lx;
      int worldZ = baseZ + lz;
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
  int length = 0;
  bool liquid = false;
};

void collectCarverEvents(int chunkX,
                         int chunkZ,
                         int seed,
                         bool liquid,
                         std::vector<CarverEvent>& outEvents) {
  outEvents.clear();

  int minRegionX = floorDiv(chunkX - 2, kCarverRegionSizeChunks);
  int maxRegionX = floorDiv(chunkX + 2, kCarverRegionSizeChunks);
  int minRegionZ = floorDiv(chunkZ - 2, kCarverRegionSizeChunks);
  int maxRegionZ = floorDiv(chunkZ + 2, kCarverRegionSizeChunks);

  constexpr float kPi = 3.14159265358979323846f;
  for (int rz = minRegionZ; rz <= maxRegionZ; ++rz) {
    for (int rx = minRegionX; rx <= maxRegionX; ++rx) {
      uint32_t state = static_cast<uint32_t>(
        hashChunkSeed(seed, rx, rz, liquid ? 0x1A11u : 0xCA11u));
      int attempts = liquid ? 2 : 3;
      for (int i = 0; i < attempts; ++i) {
        float chance = liquid ? 0.32f : 0.58f;
        if (rand01(state) > chance) {
          continue;
        }

        int startChunkX = rx * kCarverRegionSizeChunks + randIntInclusive(state, 0, kCarverRegionSizeChunks - 1);
        int startChunkZ = rz * kCarverRegionSizeChunks + randIntInclusive(state, 0, kCarverRegionSizeChunks - 1);
        float startX = static_cast<float>(startChunkX * kChunkSize + randIntInclusive(state, 0, kChunkSize - 1));
        float startZ = static_cast<float>(startChunkZ * kChunkSize + randIntInclusive(state, 0, kChunkSize - 1));
        float startY = liquid
          ? static_cast<float>(randIntInclusive(state, 2, 18))
          : static_cast<float>(randIntInclusive(state, 10, kChunkHeight - 20));
        float yaw = rand01(state) * 2.0f * kPi;
        float pitch = (rand01(state) - 0.5f) * (liquid ? 0.45f : 0.30f);
        float radius = liquid
          ? (1.4f + rand01(state) * 2.2f)
          : (1.7f + rand01(state) * 3.4f);
        int length = liquid
          ? randIntInclusive(state, 26, 72)
          : randIntInclusive(state, 36, 128);
        outEvents.push_back({startX, startY, startZ, yaw, pitch, radius, length, liquid});
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
  collectCarverEvents(chunkX, chunkZ, seed, liquidPass, events);

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
      float radiusWave = 0.75f + 0.45f * std::sin(t * kPi);
      float radius = std::max(1.1f, event.baseRadius * radiusWave);

      int minWX = static_cast<int>(std::floor(x - radius)) - 1;
      int maxWX = static_cast<int>(std::floor(x + radius)) + 1;
      int minWZ = static_cast<int>(std::floor(z - radius)) - 1;
      int maxWZ = static_cast<int>(std::floor(z + radius)) + 1;
      int minWY = std::max(minY, static_cast<int>(std::floor(y - radius)) - 1);
      int maxWY = std::min(maxY, static_cast<int>(std::floor(y + radius)) + 1);

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
              float ny = (static_cast<float>(wy) + 0.5f - y) / (radius * 0.9f);
              float nz = (static_cast<float>(wz) + 0.5f - z) / radius;
              float dist = nx * nx + ny * ny + nz * nz;
              if (dist >= 1.0f) {
                continue;
              }

              int lx = wx - baseX;
              int lz = wz - baseZ;
              uint8_t current = getLocal(lx, wy, lz);
              if (current == kAir || isWaterBlock(current) || current == kLeaves || current == kWood) {
                continue;
              }

              if (liquidPass) {
                uint8_t fluid = kWater;
                setLocal(lx, wy, lz, fluid);
              } else {
                setLocal(lx, wy, lz, kAir);
              }
            }
          }
        }
      }

      float speed = liquidPass ? 1.25f : 1.55f;
      x += std::cos(yaw) * std::cos(pitch) * speed;
      y += std::sin(pitch) * speed;
      z += std::sin(yaw) * std::cos(pitch) * speed;
      yaw += (rand01(state) - 0.5f) * (liquidPass ? 0.19f : 0.13f);
      pitch = std::clamp(pitch + (rand01(state) - 0.5f) * (liquidPass ? 0.11f : 0.07f), -0.65f, 0.65f);
    }
  }

  applyHeightRangeMask(ioBlocks, minY, maxY);
}

void placeOreFeatures(int cx,
                      int cz,
                      int seed,
                      const WorldGenSettings& settings,
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
  };

  std::array<OreRule, 3> rules = {{
    {kCoalOre, 0xC011u, 18, std::max(minY, 6), maxY, 8, 16},
    {kIronOre, 0x1A2Bu, 16, std::max(minY, 4), std::min(maxY, 74), 5, 10},
    {kGoldOre, 0x90D1u, 4, minY, std::min(maxY, 24), 4, 8}
  }};

  for (const OreRule& rule : rules) {
    uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, cx, cz, rule.salt));
    int oreMinY = std::clamp(rule.minY, minY, maxY);
    int oreMaxY = std::clamp(rule.maxY, oreMinY, maxY);

    for (int attempt = 0; attempt < rule.attempts; ++attempt) {
      int lx = randIntInclusive(state, 0, kChunkSize - 1);
      int lz = randIntInclusive(state, 0, kChunkSize - 1);
      int y = randIntInclusive(state, oreMinY, oreMaxY);
      int vein = randIntInclusive(state, rule.minVein, rule.maxVein);

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

void placeTreeFeatures(int cx,
                       int cz,
                       int seed,
                       const WorldGenSettings& settings,
                       const std::array<uint8_t, kChunkColumnCount>& biomeMap,
                       std::vector<uint8_t>& ioBlocks) {
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
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
    float chance = 0.10f;
    if (biome == static_cast<uint8_t>(DebugBiomeId::kForest)) {
      chance = 0.34f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kPlains)) {
      chance = 0.08f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kDesert) ||
               biome == static_cast<uint8_t>(DebugBiomeId::kBeach) ||
               biome == static_cast<uint8_t>(DebugBiomeId::kOcean)) {
      chance = 0.0f;
    } else if (biome == static_cast<uint8_t>(DebugBiomeId::kMountains)) {
      chance = 0.05f;
    }

    if (rand01(state) > chance) {
      continue;
    }

    int trunkHeight = randIntInclusive(state, 4, 6);
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

    for (int y = topY + 1; y <= topY + trunkHeight; ++y) {
      setLocal(lx, y, lz, kWood);
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
            setLocal(tx, y, tz, kLeaves);
          }
        }
      }
    }

    int worldX = baseX + lx;
    int worldZ = baseZ + lz;
    if (hashedNoise01(worldX, topY, worldZ, seed, 0x4C41u) > 0.82f && topY + trunkHeight + 1 <= maxY) {
      setLocal(lx, topY + trunkHeight + 1, lz, kLeaves);
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
        if (t != kAir && !isWaterBlock(t) && !isUnderwaterPlantBlock(t)) {
          topY = y;
          break;
        }
      }

      if (topY < minY || topY + 1 > maxY) {
        continue;
      }

      uint8_t ground = getLocal(lx, topY, lz);
      if (ground != kSand && ground != kGravel && ground != kDirt && ground != kStone) {
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

      float coralChance = 0.06f;
      if (ground == kGravel || ground == kStone) {
        coralChance += 0.18f;
      }
      if (waterDepth >= 4) {
        coralChance += 0.06f;
      }
      coralChance = std::clamp(coralChance, 0.0f, 0.42f);
      if (rand01(state) < coralChance) {
        setLocal(lx, topY + 1, lz, kCoral);
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

  int regionX = floorDiv(cx, kStructureRegionSizeChunks);
  int regionZ = floorDiv(cz, kStructureRegionSizeChunks);
  for (int rz = regionZ - 1; rz <= regionZ + 1; ++rz) {
    for (int rx = regionX - 1; rx <= regionX + 1; ++rx) {
      uint32_t state = static_cast<uint32_t>(hashChunkSeed(seed, rx, rz, 0x5EEDu));
      if (rand01(state) > 0.22f) {
        continue;
      }

      int startChunkX = rx * kStructureRegionSizeChunks + randIntInclusive(state, 0, kStructureRegionSizeChunks - 1);
      int startChunkZ = rz * kStructureRegionSizeChunks + randIntInclusive(state, 0, kStructureRegionSizeChunks - 1);
      int centerX = startChunkX * kChunkSize + randIntInclusive(state, 4, kChunkSize - 5);
      int centerZ = startChunkZ * kChunkSize + randIntInclusive(state, 4, kChunkSize - 5);

      int centerChunkLocalX = centerX - baseX;
      int centerChunkLocalZ = centerZ - baseZ;
      if ((centerChunkLocalX < -6 || centerChunkLocalX > kChunkSize + 5) &&
          (centerChunkLocalZ < -6 || centerChunkLocalZ > kChunkSize + 5)) {
        continue;
      }

      int foundationMinY = minY + 2;
      int foundationMaxY = maxY - 8;
      if (foundationMaxY < foundationMinY) {
        continue;
      }
      int foundationY = randIntInclusive(state, foundationMinY, foundationMaxY);
      for (int oz = -3; oz <= 3; ++oz) {
        for (int ox = -3; ox <= 3; ++ox) {
          int wx = centerX + ox;
          int wz = centerZ + oz;
          setWorldIfInChunk(wx, foundationY, wz, kStone);
          if (std::abs(ox) == 3 || std::abs(oz) == 3) {
            setWorldIfInChunk(wx, foundationY + 1, wz, kStone);
          }
        }
      }

      std::array<std::pair<int, int>, 4> corners = {{
        {-2, -2}, {2, -2}, {-2, 2}, {2, 2}
      }};
      for (const auto& [ox, oz] : corners) {
        int pillarH = randIntInclusive(state, 3, 6);
        for (int y = 1; y <= pillarH; ++y) {
          setWorldIfInChunk(centerX + ox, foundationY + y, centerZ + oz, kStone);
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
                     std::vector<uint8_t>& ioBlocks) {
  placeOreFeatures(cx, cz, seed, settings, ioBlocks);
  if (settings.generateStructures) {
    placeRegionalStructureFeatures(cx, cz, seed, settings, ioBlocks);
  }
  placeTreeFeatures(cx, cz, seed, settings, biomeMap, ioBlocks);
  placeUnderwaterPlantFeatures(cx, cz, seed, settings, biomeMap, ioBlocks);
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
  runFeatureStage(cx, cz, seed, settings, out.biomeMap, out.blocks);
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
  startChunkWorkers();
}

World::~World() {
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

void World::generateChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus) {
  if (targetStatus <= ChunkGenStatus::kEmpty) {
    return;
  }

  pumpChunkGeneration();
  Chunk& chunk = ensureChunk(chunkX, chunkZ);
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
    pendingGenerationEpochByKey.erase(chunkKey(chunkX, chunkZ));
    chunk.dirty = true;
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

void World::startChunkWorkers() {
  uint32_t hw = std::thread::hardware_concurrency();
  size_t workerCount = hw == 0 ? 2u : static_cast<size_t>(hw);
  workerCount = std::max<size_t>(1, std::min<size_t>(workerCount, 4));

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

          task = generationQueue.front();
          generationQueue.pop();
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

void World::queueChunkGeneration(int cx, int cz, uint32_t epoch, ChunkGenStatus targetStatus) {
  uint64_t key = chunkKey(cx, cz);
  auto pendingIt = pendingGenerationEpochByKey.find(key);
  if (pendingIt != pendingGenerationEpochByKey.end() && pendingIt->second == epoch) {
    return;
  }
  pendingGenerationEpochByKey[key] = epoch;

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    generationQueue.push({cx, cz, seed, genSettings, epoch, key, targetStatus});
  }
  generationCv.notify_one();
}

void World::resetChunkGeneration() {
  ++generationEpoch;
  pendingGenerationEpochByKey.clear();

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    while (!generationQueue.empty()) {
      generationQueue.pop();
    }
    while (!generationResults.empty()) {
      generationResults.pop();
    }
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
      neighbor->dirty = true;
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
    chunk.biomeMap = result.biomeMap;
    chunk.climateMap = result.climateMap;
    chunk.generatedStatus = result.status;
    chunk.generating = false;
    chunk.dirty = true;
    markNeighborChunksDirty(chunk.cx, chunk.cz);
    meshDirty = true;
  }
}

World::Chunk& World::ensureChunk(int cx, int cz) {
  uint64_t key = chunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
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
    chunk.modified = true;
    chunk.generatedStatus = ChunkGenStatus::kFull;
    savedChunks.erase(savedIt);
  } else {
    chunk.generating = true;
    chunk.generationEpoch = generationEpoch;
  }

  auto insertResult = chunks.emplace(key, std::move(chunk));
  if (insertResult.first->second.generating) {
    queueChunkGeneration(cx, cz, insertResult.first->second.generationEpoch, ChunkGenStatus::kFull);
  } else {
    markNeighborChunksDirty(cx, cz);
  }
  meshDirty = true;
  return insertResult.first->second;
}

void World::generateChunk(World::Chunk& chunk, ChunkGenStatus targetStatus) {
  GeneratedChunkData generated =
    generateChunkDataToStatus(chunk.cx, chunk.cz, seed, genSettings, targetStatus);
  chunk.blocks = std::move(generated.blocks);
  chunk.biomeMap = generated.biomeMap;
  chunk.climateMap = generated.climateMap;
  chunk.generatedStatus = generated.status;
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
    pendingGenerationEpochByKey.erase(chunkKey(cx, cz));
  }

  size_t idx = static_cast<size_t>(localIndex(lx, y, lz));
  if (chunk->blocks[idx] == type) {
    return;
  }
  chunk->blocks[idx] = type;
  chunk->dirty = true;
  chunk->modified = true;
  if (lx == 0 || lx == kChunkSize - 1 || lz == 0 || lz == kChunkSize - 1) {
    markNeighborChunksDirty(cx, cz);
  }
  meshDirty = true;
}

glm::vec3 World::blockColor(uint8_t type) const {
  if (isWaterBlock(type)) {
    return {0.22f, 0.45f, 0.88f};
  }

  switch (type) {
    case kGrass:
      return {0.2f, 0.8f, 0.2f};
    case kDirt:
      return {0.55f, 0.35f, 0.2f};
    case kSand:
      return {0.86f, 0.78f, 0.50f};
    case kGravel:
      return {0.46f, 0.44f, 0.42f};
    case kWood:
      return {0.49f, 0.33f, 0.16f};
    case kLeaves:
      return {0.16f, 0.58f, 0.16f};
    case kSeagrass:
      return {0.14f, 0.62f, 0.34f};
    case kCoral:
      return {0.88f, 0.48f, 0.40f};
    case kCoalOre:
      return {0.22f, 0.22f, 0.22f};
    case kIronOre:
      return {0.73f, 0.54f, 0.40f};
    case kGoldOre:
      return {0.92f, 0.75f, 0.22f};
    case kStone:
      return {0.6f, 0.6f, 0.6f};
    default:
      return {1.0f, 1.0f, 1.0f};
  }
}

void World::generate() {
  resetChunkGeneration();
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

  // Keep a small hysteresis ring to avoid visible world cutoffs while new chunks
  // are still generating near the view boundary.
  const int loadRadius = radius + 1;
  const int unloadRadius = radius + 2;

  for (int cz = centerChunkZ - loadRadius; cz <= centerChunkZ + loadRadius; ++cz) {
    for (int cx = centerChunkX - loadRadius; cx <= centerChunkX + loadRadius; ++cx) {
      (void)ensureChunk(cx, cz);
    }
  }

  for (auto it = chunks.begin(); it != chunks.end();) {
    int cx = it->second.cx;
    int cz = it->second.cz;
    int dist = std::max(std::abs(cx - centerChunkX), std::abs(cz - centerChunkZ));
    if (dist > unloadRadius) {
      if (it->second.modified) {
        savedChunks[it->first] = it->second.blocks;
      }
      it = chunks.erase(it);
      meshDirty = true;
    } else {
      ++it;
    }
  }
}

bool World::waitForChunkRegion(int centerChunkX, int centerChunkZ, int radius, int maxWaitMs) {
  using clock = std::chrono::steady_clock;
  auto deadline = clock::now() + std::chrono::milliseconds(std::max(1, maxWaitMs));

  while (clock::now() < deadline) {
    pumpChunkGeneration();

    bool ready = true;
    for (int cz = centerChunkZ - radius; cz <= centerChunkZ + radius && ready; ++cz) {
      for (int cx = centerChunkX - radius; cx <= centerChunkX + radius; ++cx) {
        Chunk* chunk = findChunk(cx, cz);
        if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
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
      if (!chunk || chunk->generating || chunk->generatedStatus < ChunkGenStatus::kNoise) {
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
  std::unordered_set<uint64_t> touched;
  touched.reserve(static_cast<size_t>(maxUpdates * 16));

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
    for (const auto& [ox, oz] : kSideOffsets) {
      uint8_t neighbor = getLoadedBlock(x + ox, y, z + oz);
      if (!isWaterBlock(neighbor)) {
        continue;
      }
      int level = static_cast<int>(waterLevelFromBlock(neighbor));
      if (level == 0) {
        bestLevel = std::min(bestLevel, 1);
      } else if (level < 7) {
        bestLevel = std::min(bestLevel, level + 1);
      }
    }

    if (bestLevel <= 7) {
      return blockFromWaterLevel(static_cast<uint8_t>(bestLevel));
    }
    return kAir;
  };

  auto addCandidate = [&](int x, int y, int z) {
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

      for (int lz = lzStart; lz <= lzEnd; ++lz) {
        for (int lx = lxStart; lx <= lxEnd; ++lx) {
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

            if (updates.size() > static_cast<size_t>(maxUpdates * 16)) {
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
    chunk->dirty = true;
    chunk->modified = true;
    if (lx == 0 || lx == kChunkSize - 1 || lz == 0 || lz == kChunkSize - 1) {
      markNeighborChunksDirty(cx, cz);
    }
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
    out.lx = lx;
    out.lz = lz;
    return true;
  };

  auto markChunkEdited = [&](const CellRef& cell) {
    cell.chunk->dirty = true;
    cell.chunk->modified = true;
    if (cell.lx == 0 || cell.lx == kChunkSize - 1 ||
        cell.lz == 0 || cell.lz == kChunkSize - 1) {
      markNeighborChunksDirty(cell.cx, cell.cz);
    }
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

  auto markChunkDirty = [&](const glm::ivec3& pos) {
    int cx = floorDiv(pos.x, kChunkSize);
    int cz = floorDiv(pos.z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    if (chunk) {
      chunk->dirty = true;
    }
    meshDirty = true;
  };

  if (breakOverlay.active) {
    markChunkDirty(breakOverlay.block);
  }

  breakOverlay.active = true;
  breakOverlay.block = block;
  breakOverlay.stage = clampedStage;
  markChunkDirty(block);
}

void World::clearBreakOverlay() {
  if (!breakOverlay.active) {
    return;
  }
  glm::ivec3 prevBlock = breakOverlay.block;
  breakOverlay.active = false;
  breakOverlay.stage = 0;

  int cx = floorDiv(prevBlock.x, kChunkSize);
  int cz = floorDiv(prevBlock.z, kChunkSize);
  Chunk* chunk = findChunk(cx, cz);
  if (chunk) {
    chunk->dirty = true;
  }
  meshDirty = true;
}

void World::buildChunkMesh(World::Chunk& chunk) {
  chunk.meshVertices.clear();
  chunk.meshIndices.clear();

  auto isFaceVisible = [](uint8_t current, uint8_t neighbor) {
    if (isUnderwaterPlantBlock(current)) {
      return false;
    }
    if (isWaterBlock(current)) {
      return neighbor == kAir || isUnderwaterPlantBlock(neighbor);
    }
    return neighbor == kAir || isWaterBlock(neighbor) || isUnderwaterPlantBlock(neighbor);
  };

  auto tileFor = [](uint8_t type, int axis, bool positive) {
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
    if (type == kWood) {
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
    return kTileStone;
  };

  auto uvForTile = [](int tile, float u, float v) {
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
  };

  auto addQuad = [&](const glm::vec3& v0,
                     const glm::vec3& v1,
                     const glm::vec3& v2,
                     const glm::vec3& v3,
                     const glm::vec3& color,
                     int tile) {
    glm::vec2 uv0 = uvForTile(tile, 0.0f, 0.0f);
    glm::vec2 uv1 = uvForTile(tile, 1.0f, 0.0f);
    glm::vec2 uv2 = uvForTile(tile, 1.0f, 1.0f);
    glm::vec2 uv3 = uvForTile(tile, 0.0f, 1.0f);

    uint32_t startIndex = static_cast<uint32_t>(chunk.meshVertices.size());
    chunk.meshVertices.push_back({v0, color, uv0});
    chunk.meshVertices.push_back({v1, color, uv1});
    chunk.meshVertices.push_back({v2, color, uv2});
    chunk.meshVertices.push_back({v3, color, uv3});

    chunk.meshIndices.push_back(startIndex + 0);
    chunk.meshIndices.push_back(startIndex + 1);
    chunk.meshIndices.push_back(startIndex + 2);
    chunk.meshIndices.push_back(startIndex + 0);
    chunk.meshIndices.push_back(startIndex + 2);
    chunk.meshIndices.push_back(startIndex + 3);
  };

  const float overlayOffset = 0.01f;
  bool overlayActive = breakOverlay.active && breakOverlay.stage > 0;
  int overlayTile = kBreakTileBase + std::clamp(breakOverlay.stage, 1, kBreakStages) - 1;
  int overlayX = breakOverlay.block.x;
  int overlayY = breakOverlay.block.y;
  int overlayZ = breakOverlay.block.z;

  int baseX = chunk.cx * kChunkSize;
  int baseZ = chunk.cz * kChunkSize;

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      for (int y = 0; y < kChunkHeight; ++y) {
        uint8_t blockType = chunk.blocks[static_cast<size_t>(localIndex(lx, y, lz))];
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
                             overlayX == x && overlayY == y && overlayZ == z;

        if (isUnderwaterPlantBlock(blockType)) {
          int tile = tileFor(blockType, 1, true);
          glm::vec3 color = blockColor(blockType) * (0.84f + 0.16f * heightFactor);
          float inset = 0.16f;
          addQuad({fx + inset, fy, fz + inset},
                  {fx + inset, fy1, fz + inset},
                  {fx1 - inset, fy1, fz1 - inset},
                  {fx1 - inset, fy, fz1 - inset},
                  color,
                  tile);
          addQuad({fx1 - inset, fy, fz + inset},
                  {fx1 - inset, fy1, fz + inset},
                  {fx + inset, fy1, fz1 - inset},
                  {fx + inset, fy, fz1 - inset},
                  color,
                  tile);
          continue;
        }

        if (isFaceVisible(blockType, getBlock(x + 1, y, z))) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, true);
          addQuad({fx1, fy, fz}, {fx1, fy1, fz}, {fx1, fy1, fz1}, {fx1, fy, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(overlayOffset, 0.0f, 0.0f);
            addQuad(glm::vec3(fx1, fy, fz) + offset,
                    glm::vec3(fx1, fy1, fz) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (isFaceVisible(blockType, getBlock(x - 1, y, z))) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, false);
          addQuad({fx, fy, fz}, {fx, fy, fz1}, {fx, fy1, fz1}, {fx, fy1, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(-overlayOffset, 0.0f, 0.0f);
            addQuad(glm::vec3(fx, fy, fz) + offset,
                    glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(fx, fy1, fz) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (isFaceVisible(blockType, getBlock(x, y + 1, z))) {
          float shade = 1.0f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, true);
          addQuad({fx, fy1, fz}, {fx, fy1, fz1}, {fx1, fy1, fz1}, {fx1, fy1, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, overlayOffset, 0.0f);
            addQuad(glm::vec3(fx, fy1, fz) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx1, fy1, fz) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (isFaceVisible(blockType, getBlock(x, y - 1, z))) {
          float shade = 0.5f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, false);
          addQuad({fx, fy, fz}, {fx1, fy, fz}, {fx1, fy, fz1}, {fx, fy, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, -overlayOffset, 0.0f);
            addQuad(glm::vec3(fx, fy, fz) + offset,
                    glm::vec3(fx1, fy, fz) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (isFaceVisible(blockType, getBlock(x, y, z + 1))) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, true);
          addQuad({fx, fy, fz1}, {fx1, fy, fz1}, {fx1, fy1, fz1}, {fx, fy1, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, overlayOffset);
            addQuad(glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (isFaceVisible(blockType, getBlock(x, y, z - 1))) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, false);
          addQuad({fx, fy, fz}, {fx, fy1, fz}, {fx1, fy1, fz}, {fx1, fy, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, -overlayOffset);
            addQuad(glm::vec3(fx, fy, fz) + offset,
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

  chunk.dirty = false;
}

void World::buildMesh(std::vector<Vertex>& outVertices,
                      std::vector<uint32_t>& outIndices) {
  pumpChunkGeneration();

  outVertices.clear();
  outIndices.clear();

  for (auto& entry : chunks) {
    Chunk& chunk = entry.second;
    if (chunk.generating || chunk.generatedStatus < ChunkGenStatus::kNoise) {
      continue;
    }
    if (chunk.dirty) {
      buildChunkMesh(chunk);
    }

    uint32_t vertexOffset = static_cast<uint32_t>(outVertices.size());
    outVertices.insert(outVertices.end(),
                       chunk.meshVertices.begin(),
                       chunk.meshVertices.end());

    outIndices.reserve(outIndices.size() + chunk.meshIndices.size());
    for (uint32_t idx : chunk.meshIndices) {
      outIndices.push_back(idx + vertexOffset);
    }
  }
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
  uint32_t version = 9;
  uint32_t cs = static_cast<uint32_t>(kChunkSize);
  uint32_t ch = static_cast<uint32_t>(kChunkHeight);
  uint32_t seedValue = static_cast<uint32_t>(seed);
  uint8_t presetValue = static_cast<uint8_t>(genSettings.preset);
  uint8_t structuresValue = genSettings.generateStructures ? 1u : 0u;
  float caveDensityValue = genSettings.caveDensity;
  float ravineFrequencyValue = genSettings.ravineFrequency;
  uint8_t startInventoryModeValue = genSettings.startInventoryMode;
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

  if (std::strncmp(magic, "CUB2", 4) != 0 || (version != 7 && version != 8 && version != 9) ||
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
  if (presetValue > static_cast<uint8_t>(WorldPreset::kClassicFlat)) {
    presetValue = static_cast<uint8_t>(WorldPreset::kMinecraftStyle);
  }
  WorldGenSettings loadedSettings{};
  loadedSettings.preset = static_cast<WorldPreset>(presetValue);
  loadedSettings.generateStructures = structuresValue != 0;
  loadedSettings.caveDensity = caveDensityValue;
  loadedSettings.ravineFrequency = ravineFrequencyValue;
  loadedSettings.startInventoryMode = startInventoryModeValue;
  loadedSettings.minY = static_cast<int>(minYValue);
  loadedSettings.maxY = static_cast<int>(maxYValue);
  setGenerationSettings(loadedSettings);
  resetChunkGeneration();
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
