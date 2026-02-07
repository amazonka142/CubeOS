#pragma once

#include "mesh.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 128;

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
  kGoldOre = 11
};

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
  void updateActiveChunks(int centerChunkX, int centerChunkZ, int radius);
  bool waitForChunkRegion(int centerChunkX, int centerChunkZ, int radius, int maxWaitMs);
  void simulateWater(int centerX, int centerZ, int radiusXZ, int maxUpdates);
  bool consumeMeshDirty();
  void setBreakOverlay(const glm::ivec3& block, int stage);
  void clearBreakOverlay();

  bool inBounds(int x, int y, int z) const;
  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t type);

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
  void generateChunk(Chunk& chunk);
  void buildChunkMesh(Chunk& chunk);
  void markNeighborChunksDirty(int cx, int cz);
  void startChunkWorkers();
  void stopChunkWorkers();
  void queueChunkGeneration(int cx, int cz, uint32_t epoch);
  void resetChunkGeneration();
  void pumpChunkGeneration();

  struct ChunkGenerationTask {
    int cx = 0;
    int cz = 0;
    int seed = 1337;
    uint32_t epoch = 0;
    uint64_t key = 0;
  };

  struct ChunkGenerationResult {
    uint64_t key = 0;
    uint32_t epoch = 0;
    std::vector<uint8_t> blocks;
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
  };

  int initialChunksX;
  int initialChunksZ;
  int initialRadius = 1;
  int seed = 1337;
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
