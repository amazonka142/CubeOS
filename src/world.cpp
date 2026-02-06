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

  const int dims[3] = {worldWidth, kChunkHeight, worldDepth};
  std::vector<int> maskValues;

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
    float u0 = tx * tileSizeU;
    float v0 = ty * tileSizeV;
    return glm::vec2(u0 + u * tileSizeU, v0 + v * tileSizeV);
  };

  for (int d = 0; d < 3; ++d) {
    int axisUIndex = (d + 1) % 3;
    int axisVIndex = (d + 2) % 3;
    int meshSpanU = (axisUIndex == 0) ? dims[0] : (axisUIndex == 1 ? dims[1] : dims[2]);
    int meshSpanV = (axisVIndex == 0) ? dims[0] : (axisVIndex == 1 ? dims[1] : dims[2]);

    maskValues.assign(static_cast<size_t>(meshSpanU * meshSpanV), 0);

    int x[3] = {0, 0, 0};
    int q[3] = {0, 0, 0};
    q[d] = 1;

    for (x[d] = -1; x[d] < dims[d];) {
      int n = 0;
      for (x[axisVIndex] = 0; x[axisVIndex] < meshSpanV; ++x[axisVIndex]) {
        for (x[axisUIndex] = 0; x[axisUIndex] < meshSpanU; ++x[axisUIndex]) {
          int ax = x[0];
          int ay = x[1];
          int az = x[2];
          int bx = x[0] + q[0];
          int by = x[1] + q[1];
          int bz = x[2] + q[2];

          uint8_t a = (x[d] >= 0) ? getBlock(ax, ay, az) : kAir;
          uint8_t b = (x[d] < dims[d] - 1) ? getBlock(bx, by, bz) : kAir;

          if (a != kAir && b == kAir) {
            maskValues[n] = a;
          } else if (a == kAir && b != kAir) {
            maskValues[n] = -static_cast<int>(b);
          } else {
            maskValues[n] = 0;
          }
          ++n;
        }
      }

      ++x[d];

      n = 0;
      for (int j = 0; j < meshSpanV; ++j) {
        for (int i = 0; i < meshSpanU;) {
          int c = maskValues[n];
          if (c == 0) {
            ++i;
            ++n;
            continue;
          }

          int w = 1;
          while (i + w < meshSpanU && maskValues[n + w] == c) {
            ++w;
          }

          int h = 1;
          bool done = false;
          while (j + h < meshSpanV && !done) {
            for (int k = 0; k < w; ++k) {
              if (maskValues[n + k + h * meshSpanU] != c) {
                done = true;
                break;
              }
            }
            if (!done) {
              ++h;
            }
          }

          int du[3] = {0, 0, 0};
          int dv[3] = {0, 0, 0};
          du[axisUIndex] = w;
          dv[axisVIndex] = h;

          int x0[3] = {x[0], x[1], x[2]};
          x0[axisUIndex] = i;
          x0[axisVIndex] = j;

          float shade = 0.8f;
          if (d == 1) {
            shade = (c > 0) ? 1.0f : 0.5f;
          }
          float heightFactor = 0.6f + 0.4f * (static_cast<float>(x0[1]) / (kChunkHeight - 1));
          uint8_t blockType = static_cast<uint8_t>(std::abs(c));
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, d, c > 0);

          glm::vec3 v0;
          glm::vec3 v1;
          glm::vec3 v2;
          glm::vec3 v3;

          if (c > 0) {
            v0 = glm::vec3(x0[0], x0[1], x0[2]) + glm::vec3(q[0], q[1], q[2]);
            v1 = v0 + glm::vec3(du[0], du[1], du[2]);
            v2 = v1 + glm::vec3(dv[0], dv[1], dv[2]);
            v3 = v0 + glm::vec3(dv[0], dv[1], dv[2]);
          } else {
            v0 = glm::vec3(x0[0], x0[1], x0[2]);
            v1 = v0 + glm::vec3(dv[0], dv[1], dv[2]);
            v2 = v1 + glm::vec3(du[0], du[1], du[2]);
            v3 = v0 + glm::vec3(du[0], du[1], du[2]);
          }

          glm::vec2 uv0 = uvForTile(tile, 0.0f, 0.0f);
          glm::vec2 uv1 = uvForTile(tile, 1.0f, 0.0f);
          glm::vec2 uv2 = uvForTile(tile, 1.0f, 1.0f);
          glm::vec2 uv3 = uvForTile(tile, 0.0f, 1.0f);

          glm::vec3 e1 = v1 - v0;
          glm::vec3 e2 = v2 - v0;
          glm::vec3 n = glm::cross(e1, e2);
          glm::vec3 expectedNormal(0.0f);
          expectedNormal[d] = (c > 0) ? 1.0f : -1.0f;
          if (glm::dot(n, expectedNormal) < 0.0f) {
            std::swap(v1, v3);
            std::swap(uv1, uv3);
          }

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

          for (int rowIdx = 0; rowIdx < h; ++rowIdx) {
            int maskRowStart = n + rowIdx * meshSpanU;
            for (int colIdx = 0; colIdx < w; ++colIdx) {
              maskValues[maskRowStart + colIdx] = 0;
            }
          }

          i += w;
          n += w;
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
