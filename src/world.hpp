#pragma once

#include "mesh.hpp"

#include <cstdint>
#include <vector>

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 64;

enum BlockType : uint8_t {
  kAir = 0,
  kGrass = 1,
  kDirt = 2,
  kStone = 3
};

class World {
public:
  World(int chunksX, int chunksZ);

  void generate();
  void buildMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const;

  bool inBounds(int x, int y, int z) const;
  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t type);

  int width() const { return worldWidth; }
  int height() const { return kChunkHeight; }
  int depth() const { return worldDepth; }
  int chunkCountX() const { return chunksX; }
  int chunkCountZ() const { return chunksZ; }

private:
  int index(int x, int y, int z) const;
  int chunkIndex(int cx, int cz) const;
  int localIndex(int lx, int ly, int lz) const;
  glm::vec3 blockColor(uint8_t type) const;

  struct Chunk {
    std::vector<uint8_t> blocks;
    bool dirty = true;
  };

  int chunksX;
  int chunksZ;
  int worldWidth;
  int worldDepth;
  std::vector<Chunk> chunks;
};
