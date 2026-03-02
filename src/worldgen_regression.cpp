#include "world.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kSeaLevel = 32;
constexpr int kLoadRadiusChunks = 2;
constexpr int kFingerprintRadiusChunks = 1;
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t fnvMixU32(uint64_t hash, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    uint8_t b = static_cast<uint8_t>((value >> (i * 8)) & 0xFFu);
    hash ^= static_cast<uint64_t>(b);
    hash *= kFnvPrime;
  }
  return hash;
}

uint64_t fnvMixU8(uint64_t hash, uint8_t value) {
  hash ^= static_cast<uint64_t>(value);
  hash *= kFnvPrime;
  return hash;
}

struct SeedMetrics {
  double landRatio = 0.0;
  double lowlandRatio = 0.0;
  double meanAquifer = 0.0;
  double meanContinentalness = 0.0;
  int biomeCount = 0;
  int maxSurfaceY = 0;
  int minSurfaceY = 0;
};

bool ensureFullWindow(World& world, int radiusChunks) {
  world.updateActiveChunks(0, 0, radiusChunks);

  for (int pass = 0; pass < 2; ++pass) {
    bool allFull = true;
    for (int cz = -radiusChunks; cz <= radiusChunks; ++cz) {
      for (int cx = -radiusChunks; cx <= radiusChunks; ++cx) {
        if (world.getChunkGenerationStatus(cx, cz) < ChunkGenStatus::kFull) {
          world.generateChunkToStatus(cx, cz, ChunkGenStatus::kFull);
        }
      }
    }

    for (int cz = -radiusChunks; cz <= radiusChunks; ++cz) {
      for (int cx = -radiusChunks; cx <= radiusChunks; ++cx) {
        if (world.getChunkGenerationStatus(cx, cz) < ChunkGenStatus::kFull) {
          allFull = false;
          break;
        }
      }
      if (!allFull) {
        break;
      }
    }

    if (allFull) {
      return true;
    }
    world.updateActiveChunks(0, 0, radiusChunks);
  }

  return false;
}

uint64_t fingerprintWindow(World& world, int radiusChunks) {
  uint64_t hash = kFnvOffset;
  for (int cz = -radiusChunks; cz <= radiusChunks; ++cz) {
    for (int cx = -radiusChunks; cx <= radiusChunks; ++cx) {
      int baseX = cx * kChunkSize;
      int baseZ = cz * kChunkSize;
      hash = fnvMixU32(hash, static_cast<uint32_t>(cx));
      hash = fnvMixU32(hash, static_cast<uint32_t>(cz));
      for (int y = 0; y < kChunkHeight; y += 4) {
        for (int lz = 0; lz < kChunkSize; lz += 4) {
          for (int lx = 0; lx < kChunkSize; lx += 4) {
            uint8_t t = world.getBlock(baseX + lx, y, baseZ + lz);
            hash = fnvMixU8(hash, t);
          }
        }
      }
    }
  }
  return hash;
}

SeedMetrics analyzeSeed(World& world) {
  SeedMetrics out{};
  out.maxSurfaceY = -100000;
  out.minSurfaceY = 100000;

  std::array<bool, 256> biomeSeen{};
  biomeSeen.fill(false);

  int sampleCount = 0;
  int landCount = 0;
  int lowlandCount = 0;
  double aquiferSum = 0.0;
  double continentalnessSum = 0.0;

  constexpr std::array<std::pair<int, int>, 6> kMetricCenters = {{
    {0, 0},
    {2200, 0},
    {-2200, 0},
    {0, 2200},
    {0, -2200},
    {1800, 1800}
  }};
  constexpr int kMetricHalfSpan = 192;
  constexpr int kMetricStep = 48;

  for (const auto& center : kMetricCenters) {
    for (int dz = -kMetricHalfSpan; dz <= kMetricHalfSpan; dz += kMetricStep) {
      for (int dx = -kMetricHalfSpan; dx <= kMetricHalfSpan; dx += kMetricStep) {
        int x = center.first + dx;
        int z = center.second + dz;
        ++sampleCount;

        uint8_t biome = 0;
        BiomeClimateSample climate{};
        world.sampleBiomeClimateAt(x, z, biome, climate);
        biomeSeen[biome] = true;

        int surfaceY = world.sampleSurfaceHeightAt(x, z);
        out.maxSurfaceY = std::max(out.maxSurfaceY, surfaceY);
        out.minSurfaceY = std::min(out.minSurfaceY, surfaceY);

        bool dryLand = surfaceY > kSeaLevel + 1;
        if (dryLand) {
          ++landCount;
        }
        if (surfaceY <= kSeaLevel + 1) {
          ++lowlandCount;
        }

        aquiferSum += static_cast<double>(world.sampleAquiferLevelAt(x, z));
        continentalnessSum += static_cast<double>(climate.continentalness);
      }
    }
  }

  int biomeCount = 0;
  for (bool seen : biomeSeen) {
    if (seen) {
      ++biomeCount;
    }
  }

  double invSamples = sampleCount > 0 ? (1.0 / static_cast<double>(sampleCount)) : 0.0;
  out.landRatio = static_cast<double>(landCount) * invSamples;
  out.lowlandRatio = static_cast<double>(lowlandCount) * invSamples;
  out.meanAquifer = aquiferSum * invSamples;
  out.meanContinentalness = continentalnessSum * invSamples;
  out.biomeCount = biomeCount;
  return out;
}

bool checkSeed(int seed, std::string& errorLine) {
  WorldGenSettings settings{};
  settings.preset = WorldPreset::kMinecraftStyle;
  settings.generateStructures = true;
  settings.caveDensity = 1.0f;
  settings.ravineFrequency = 1.0f;
  settings.minY = 0;
  settings.maxY = kChunkHeight - 1;

  World worldA(3, 3, seed);
  worldA.setGenerationSettings(settings);
  worldA.setSeed(seed);
  worldA.generate();
  if (!ensureFullWindow(worldA, kLoadRadiusChunks)) {
    errorLine = "seed " + std::to_string(seed) + " failed to fully load chunk window";
    return false;
  }

  SeedMetrics metrics = analyzeSeed(worldA);
  uint64_t hashA = fingerprintWindow(worldA, kFingerprintRadiusChunks);

  World worldB(3, 3, seed);
  worldB.setGenerationSettings(settings);
  worldB.setSeed(seed);
  worldB.generate();
  if (!ensureFullWindow(worldB, kLoadRadiusChunks)) {
    errorLine = "seed " + std::to_string(seed) + " failed to reload chunk window";
    return false;
  }
  uint64_t hashB = fingerprintWindow(worldB, kFingerprintRadiusChunks);

  std::cout << "seed " << seed
            << " land=" << std::fixed << std::setprecision(3) << metrics.landRatio
            << " lowland=" << metrics.lowlandRatio
            << " biomes=" << metrics.biomeCount
            << " surf[min,max]=" << metrics.minSurfaceY << "," << metrics.maxSurfaceY
            << " aquiferAvg=" << metrics.meanAquifer
            << " contAvg=" << metrics.meanContinentalness
            << " hash=" << std::hex << hashA << std::dec << "\n";

  if (hashA != hashB) {
    errorLine = "seed " + std::to_string(seed) + " is not deterministic across full-window fingerprint";
    return false;
  }
  if (metrics.landRatio < 0.24) {
    errorLine = "seed " + std::to_string(seed) + " has too little land";
    return false;
  }
  if (metrics.lowlandRatio > 0.95) {
    errorLine = "seed " + std::to_string(seed) + " has excessive lowland/ocean coverage";
    return false;
  }
  if (metrics.biomeCount < 3) {
    errorLine = "seed " + std::to_string(seed) + " has insufficient biome variety";
    return false;
  }
  if (metrics.maxSurfaceY < kSeaLevel + 6) {
    errorLine = "seed " + std::to_string(seed) + " has too little terrain relief";
    return false;
  }
  if (metrics.meanAquifer > static_cast<double>(kSeaLevel + 10)) {
    errorLine = "seed " + std::to_string(seed) + " aquifer average is too high";
    return false;
  }
  if (metrics.meanContinentalness < 0.18 || metrics.meanContinentalness > 0.86) {
    errorLine = "seed " + std::to_string(seed) + " continentalness average out of bounds";
    return false;
  }

  return true;
}

} // namespace

int main() {
  std::vector<int> seeds = {
    0,
    1337,
    2024
  };

  bool ok = true;
  for (int seed : seeds) {
    std::string errorLine;
    if (!checkSeed(seed, errorLine)) {
      std::cerr << "worldgen regression failure: " << errorLine << "\n";
      ok = false;
    }
  }

  if (!ok) {
    std::cerr << "worldgen regression suite failed\n";
    return 1;
  }

  std::cout << "worldgen regression suite passed\n";
  return 0;
}
