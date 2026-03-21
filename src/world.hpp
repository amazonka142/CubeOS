#pragma once

#include "mesh.hpp"

#include <condition_variable>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 128;
constexpr size_t kChunkColumnCount = static_cast<size_t>(kChunkSize * kChunkSize);
constexpr int kChunkSectionSize = 16;
constexpr int kChunkSectionCount = kChunkHeight / kChunkSectionSize;
constexpr int kSectionSampleSize = kChunkSectionSize + 2;
constexpr size_t kSectionSampleCount =
  static_cast<size_t>(kSectionSampleSize * kSectionSampleSize * kSectionSampleSize);
static_assert(kChunkHeight % kChunkSectionSize == 0,
              "kChunkHeight must be divisible by kChunkSectionSize");

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
  kWaterFlow7 = 18,
  kSeagrass = 19,
  kCoral = 20,
  kDiamondOre = 21,
  kWorkbench = 22,
  kPlanks = 23,
  kStick = 24,
  kIronIngot = 25,
  kDiamond = 26,
  kWoodPickaxe = 27,
  kStonePickaxe = 28,
  kIronPickaxe = 29,
  kTorch = 30,
  kFurnace = 31,
  kLootCache = 32,
  kWorkbenchNorth = 33,
  kWorkbenchEast = 34,
  kWorkbenchSouth = 35,
  kWorkbenchWest = 36,
  kFurnaceNorth = 37,
  kFurnaceEast = 38,
  kFurnaceSouth = 39,
  kFurnaceWest = 40,
  kTorchNorth = 41,
  kTorchEast = 42,
  kTorchSouth = 43,
  kTorchWest = 44
};

enum HorizontalFacing : uint8_t {
  kFacingNorth = 0,
  kFacingEast = 1,
  kFacingSouth = 2,
  kFacingWest = 3
};

constexpr bool isWorkbenchBlock(uint8_t type) {
  return type == kWorkbench ||
         type == kWorkbenchNorth ||
         type == kWorkbenchEast ||
         type == kWorkbenchSouth ||
         type == kWorkbenchWest;
}

constexpr bool isFurnaceBlock(uint8_t type) {
  return type == kFurnace ||
         type == kFurnaceNorth ||
         type == kFurnaceEast ||
         type == kFurnaceSouth ||
         type == kFurnaceWest;
}

constexpr bool isTorchBlock(uint8_t type) {
  return type == kTorch ||
         type == kTorchNorth ||
         type == kTorchEast ||
         type == kTorchSouth ||
         type == kTorchWest;
}

constexpr bool isWallTorchBlock(uint8_t type) {
  return type == kTorchNorth ||
         type == kTorchEast ||
         type == kTorchSouth ||
         type == kTorchWest;
}

constexpr uint8_t torchFacingIndex(uint8_t type) {
  switch (type) {
    case kTorchNorth:
      return kFacingNorth;
    case kTorchEast:
      return kFacingEast;
    case kTorchWest:
      return kFacingWest;
    case kTorchSouth:
    default:
      return kFacingSouth;
  }
}

constexpr uint8_t torchBlockForFacing(uint8_t facing) {
  switch (facing) {
    case kFacingNorth:
      return kTorchNorth;
    case kFacingEast:
      return kTorchEast;
    case kFacingWest:
      return kTorchWest;
    case kFacingSouth:
    default:
      return kTorchSouth;
  }
}

inline glm::ivec3 torchFacingVector(uint8_t type) {
  switch (torchFacingIndex(type)) {
    case kFacingNorth:
      return {0, 0, -1};
    case kFacingEast:
      return {1, 0, 0};
    case kFacingWest:
      return {-1, 0, 0};
    case kFacingSouth:
    default:
      return {0, 0, 1};
  }
}

inline glm::ivec3 torchSupportOffset(uint8_t type) {
  if (!isWallTorchBlock(type)) {
    return {0, -1, 0};
  }
  glm::ivec3 facing = torchFacingVector(type);
  return {-facing.x, 0, -facing.z};
}

inline glm::vec3 torchBaseOffset(uint8_t type) {
  if (!isWallTorchBlock(type)) {
    return {0.5f, 0.02f, 0.5f};
  }
  glm::ivec3 facing = torchFacingVector(type);
  glm::vec3 forward(static_cast<float>(facing.x), 0.0f, static_cast<float>(facing.z));
  return {0.5f - forward.x * 0.18f, 0.22f, 0.5f - forward.z * 0.18f};
}

inline glm::vec3 torchTopOffset(uint8_t type) {
  if (!isWallTorchBlock(type)) {
    return {0.5f, 0.78f, 0.5f};
  }
  glm::ivec3 facing = torchFacingVector(type);
  glm::vec3 forward(static_cast<float>(facing.x), 0.0f, static_cast<float>(facing.z));
  return {0.5f + forward.x * 0.06f, 0.88f, 0.5f + forward.z * 0.06f};
}

inline glm::vec3 torchLightOffset(uint8_t type) {
  if (!isWallTorchBlock(type)) {
    return {0.5f, 0.78f, 0.5f};
  }
  glm::ivec3 facing = torchFacingVector(type);
  glm::vec3 forward(static_cast<float>(facing.x), 0.0f, static_cast<float>(facing.z));
  return {0.5f + forward.x * 0.10f, 0.84f, 0.5f + forward.z * 0.10f};
}

constexpr uint8_t blockFacingIndex(uint8_t type) {
  switch (type) {
    case kWorkbenchNorth:
    case kFurnaceNorth:
      return kFacingNorth;
    case kWorkbenchEast:
    case kFurnaceEast:
      return kFacingEast;
    case kWorkbenchWest:
    case kFurnaceWest:
      return kFacingWest;
    case kWorkbench:
    case kWorkbenchSouth:
    case kFurnace:
    case kFurnaceSouth:
    default:
      return kFacingSouth;
  }
}

constexpr bool faceMatchesFacing(uint8_t facing, int axis, bool positive) {
  return (facing == kFacingNorth && axis == 2 && !positive) ||
         (facing == kFacingEast && axis == 0 && positive) ||
         (facing == kFacingSouth && axis == 2 && positive) ||
         (facing == kFacingWest && axis == 0 && !positive);
}

constexpr uint8_t workbenchBlockForFacing(uint8_t facing) {
  switch (facing) {
    case kFacingNorth:
      return kWorkbenchNorth;
    case kFacingEast:
      return kWorkbenchEast;
    case kFacingWest:
      return kWorkbenchWest;
    case kFacingSouth:
    default:
      return kWorkbenchSouth;
  }
}

constexpr uint8_t furnaceBlockForFacing(uint8_t facing) {
  switch (facing) {
    case kFacingNorth:
      return kFurnaceNorth;
    case kFacingEast:
      return kFurnaceEast;
    case kFacingWest:
      return kFurnaceWest;
    case kFacingSouth:
    default:
      return kFurnaceSouth;
  }
}

constexpr uint8_t itemTypeForPlacedBlock(uint8_t type) {
  if (isWorkbenchBlock(type)) {
    return kWorkbench;
  }
  if (isFurnaceBlock(type)) {
    return kFurnace;
  }
  if (isTorchBlock(type)) {
    return kTorch;
  }
  return type;
}

constexpr bool isStructureRewardBlock(uint8_t type) {
  return type == kLootCache;
}

constexpr bool isWaterBlock(uint8_t type) {
  return type == kWater || (type >= kWaterFlow1 && type <= kWaterFlow7);
}

constexpr bool isBlockType(uint8_t type) {
  switch (type) {
    case kGrass:
    case kDirt:
    case kStone:
    case kSand:
    case kGravel:
    case kWood:
    case kLeaves:
    case kWater:
    case kCoalOre:
    case kIronOre:
    case kGoldOre:
    case kWaterFlow1:
    case kWaterFlow2:
    case kWaterFlow3:
    case kWaterFlow4:
    case kWaterFlow5:
    case kWaterFlow6:
    case kWaterFlow7:
    case kSeagrass:
    case kCoral:
    case kDiamondOre:
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
    case kPlanks:
    case kTorch:
    case kTorchNorth:
    case kTorchEast:
    case kTorchSouth:
    case kTorchWest:
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
    case kLootCache:
      return true;
    default:
      return false;
  }
}

constexpr bool isUnderwaterPlantBlock(uint8_t type) {
  return type == kSeagrass || type == kCoral;
}

constexpr bool isWaterVolumeBlock(uint8_t type) {
  return isWaterBlock(type) || isUnderwaterPlantBlock(type);
}

constexpr bool canSupportSeagrassBlock(uint8_t ground) {
  return ground == kGrass || ground == kDirt || ground == kSand || ground == kGravel;
}

constexpr bool canSupportCoralBlock(uint8_t ground) {
  return ground == kSand || ground == kGravel || ground == kStone || ground == kDirt;
}

constexpr bool canSupportUnderwaterPlant(uint8_t plantType, uint8_t ground) {
  if (plantType == kSeagrass) {
    return canSupportSeagrassBlock(ground);
  }
  if (plantType == kCoral) {
    return canSupportCoralBlock(ground);
  }
  return false;
}

constexpr bool isDecorationBlock(uint8_t type) {
  return isUnderwaterPlantBlock(type) || isTorchBlock(type);
}

constexpr bool isToolItem(uint8_t type) {
  return type >= kWoodPickaxe && type <= kIronPickaxe;
}

constexpr bool isPlaceableItem(uint8_t type) {
  return isBlockType(type) &&
         type != kAir &&
         !isStructureRewardBlock(type) &&
         !(type >= kWaterFlow1 && type <= kWaterFlow7);
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

struct ChunkMeshUpload {
  uint64_t key = 0;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};

class World {
public:
  struct RuntimeStats {
    size_t loadedChunks = 0;
    size_t generatingChunks = 0;
    size_t dirtySections = 0;
    size_t queuedSections = 0;
    size_t pendingSectionRebuilds = 0;
    size_t pendingMeshUploads = 0;
    size_t pendingRemovedMeshes = 0;
    size_t sectionWorkerQueue = 0;
    size_t sectionWorkerResults = 0;
    size_t generationQueue = 0;
    size_t generationResults = 0;
  };

  World(int initialChunksX, int initialChunksZ, int seed = 1337);
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  void generate();
  void buildMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices);
  void snapshotChunkMeshes(std::vector<ChunkMeshUpload>& outUploads);
  bool consumeChunkMeshUpdates(std::vector<ChunkMeshUpload>& outUploads,
                               std::vector<uint64_t>& outRemoved,
                               int buildBudget,
                               int uploadBudget);
  bool save(const std::string& path) const;
  bool load(const std::string& path);
  void requestChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus);
  void generateChunkToStatus(int chunkX, int chunkZ, ChunkGenStatus targetStatus);
  ChunkGenStatus getChunkGenerationStatus(int chunkX, int chunkZ) const;
  void updateActiveChunks(int centerChunkX, int centerChunkZ, int radius);
  bool waitForChunkRegion(int centerChunkX,
                          int centerChunkZ,
                          int radius,
                          int maxWaitMs,
                          ChunkGenStatus minStatus = ChunkGenStatus::kNoise);
  void simulateWater(int centerX, int centerZ, int radiusXZ, int maxUpdates);
  void simulateFallingBlocks(int centerX, int centerZ, int radiusXZ, int maxUpdates);
  bool consumeMeshDirty();
  void setBreakOverlay(const glm::ivec3& block, int stage);
  void clearBreakOverlay();

  bool inBounds(int x, int y, int z) const;
  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t type);
  bool sampleBiomeClimateAt(int x, int z, uint8_t& outBiome, BiomeClimateSample& outClimate) const;
  int sampleSurfaceHeightAt(int x, int z) const;
  float sampleDensityAt(int x, int y, int z) const;
  int sampleAquiferLevelAt(int x, int z) const;
  std::vector<glm::ivec3> collectTorchBlocks() const;
  void setSeed(int newSeed) { seed = newSeed; }
  int getSeed() const { return seed; }
  void setGenerationSettings(const WorldGenSettings& settings);
  const WorldGenSettings& getGenerationSettings() const { return genSettings; }
  RuntimeStats collectRuntimeStats() const;

  int height() const { return kChunkHeight; }
  int chunkCount() const { return static_cast<int>(chunks.size()); }

private:
  struct Chunk;
  int localIndex(int lx, int ly, int lz) const;
  glm::vec3 blockColor(uint8_t type) const;
  uint64_t chunkKey(int cx, int cz) const;
  uint64_t sectionKey(int cx, int cz, int sectionY) const;
  bool decodeSectionKey(uint64_t key, int& outCx, int& outCz, int& outSectionY) const;
  bool isChunkMeshReady(const Chunk& chunk) const;
  int sectionIndexForY(int y) const;
  Chunk* findChunk(int cx, int cz);
  const Chunk* findChunk(int cx, int cz) const;
  Chunk& ensureChunk(int cx, int cz, ChunkGenStatus targetStatus = ChunkGenStatus::kFull);
  void rebuildChunkTorchIndices(Chunk& chunk);
  void generateChunk(Chunk& chunk, ChunkGenStatus targetStatus = ChunkGenStatus::kFull);
  bool buildChunkSectionMeshNow(Chunk& chunk, int sectionY);
  void buildChunkMesh(Chunk& chunk);
  void markChunkSectionDirty(Chunk& chunk, int sectionY, bool queueRebuild);
  void markChunkSectionsDirty(Chunk& chunk, bool queueRebuild);
  void markSectionAndNeighborsDirty(int cx, int cz, int lx, int y, int lz, bool queueRebuild);
  void markNeighborChunksDirty(int cx, int cz);
  void markChunkBoundaryNeighborsDirty(int cx, int cz);
  void enqueueSectionRebuild(uint64_t key);
  bool queueSectionRebuildTask(Chunk& chunk, int sectionY);
  void enqueueChunkMeshUpload(uint64_t key);
  void pumpMeshRebuildResults();
  void resetMeshRebuildQueues();
  void startMeshWorkers();
  void stopMeshWorkers();
  void startChunkWorkers();
  void stopChunkWorkers();
  int generationTaskPriority(int cx, int cz, ChunkGenStatus targetStatus) const;
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
    int priority = 0;
    uint64_t sequence = 0;
  };

  struct ChunkGenerationTaskCompare {
    bool operator()(const ChunkGenerationTask& lhs, const ChunkGenerationTask& rhs) const {
      if (lhs.priority != rhs.priority) {
        return lhs.priority < rhs.priority;
      }
      return lhs.sequence < rhs.sequence;
    }
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

  struct SectionRenderData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool dirty = true;
    bool queued = false;
    uint32_t version = 1;
  };

  struct SectionRebuildTask {
    uint64_t sectionKey = 0;
    uint64_t chunkLookupKey = 0;
    int cx = 0;
    int cz = 0;
    int sectionY = 0;
    uint32_t version = 0;
    std::array<uint8_t, kSectionSampleCount> samples{};
    std::array<float, kSectionSampleCount> skyLightSamples{};
    bool overlayActive = false;
    glm::ivec3 overlayBlock{};
    int overlayStage = 0;
  };

  struct SectionRebuildResult {
    uint64_t sectionKey = 0;
    uint64_t chunkLookupKey = 0;
    int sectionY = 0;
    uint32_t version = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
  };

  struct Chunk {
    int cx = 0;
    int cz = 0;
    std::vector<uint8_t> blocks;
    std::vector<uint16_t> torchLocalIndices;
    std::array<SectionRenderData, kChunkSectionCount> sectionMeshes{};
    bool dirty = true;
    bool modified = false;
    bool generating = false;
    uint32_t generationEpoch = 0;
    ChunkGenStatus generationTargetStatus = ChunkGenStatus::kEmpty;
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
  std::vector<uint64_t> pendingRemovedMeshKeys;
  std::deque<uint64_t> pendingSectionRebuildQueue;
  std::unordered_set<uint64_t> pendingSectionRebuildSet;
  std::deque<uint64_t> pendingMeshUploadQueue;
  std::unordered_set<uint64_t> pendingMeshUploadSet;
  std::queue<SectionRebuildTask> sectionRebuildQueue;
  std::queue<SectionRebuildResult> sectionRebuildResults;
  std::vector<std::thread> sectionRebuildWorkers;
  bool stopSectionRebuildWorkers = false;
  mutable std::mutex sectionRebuildMutex;
  std::condition_variable sectionRebuildCv;
  uint32_t generationEpoch = 1;
  int generationFocusChunkX = 0;
  int generationFocusChunkZ = 0;
  uint64_t generationTaskSequence = 0;
  std::unordered_map<uint64_t, uint32_t> pendingGenerationEpochByKey;
  std::priority_queue<ChunkGenerationTask,
                      std::vector<ChunkGenerationTask>,
                      ChunkGenerationTaskCompare> generationQueue;
  std::queue<ChunkGenerationResult> generationResults;
  std::vector<std::thread> generationWorkers;
  bool stopGenerationWorkers = false;
  mutable std::mutex generationMutex;
  std::condition_variable generationCv;
};
