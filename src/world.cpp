#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

World::World(int chunksXIn, int chunksZIn)
    : chunksX(chunksXIn),
      chunksZ(chunksZIn),
      worldWidth(chunksXIn * kChunkSize),
      worldDepth(chunksZIn * kChunkSize),
      chunks(static_cast<size_t>(chunksXIn * chunksZIn)) {
  for (auto& chunk : chunks) {
    chunk.blocks.resize(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
    chunk.dirty = true;
  }
}

int World::index(int x, int y, int z) const {
  return x + z * worldWidth + y * worldWidth * worldDepth;
}

int World::chunkIndex(int cx, int cz) const {
  return cx + cz * chunksX;
}

int World::localIndex(int lx, int ly, int lz) const {
  return lx + lz * kChunkSize + ly * kChunkSize * kChunkSize;
}

bool World::inBounds(int x, int y, int z) const {
  return x >= 0 && x < worldWidth &&
         y >= 0 && y < kChunkHeight &&
         z >= 0 && z < worldDepth;
}

uint8_t World::getBlock(int x, int y, int z) const {
  if (!inBounds(x, y, z)) {
    return kAir;
  }
  int cx = x / kChunkSize;
  int cz = z / kChunkSize;
  int lx = x % kChunkSize;
  int lz = z % kChunkSize;
  const Chunk& chunk = chunks[static_cast<size_t>(chunkIndex(cx, cz))];
  return chunk.blocks[static_cast<size_t>(localIndex(lx, y, lz))];
}

void World::setBlock(int x, int y, int z, uint8_t type) {
  if (!inBounds(x, y, z)) {
    return;
  }
  int cx = x / kChunkSize;
  int cz = z / kChunkSize;
  int lx = x % kChunkSize;
  int lz = z % kChunkSize;
  Chunk& chunk = chunks[static_cast<size_t>(chunkIndex(cx, cz))];
  chunk.blocks[static_cast<size_t>(localIndex(lx, y, lz))] = type;
  chunk.dirty = true;
}

glm::vec3 World::blockColor(uint8_t type) const {
  switch (type) {
    case kGrass:
      return {0.2f, 0.8f, 0.2f};
    case kDirt:
      return {0.55f, 0.35f, 0.2f};
    case kStone:
      return {0.6f, 0.6f, 0.6f};
    default:
      return {1.0f, 1.0f, 1.0f};
  }
}

void World::generate() {
  for (auto& chunk : chunks) {
    std::fill(chunk.blocks.begin(), chunk.blocks.end(), kAir);
    chunk.dirty = true;
  }

  for (int z = 0; z < worldDepth; ++z) {
    for (int x = 0; x < worldWidth; ++x) {
      float fx = static_cast<float>(x);
      float fz = static_cast<float>(z);
      float heightNoise = std::sin(fx * 0.22f) * 6.0f + std::cos(fz * 0.18f) * 5.0f;
      int height = static_cast<int>(24.0f + heightNoise);
      if (height < 1) {
        height = 1;
      } else if (height > kChunkHeight - 1) {
        height = kChunkHeight - 1;
      }

      for (int y = 0; y < height; ++y) {
        uint8_t type = kStone;
        if (y == height - 1) {
          type = kGrass;
        } else if (y >= height - 4) {
          type = kDirt;
        }
        setBlock(x, y, z, type);
      }
    }
  }
}

void World::buildMesh(std::vector<Vertex>& outVertices,
                      std::vector<uint32_t>& outIndices) const {
  outVertices.clear();
  outIndices.clear();

  auto tileFor = [](uint8_t type, int axis, bool positive) {
    // Tile indices in atlas: 0=grass top, 1=grass side, 2=dirt, 3=stone.
    if (type == kGrass) {
      if (axis == 1 && positive) {
        return 0;
      }
      if (axis == 1 && !positive) {
        return 2;
      }
      return 1;
    }
    if (type == kDirt) {
      return 2;
    }
    return 3;
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

    uint32_t startIndex = static_cast<uint32_t>(outVertices.size());
    outVertices.push_back({v0, color, uv0});
    outVertices.push_back({v1, color, uv1});
    outVertices.push_back({v2, color, uv2});
    outVertices.push_back({v3, color, uv3});

    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 1);
    outIndices.push_back(startIndex + 2);
    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 2);
    outIndices.push_back(startIndex + 3);
  };

  for (int z = 0; z < worldDepth; ++z) {
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < worldWidth; ++x) {
        uint8_t blockType = getBlock(x, y, z);
        if (blockType == kAir) {
          continue;
        }

        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        float fz = static_cast<float>(z);
        float fx1 = fx + 1.0f;
        float fy1 = fy + 1.0f;
        float fz1 = fz + 1.0f;

        float heightFactor = 0.6f + 0.4f * (fy / (kChunkHeight - 1));

        // +X face
        if (getBlock(x + 1, y, z) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, true);
          addQuad({fx1, fy, fz}, {fx1, fy1, fz}, {fx1, fy1, fz1}, {fx1, fy, fz1}, color, tile);
        }

        // -X face
        if (getBlock(x - 1, y, z) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, false);
          addQuad({fx, fy, fz}, {fx, fy, fz1}, {fx, fy1, fz1}, {fx, fy1, fz}, color, tile);
        }

        // +Y face (top)
        if (getBlock(x, y + 1, z) == kAir) {
          float shade = 1.0f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, true);
          addQuad({fx, fy1, fz}, {fx, fy1, fz1}, {fx1, fy1, fz1}, {fx1, fy1, fz}, color, tile);
        }

        // -Y face (bottom)
        if (getBlock(x, y - 1, z) == kAir) {
          float shade = 0.5f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, false);
          addQuad({fx, fy, fz}, {fx1, fy, fz}, {fx1, fy, fz1}, {fx, fy, fz1}, color, tile);
        }

        // +Z face
        if (getBlock(x, y, z + 1) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, true);
          addQuad({fx, fy, fz1}, {fx1, fy, fz1}, {fx1, fy1, fz1}, {fx, fy1, fz1}, color, tile);
        }

        // -Z face
        if (getBlock(x, y, z - 1) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, false);
          addQuad({fx, fy, fz}, {fx, fy1, fz}, {fx1, fy1, fz}, {fx1, fy, fz}, color, tile);
        }
      }
    }
  }
}

bool World::save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }

  const char magic[4] = {'C', 'U', 'B', 'E'};
  uint32_t version = 1;
  uint32_t cx = static_cast<uint32_t>(chunksX);
  uint32_t cz = static_cast<uint32_t>(chunksZ);
  uint32_t cs = static_cast<uint32_t>(kChunkSize);
  uint32_t ch = static_cast<uint32_t>(kChunkHeight);

  out.write(magic, 4);
  out.write(reinterpret_cast<const char*>(&version), sizeof(version));
  out.write(reinterpret_cast<const char*>(&cx), sizeof(cx));
  out.write(reinterpret_cast<const char*>(&cz), sizeof(cz));
  out.write(reinterpret_cast<const char*>(&cs), sizeof(cs));
  out.write(reinterpret_cast<const char*>(&ch), sizeof(ch));

  for (const auto& chunk : chunks) {
    out.write(reinterpret_cast<const char*>(chunk.blocks.data()),
              static_cast<std::streamsize>(chunk.blocks.size()));
  }

  return true;
}

bool World::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }

  char magic[4] = {};
  uint32_t version = 0;
  uint32_t cx = 0;
  uint32_t cz = 0;
  uint32_t cs = 0;
  uint32_t ch = 0;

  in.read(magic, 4);
  in.read(reinterpret_cast<char*>(&version), sizeof(version));
  in.read(reinterpret_cast<char*>(&cx), sizeof(cx));
  in.read(reinterpret_cast<char*>(&cz), sizeof(cz));
  in.read(reinterpret_cast<char*>(&cs), sizeof(cs));
  in.read(reinterpret_cast<char*>(&ch), sizeof(ch));

  if (std::strncmp(magic, "CUBE", 4) != 0 || version != 1 ||
      cx != static_cast<uint32_t>(chunksX) ||
      cz != static_cast<uint32_t>(chunksZ) ||
      cs != static_cast<uint32_t>(kChunkSize) ||
      ch != static_cast<uint32_t>(kChunkHeight)) {
    return false;
  }

  for (auto& chunk : chunks) {
    if (!in.read(reinterpret_cast<char*>(chunk.blocks.data()),
                 static_cast<std::streamsize>(chunk.blocks.size()))) {
      return false;
    }
    chunk.dirty = true;
  }

  return true;
}
