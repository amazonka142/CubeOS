#include "world.hpp"

#include <cmath>

World::World(int chunksX, int chunksZ)
    : worldWidth(chunksX * kChunkSize),
      worldDepth(chunksZ * kChunkSize),
      blocks(static_cast<size_t>(worldWidth * worldDepth * kChunkHeight), kAir) {}

int World::index(int x, int y, int z) const {
  return x + z * worldWidth + y * worldWidth * worldDepth;
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
  return blocks[static_cast<size_t>(index(x, y, z))];
}

void World::setBlock(int x, int y, int z, uint8_t type) {
  if (!inBounds(x, y, z)) {
    return;
  }
  blocks[static_cast<size_t>(index(x, y, z))] = type;
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
  std::fill(blocks.begin(), blocks.end(), kAir);

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

  struct FaceDef {
    glm::ivec3 dir;
    glm::vec3 corners[4];
  };

  const FaceDef faces[6] = {
    {{ 1, 0, 0}, {{1,0,0}, {1,0,1}, {1,1,1}, {1,1,0}}}, // +X
    {{-1, 0, 0}, {{0,0,1}, {0,0,0}, {0,1,0}, {0,1,1}}}, // -X
    {{ 0, 1, 0}, {{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}}}, // +Y
    {{ 0,-1, 0}, {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}}}, // -Y
    {{ 0, 0, 1}, {{1,0,1}, {0,0,1}, {0,1,1}, {1,1,1}}}, // +Z
    {{ 0, 0,-1}, {{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}}}  // -Z
  };

  outVertices.reserve(worldWidth * worldDepth * 6);
  outIndices.reserve(worldWidth * worldDepth * 36);

  for (int y = 0; y < kChunkHeight; ++y) {
    for (int z = 0; z < worldDepth; ++z) {
      for (int x = 0; x < worldWidth; ++x) {
        uint8_t type = getBlock(x, y, z);
        if (type == kAir) {
          continue;
        }

        glm::vec3 color = blockColor(type);

        for (const auto& face : faces) {
          int nx = x + face.dir.x;
          int ny = y + face.dir.y;
          int nz = z + face.dir.z;
          if (getBlock(nx, ny, nz) != kAir) {
            continue;
          }

          uint32_t startIndex = static_cast<uint32_t>(outVertices.size());
          for (const auto& corner : face.corners) {
            glm::vec3 pos = glm::vec3(static_cast<float>(x),
                                      static_cast<float>(y),
                                      static_cast<float>(z)) + corner;
            outVertices.push_back({pos, color});
          }

          outIndices.push_back(startIndex + 0);
          outIndices.push_back(startIndex + 1);
          outIndices.push_back(startIndex + 2);
          outIndices.push_back(startIndex + 0);
          outIndices.push_back(startIndex + 2);
          outIndices.push_back(startIndex + 3);
        }
      }
    }
  }
}
