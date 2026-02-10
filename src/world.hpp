#pragma once

#include "mesh.hpp"

#include <condition_variable>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 128;
constexpr size_t kChunkColumnCount = static_cast<size_t>(kChunkSize * kChunkSize);

enum class WorldPreset : uint8_t {
  kMinecraftStyle = 0,
  kClassicFlat = 1
};

enum class ChunkGenStatus : uint8_t {
  kEmpty = 0,
  kStructureStarts = 1,
  kStructureReferences = 2,
  kBiomes = 3,
  kNoise = 4,
  kSurface = 5,
  kCarvers = 6,
  kLiquidCarvers = 7,
  kFeatures = 8,
  kLight = 9,
  kSpawn = 10,
  kHeightmaps = 11,
  kFull = 12
};

struct BiomeClimateSample {
  float temperature = 0.5f;
  float humidity = 0.5f;
  float continentalness = 0.5f;
  float erosion = 0.5f;
  float depth = 0.5f;
  float weirdness = 0.5f;
};

struct WorldGenSettings {
  WorldPreset preset = WorldPreset::kMinecraftStyle;
  bool generateStructures = true;
  float caveDensity = 1.0f;
  float ravineFrequency = 1.0f;
  uint8_t startInventoryMode = 0; // 0=Empty, 1=CreativeTest (reserved for v0.2.1+)
  int minY = 0;
  int maxY = kChunkHeight - 1;
};

enum BlockType : uint8_t {
  kAir = 0,
  kGrass = 1,
  kDirt = 2,
  kStone = 3,
  kSand = 4,
  kGravel = 5,
  kWood = 6,
  kLeaves = 7,
  kWater = 8,
  kCoalOre = 9,
  kIronOre = 10,
  kGoldOre = 11,
  kWaterFlow1 = 12,
  kWaterFlow2 = 13,
  kWaterFlow3 = 14,
  kWaterFlow4 = 15,
  kWaterFlow5 = 16,
  kWaterFlow6 = 17,
  kWaterFlow7 = 18
};

constexpr bool isWaterBlock(uint8_t type) {
  return type == kWater || (type >= kWaterFlow1 && type <= kWaterFlow7);
}

constexpr uint8_t waterLevelFromBlock(uint8_t type) {
  if (type == kWater) {
    return 0;
  }
  if (type >= kWaterFlow1 && type <= kWaterFlow7) {
    return static_cast<uint8_t>(1 + (type - kWaterFlow1));
  }
  return 255;
}

constexpr uint8_t blockFromWaterLevel(uint8_t level) {
  if (level == 0) {
    return kWater;
  }
  if (level >= 1 && level <= 7) {
    return static_cast<uint8_t>(kWaterFlow1 + (level - 1));
  }
  return kAir;
}

class World {
public:
  World(int initialChunksX, int initialChunksZ, int seed = 1337);
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  void generate();
  void buildMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices);
  bool save(const std::string& path) const;
  bool load(const std::string& path);
  void generateChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus);
  ChunkGenStatus getChunkGenerationStatus(int chunkX, int chunkZ) const;
  void updateActiveChunks(int centerChunkX, int centerChunkZ, int radius);
  bool waitForChunkRegion(int centerChunkX, int centerChunkZ, int radius, int maxWaitMs);
  void simulateWater(int centerX, int centerZ, int radiusXZ, int maxUpdates);
  void simulateFallingBlocks(int centerX, int centerZ, int radiusXZ, int maxUpdates);
  bool consumeMeshDirty();
  void setBreakOverlay(const glm::ivec3& block, int stage);
  void clearBreakOverlay();

  bool inBounds(int x, int y, int z) const;
  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t type);
  void setSeed(int newSeed) { seed = newSeed; }
  int getSeed() const { return seed; }
  void setGenerationSettings(const WorldGenSettings& settings);
  const WorldGenSettings& getGenerationSettings() const { return genSettings; }

  int height() const { return kChunkHeight; }
  int chunkCount() const { return static_cast<int>(chunks.size()); }

private:
  struct Chunk;
  int localIndex(int lx, int ly, int lz) const;
  glm::vec3 blockColor(uint8_t type) const;
  uint64_t chunkKey(int cx, int cz) const;
  Chunk* findChunk(int cx, int cz);
  const Chunk* findChunk(int cx, int cz) const;
  Chunk& ensureChunk(int cx, int cz);
  void generateChunk(Chunk& chunk, ChunkGenStatus targetStatus = ChunkGenStatus::kFull);
  void buildChunkMesh(Chunk& chunk);
  void markNeighborChunksDirty(int cx, int cz);
  void startChunkWorkers();
  void stopChunkWorkers();
  void queueChunkGeneration(int cx, int cz, uint32_t epoch, ChunkGenStatus targetStatus);
  void resetChunkGeneration();
  void pumpChunkGeneration();

  struct ChunkGenerationTask {
    int cx = 0;
    int cz = 0;
    int seed = 1337;
    WorldGenSettings settings{};
    uint32_t epoch = 0;
    uint64_t key = 0;
    ChunkGenStatus targetStatus = ChunkGenStatus::kFull;
  };

  struct ChunkGenerationResult {
    uint64_t key = 0;
    uint32_t epoch = 0;
    std::vector<uint8_t> blocks;
    std::array<uint8_t, kChunkColumnCount> biomeMap{};
    std::array<BiomeClimateSample, kChunkColumnCount> climateMap{};
    ChunkGenStatus status = ChunkGenStatus::kEmpty;
  };

  struct BreakOverlay {
    bool active = false;
    glm::ivec3 block{};
    int stage = 0;
  };

  struct Chunk {
    int cx = 0;
    int cz = 0;
    std::vector<uint8_t> blocks;
    std::vector<Vertex> meshVertices;
    std::vector<uint32_t> meshIndices;
    bool dirty = true;
    bool modified = false;
    bool generating = false;
    uint32_t generationEpoch = 0;
    std::array<uint8_t, kChunkColumnCount> biomeMap{};
    std::array<BiomeClimateSample, kChunkColumnCount> climateMap{};
    ChunkGenStatus generatedStatus = ChunkGenStatus::kEmpty;
  };

  int initialChunksX;
  int initialChunksZ;
  int initialRadius = 1;
  int seed = 1337;
  WorldGenSettings genSettings{};
  bool meshDirty = true;
  BreakOverlay breakOverlay{};
  std::unordered_map<uint64_t, Chunk> chunks;
  std::unordered_map<uint64_t, std::vector<uint8_t>> savedChunks;
  uint32_t generationEpoch = 1;
  std::unordered_map<uint64_t, uint32_t> pendingGenerationEpochByKey;
  std::queue<ChunkGenerationTask> generationQueue;
  std::queue<ChunkGenerationResult> generationResults;
  std::vector<std::thread> generationWorkers;
  bool stopGenerationWorkers = false;
  mutable std::mutex generationMutex;
  std::condition_variable generationCv;
};
