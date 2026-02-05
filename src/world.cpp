#include "world.hpp"

#include <cmath>

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
  std::vector<int> mask(static_cast<size_t>(dims[0] * dims[1]));

  for (int d = 0; d < 3; ++d) {
    int u = (d + 1) % 3;
    int v = (d + 2) % 3;

    int x[3] = {0, 0, 0};
    int q[3] = {0, 0, 0};
    q[d] = 1;

    for (x[d] = -1; x[d] < dims[d];) {
      int n = 0;
      for (x[v] = 0; x[v] < dims[v]; ++x[v]) {
        for (x[u] = 0; x[u] < dims[u]; ++x[u]) {
          int ax = x[0];
          int ay = x[1];
          int az = x[2];
          int bx = x[0] + q[0];
          int by = x[1] + q[1];
          int bz = x[2] + q[2];

          uint8_t a = (x[d] >= 0) ? getBlock(ax, ay, az) : kAir;
          uint8_t b = (x[d] < dims[d] - 1) ? getBlock(bx, by, bz) : kAir;

          if (a != kAir && b == kAir) {
            mask[n] = a;
          } else if (a == kAir && b != kAir) {
            mask[n] = -static_cast<int>(b);
          } else {
            mask[n] = 0;
          }
          ++n;
        }
      }

      ++x[d];

      n = 0;
      for (int j = 0; j < dims[v]; ++j) {
        for (int i = 0; i < dims[u];) {
          int c = mask[n];
          if (c == 0) {
            ++i;
            ++n;
            continue;
          }

          int w = 1;
          while (i + w < dims[u] && mask[n + w] == c) {
            ++w;
          }

          int h = 1;
          bool done = false;
          while (j + h < dims[v] && !done) {
            for (int k = 0; k < w; ++k) {
              if (mask[n + k + h * dims[u]] != c) {
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
          du[u] = w;
          dv[v] = h;

          int x0[3] = {x[0], x[1], x[2]};
          x0[u] = i;
          x0[v] = j;

          glm::vec3 color = blockColor(static_cast<uint8_t>(std::abs(c)));

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

          uint32_t startIndex = static_cast<uint32_t>(outVertices.size());
          outVertices.push_back({v0, color});
          outVertices.push_back({v1, color});
          outVertices.push_back({v2, color});
          outVertices.push_back({v3, color});

          outIndices.push_back(startIndex + 0);
          outIndices.push_back(startIndex + 1);
          outIndices.push_back(startIndex + 2);
          outIndices.push_back(startIndex + 0);
          outIndices.push_back(startIndex + 2);
          outIndices.push_back(startIndex + 3);

          for (int y = 0; y < h; ++y) {
            for (int x2 = 0; x2 < w; ++x2) {
              mask[n + x2 + y * dims[u]] = 0;
            }
          }

          i += w;
          n += w;
        }
      }
    }
  }
}
