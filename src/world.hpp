#pragma once

#include "mesh.hpp"

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 128;

enum BlockType : uint8_t {
  kAir = 0,
  kGrass = 1,
  kDirt = 2,
  kStone = 3
};

class World {
public:
  World(int initialChunksX, int initialChunksZ, int seed = 1337);

  void generate();
  void buildMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const;
  bool save(const std::string& path) const;
  bool load(const std::string& path);
  void updateActiveChunks(int centerChunkX, int centerChunkZ, int radius);
  bool consumeMeshDirty();
  void setBreakOverlay(const glm::ivec3& block, int stage);
  void clearBreakOverlay();

  bool inBounds(int x, int y, int z) const;
  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t type);

  int height() const { return kChunkHeight; }
  int chunkCount() const { return static_cast<int>(chunks.size()); }

private:
  int localIndex(int lx, int ly, int lz) const;
  glm::vec3 blockColor(uint8_t type) const;
  uint64_t chunkKey(int cx, int cz) const;
  struct Chunk* findChunk(int cx, int cz);
  const struct Chunk* findChunk(int cx, int cz) const;
  struct Chunk& ensureChunk(int cx, int cz);
  void generateChunk(struct Chunk& chunk);

  struct BreakOverlay {
    bool active = false;
    glm::ivec3 block{};
    int stage = 0;
  };

  struct Chunk {
    int cx = 0;
    int cz = 0;
    std::vector<uint8_t> blocks;
    bool dirty = true;
    bool modified = false;
  };

  int initialChunksX;
  int initialChunksZ;
  int initialRadius = 1;
  int seed = 1337;
  bool meshDirty = true;
  BreakOverlay breakOverlay{};
  std::unordered_map<uint64_t, Chunk> chunks;
  std::unordered_map<uint64_t, std::vector<uint8_t>> savedChunks;
};
