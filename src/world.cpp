#include "world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

#include <glm/gtc/noise.hpp>

namespace {

int floorDiv(int value, int divisor) {
  int q = value / divisor;
  int r = value % divisor;
  if (r != 0 && ((r < 0) != (divisor < 0))) {
    q -= 1;
  }
  return q;
}

int positiveMod(int value, int divisor) {
  int m = value % divisor;
  if (m < 0) {
    m += divisor;
  }
  return m;
}

float fbmNoise(float x, float z, int seed) {
  float amplitude = 1.0f;
  float frequency = 0.008f;
  float total = 0.0f;
  float maxValue = 0.0f;
  float seedX = static_cast<float>(seed) * 0.13f;
  float seedZ = static_cast<float>(seed) * 0.17f;
  for (int i = 0; i < 4; ++i) {
    glm::vec2 p((x + seedX) * frequency, (z + seedZ) * frequency);
    total += glm::perlin(p) * amplitude;
    maxValue += amplitude;
    amplitude *= 0.5f;
    frequency *= 2.0f;
  }
  if (maxValue <= 0.0f) {
    return 0.0f;
  }
  return total / maxValue;
}

float saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

float smooth01(float v) {
  float t = saturate(v);
  return t * t * (3.0f - 2.0f * t);
}

uint32_t mixBits(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

float hashedNoise01(int x, int y, int z, int seed, uint32_t salt) {
  uint32_t h = static_cast<uint32_t>(x) * 73856093u;
  h ^= static_cast<uint32_t>(y) * 19349663u;
  h ^= static_cast<uint32_t>(z) * 83492791u;
  h ^= static_cast<uint32_t>(seed) * 2654435761u;
  h ^= salt;
  h = mixBits(h);
  return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
}

int chunkLocalIndex(int lx, int ly, int lz) {
  return lx + lz * kChunkSize + ly * kChunkSize * kChunkSize;
}

std::vector<uint8_t> generateChunkBlocks(int cx, int cz, int seed) {
  std::vector<uint8_t> blocks(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  int baseX = cx * kChunkSize;
  int baseZ = cz * kChunkSize;
  constexpr int kSeaLevel = 36;

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      int worldX = baseX + lx;
      int worldZ = baseZ + lz;

      float rollingHills = glm::perlin(glm::vec2(
        static_cast<float>(worldX + seed * 11) * 0.0035f,
        static_cast<float>(worldZ - seed * 13) * 0.0035f));
      float detailNoise = fbmNoise(static_cast<float>(worldX), static_cast<float>(worldZ), seed);
      float baseHeight = 34.0f + rollingHills * 11.0f + detailNoise * 9.0f;

      float ridgeNoise = 1.0f - std::abs(glm::perlin(glm::vec2(
        static_cast<float>(worldX - seed * 5) * 0.0012f,
        static_cast<float>(worldZ + seed * 3) * 0.0012f)));
      float mountainMask = smooth01((ridgeNoise - 0.55f) / 0.45f);
      float mountainHeight = mountainMask * (18.0f + 44.0f * mountainMask);

      float canyonSignal = 1.0f - std::abs(glm::perlin(glm::vec2(
        static_cast<float>(worldX + seed * 17) * 0.0018f,
        static_cast<float>(worldZ - seed * 11) * 0.0018f)));
      float canyonMask = smooth01((canyonSignal - 0.78f) / 0.22f);
      float canyonDepth = canyonMask * (16.0f + 24.0f * canyonMask);

      float heightF = baseHeight + mountainHeight - canyonDepth;
      int height = static_cast<int>(std::round(heightF));
      height = std::clamp(height, 8, kChunkHeight - 2);

      for (int y = 0; y < height; ++y) {
        uint8_t type = kStone;
        bool isSurface = (y == height - 1);
        bool isSubsurface = (y >= height - 4);
        bool isCanyonFloor = canyonMask > 0.55f;
        bool isBeach = height <= kSeaLevel;

        if (isSurface) {
          if (isCanyonFloor) {
            type = kGravel;
          } else if (isBeach) {
            type = kSand;
          } else {
            type = kGrass;
          }
        } else if (isSubsurface) {
          if (isCanyonFloor) {
            type = kGravel;
          } else if (isBeach) {
            type = kSand;
          } else {
            type = kDirt;
          }
        }

        blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = type;
      }

      for (int y = 5; y < height - 1; ++y) {
        size_t idx = static_cast<size_t>(chunkLocalIndex(lx, y, lz));
        if (blocks[idx] == kAir) {
          continue;
        }

        float caveA = glm::perlin(glm::vec3(
          static_cast<float>(worldX + seed * 31) * 0.045f,
          static_cast<float>(y - seed * 13) * 0.055f,
          static_cast<float>(worldZ - seed * 29) * 0.045f));
        float caveB = glm::perlin(glm::vec3(
          static_cast<float>(worldX - seed * 7) * 0.09f,
          static_cast<float>(y + seed * 19) * 0.09f,
          static_cast<float>(worldZ + seed * 23) * 0.09f));
        float caveShape = std::abs(caveA * 0.72f + caveB * 0.28f);
        float depth01 = static_cast<float>(y) / static_cast<float>(height);
        float caveThreshold = 0.11f + 0.06f * depth01;
        if (caveShape < caveThreshold) {
          blocks[idx] = kAir;
          continue;
        }

        if (blocks[idx] != kStone) {
          continue;
        }

        int oreX3 = floorDiv(worldX, 3);
        int oreY3 = floorDiv(y, 3);
        int oreZ3 = floorDiv(worldZ, 3);
        float coalNoise = hashedNoise01(oreX3, oreY3, oreZ3, seed, 0xC011u);
        float ironNoise = hashedNoise01(floorDiv(worldX, 4),
                                        floorDiv(y, 4),
                                        floorDiv(worldZ, 4),
                                        seed,
                                        0x1A2Bu);
        float goldNoise = hashedNoise01(floorDiv(worldX, 4),
                                        floorDiv(y, 4),
                                        floorDiv(worldZ, 4),
                                        seed,
                                        0x90D1u);

        if (y < 40 && goldNoise > 0.94f) {
          blocks[idx] = kGoldOre;
        } else if (y < 72 && ironNoise > 0.90f) {
          blocks[idx] = kIronOre;
        } else if (y < 96 && coalNoise > 0.84f) {
          blocks[idx] = kCoalOre;
        }
      }

      if (height < kSeaLevel) {
        for (int y = height; y < kSeaLevel; ++y) {
          blocks[static_cast<size_t>(chunkLocalIndex(lx, y, lz))] = kWater;
        }
      }
    }
  }

  return blocks;
}

} // namespace

World::World(int initialChunksXIn, int initialChunksZIn, int seedIn)
    : initialChunksX(initialChunksXIn),
      initialChunksZ(initialChunksZIn),
      seed(seedIn) {
  int maxDim = std::max(initialChunksX, initialChunksZ);
  initialRadius = std::max(1, maxDim / 2);
  startChunkWorkers();
}

World::~World() {
  stopChunkWorkers();
}

void World::startChunkWorkers() {
  uint32_t hw = std::thread::hardware_concurrency();
  size_t workerCount = hw == 0 ? 2u : static_cast<size_t>(hw);
  workerCount = std::max<size_t>(1, std::min<size_t>(workerCount, 4));

  generationWorkers.reserve(workerCount);
  for (size_t i = 0; i < workerCount; ++i) {
    generationWorkers.emplace_back([this]() {
      while (true) {
        ChunkGenerationTask task;
        {
          std::unique_lock<std::mutex> lock(generationMutex);
          generationCv.wait(lock, [this]() {
            return stopGenerationWorkers || !generationQueue.empty();
          });

          if (stopGenerationWorkers && generationQueue.empty()) {
            return;
          }

          task = generationQueue.front();
          generationQueue.pop();
        }

        ChunkGenerationResult result;
        result.key = task.key;
        result.epoch = task.epoch;
        result.blocks = generateChunkBlocks(task.cx, task.cz, task.seed);

        {
          std::lock_guard<std::mutex> lock(generationMutex);
          generationResults.push(std::move(result));
        }
      }
    });
  }
}

void World::stopChunkWorkers() {
  {
    std::lock_guard<std::mutex> lock(generationMutex);
    stopGenerationWorkers = true;
    while (!generationQueue.empty()) {
      generationQueue.pop();
    }
  }
  generationCv.notify_all();

  for (std::thread& worker : generationWorkers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  generationWorkers.clear();
}

int World::localIndex(int lx, int ly, int lz) const {
  return lx + lz * kChunkSize + ly * kChunkSize * kChunkSize;
}

uint64_t World::chunkKey(int cx, int cz) const {
  uint64_t ux = static_cast<uint32_t>(cx);
  uint64_t uz = static_cast<uint32_t>(cz);
  return (ux << 32) | uz;
}

World::Chunk* World::findChunk(int cx, int cz) {
  auto it = chunks.find(chunkKey(cx, cz));
  if (it == chunks.end()) {
    return nullptr;
  }
  return &it->second;
}

const World::Chunk* World::findChunk(int cx, int cz) const {
  auto it = chunks.find(chunkKey(cx, cz));
  if (it == chunks.end()) {
    return nullptr;
  }
  return &it->second;
}

void World::queueChunkGeneration(int cx, int cz, uint32_t epoch) {
  uint64_t key = chunkKey(cx, cz);
  auto pendingIt = pendingGenerationEpochByKey.find(key);
  if (pendingIt != pendingGenerationEpochByKey.end() && pendingIt->second == epoch) {
    return;
  }
  pendingGenerationEpochByKey[key] = epoch;

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    generationQueue.push({cx, cz, seed, epoch, key});
  }
  generationCv.notify_one();
}

void World::resetChunkGeneration() {
  ++generationEpoch;
  pendingGenerationEpochByKey.clear();

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    while (!generationQueue.empty()) {
      generationQueue.pop();
    }
    while (!generationResults.empty()) {
      generationResults.pop();
    }
  }
}

void World::markNeighborChunksDirty(int cx, int cz) {
  constexpr std::array<std::pair<int, int>, 4> offsets = {{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1}
  }};

  for (const auto& [ox, oz] : offsets) {
    Chunk* neighbor = findChunk(cx + ox, cz + oz);
    if (neighbor) {
      neighbor->dirty = true;
    }
  }
}

void World::pumpChunkGeneration() {
  std::vector<ChunkGenerationResult> ready;

  {
    std::lock_guard<std::mutex> lock(generationMutex);
    while (!generationResults.empty()) {
      ready.push_back(std::move(generationResults.front()));
      generationResults.pop();
    }
  }

  if (ready.empty()) {
    return;
  }

  for (ChunkGenerationResult& result : ready) {
    auto pendingIt = pendingGenerationEpochByKey.find(result.key);
    if (pendingIt != pendingGenerationEpochByKey.end() && pendingIt->second == result.epoch) {
      pendingGenerationEpochByKey.erase(pendingIt);
    }

    auto chunkIt = chunks.find(result.key);
    if (chunkIt == chunks.end()) {
      continue;
    }

    Chunk& chunk = chunkIt->second;
    if (!chunk.generating || chunk.generationEpoch != result.epoch) {
      continue;
    }

    chunk.blocks = std::move(result.blocks);
    chunk.generating = false;
    chunk.dirty = true;
    markNeighborChunksDirty(chunk.cx, chunk.cz);
    meshDirty = true;
  }
}

World::Chunk& World::ensureChunk(int cx, int cz) {
  uint64_t key = chunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second;
  }

  Chunk chunk;
  chunk.cx = cx;
  chunk.cz = cz;
  chunk.blocks.resize(static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight), kAir);
  chunk.dirty = true;

  auto savedIt = savedChunks.find(key);
  if (savedIt != savedChunks.end()) {
    chunk.blocks = savedIt->second;
    chunk.modified = true;
    savedChunks.erase(savedIt);
  } else {
    chunk.generating = true;
    chunk.generationEpoch = generationEpoch;
  }

  auto insertResult = chunks.emplace(key, std::move(chunk));
  if (insertResult.first->second.generating) {
    queueChunkGeneration(cx, cz, insertResult.first->second.generationEpoch);
  } else {
    markNeighborChunksDirty(cx, cz);
  }
  meshDirty = true;
  return insertResult.first->second;
}

void World::generateChunk(World::Chunk& chunk) {
  chunk.blocks = generateChunkBlocks(chunk.cx, chunk.cz, seed);
  chunk.generating = false;
  chunk.generationEpoch = generationEpoch;
}

bool World::inBounds(int x, int y, int z) const {
  (void)x;
  (void)z;
  return y >= 0 && y < kChunkHeight;
}

uint8_t World::getBlock(int x, int y, int z) const {
  if (!inBounds(x, y, z)) {
    return kAir;
  }
  int cx = floorDiv(x, kChunkSize);
  int cz = floorDiv(z, kChunkSize);
  int lx = positiveMod(x, kChunkSize);
  int lz = positiveMod(z, kChunkSize);
  const Chunk* chunk = findChunk(cx, cz);
  if (!chunk) {
    return kAir;
  }
  return chunk->blocks[static_cast<size_t>(localIndex(lx, y, lz))];
}

void World::setBlock(int x, int y, int z, uint8_t type) {
  pumpChunkGeneration();

  if (!inBounds(x, y, z)) {
    return;
  }
  int cx = floorDiv(x, kChunkSize);
  int cz = floorDiv(z, kChunkSize);
  int lx = positiveMod(x, kChunkSize);
  int lz = positiveMod(z, kChunkSize);

  Chunk* chunk = findChunk(cx, cz);
  if (!chunk) {
    if (type == kAir) {
      return;
    }
    chunk = &ensureChunk(cx, cz);
  }

  if (chunk->generating) {
    generateChunk(*chunk);
    pendingGenerationEpochByKey.erase(chunkKey(cx, cz));
  }

  size_t idx = static_cast<size_t>(localIndex(lx, y, lz));
  if (chunk->blocks[idx] == type) {
    return;
  }
  chunk->blocks[idx] = type;
  chunk->dirty = true;
  chunk->modified = true;
  if (lx == 0 || lx == kChunkSize - 1 || lz == 0 || lz == kChunkSize - 1) {
    markNeighborChunksDirty(cx, cz);
  }
  meshDirty = true;
}

glm::vec3 World::blockColor(uint8_t type) const {
  switch (type) {
    case kGrass:
      return {0.2f, 0.8f, 0.2f};
    case kDirt:
      return {0.55f, 0.35f, 0.2f};
    case kSand:
      return {0.86f, 0.78f, 0.50f};
    case kGravel:
      return {0.46f, 0.44f, 0.42f};
    case kWood:
      return {0.49f, 0.33f, 0.16f};
    case kLeaves:
      return {0.16f, 0.58f, 0.16f};
    case kWater:
      return {0.22f, 0.45f, 0.88f};
    case kCoalOre:
      return {0.22f, 0.22f, 0.22f};
    case kIronOre:
      return {0.73f, 0.54f, 0.40f};
    case kGoldOre:
      return {0.92f, 0.75f, 0.22f};
    case kStone:
      return {0.6f, 0.6f, 0.6f};
    default:
      return {1.0f, 1.0f, 1.0f};
  }
}

void World::generate() {
  resetChunkGeneration();
  chunks.clear();
  savedChunks.clear();
  breakOverlay.active = false;
  breakOverlay.stage = 0;
  meshDirty = true;
  updateActiveChunks(0, 0, initialRadius);
}

void World::updateActiveChunks(int centerChunkX, int centerChunkZ, int radius) {
  pumpChunkGeneration();

  if (radius < 0) {
    return;
  }

  for (int cz = centerChunkZ - radius; cz <= centerChunkZ + radius; ++cz) {
    for (int cx = centerChunkX - radius; cx <= centerChunkX + radius; ++cx) {
      (void)ensureChunk(cx, cz);
    }
  }

  for (auto it = chunks.begin(); it != chunks.end();) {
    int cx = it->second.cx;
    int cz = it->second.cz;
    int dist = std::max(std::abs(cx - centerChunkX), std::abs(cz - centerChunkZ));
    if (dist > radius) {
      if (it->second.modified) {
        savedChunks[it->first] = it->second.blocks;
      }
      it = chunks.erase(it);
      meshDirty = true;
    } else {
      ++it;
    }
  }
}

bool World::consumeMeshDirty() {
  pumpChunkGeneration();

  if (!meshDirty) {
    return false;
  }
  meshDirty = false;
  return true;
}

void World::setBreakOverlay(const glm::ivec3& block, int stage) {
  if (stage <= 0) {
    clearBreakOverlay();
    return;
  }
  int clampedStage = std::clamp(stage, 1, kBreakStages);
  if (breakOverlay.active && breakOverlay.block == block && breakOverlay.stage == clampedStage) {
    return;
  }

  auto markChunkDirty = [&](const glm::ivec3& pos) {
    int cx = floorDiv(pos.x, kChunkSize);
    int cz = floorDiv(pos.z, kChunkSize);
    Chunk* chunk = findChunk(cx, cz);
    if (chunk) {
      chunk->dirty = true;
    }
    meshDirty = true;
  };

  if (breakOverlay.active) {
    markChunkDirty(breakOverlay.block);
  }

  breakOverlay.active = true;
  breakOverlay.block = block;
  breakOverlay.stage = clampedStage;
  markChunkDirty(block);
}

void World::clearBreakOverlay() {
  if (!breakOverlay.active) {
    return;
  }
  glm::ivec3 prevBlock = breakOverlay.block;
  breakOverlay.active = false;
  breakOverlay.stage = 0;

  int cx = floorDiv(prevBlock.x, kChunkSize);
  int cz = floorDiv(prevBlock.z, kChunkSize);
  Chunk* chunk = findChunk(cx, cz);
  if (chunk) {
    chunk->dirty = true;
  }
  meshDirty = true;
}

void World::buildChunkMesh(World::Chunk& chunk) {
  chunk.meshVertices.clear();
  chunk.meshIndices.clear();

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
    if (type == kDirt || type == kSand || type == kWood || type == kWater) {
      return 2;
    }
    if (type == kLeaves) {
      return 1;
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

    uint32_t startIndex = static_cast<uint32_t>(chunk.meshVertices.size());
    chunk.meshVertices.push_back({v0, color, uv0});
    chunk.meshVertices.push_back({v1, color, uv1});
    chunk.meshVertices.push_back({v2, color, uv2});
    chunk.meshVertices.push_back({v3, color, uv3});

    chunk.meshIndices.push_back(startIndex + 0);
    chunk.meshIndices.push_back(startIndex + 1);
    chunk.meshIndices.push_back(startIndex + 2);
    chunk.meshIndices.push_back(startIndex + 0);
    chunk.meshIndices.push_back(startIndex + 2);
    chunk.meshIndices.push_back(startIndex + 3);
  };

  const float overlayOffset = 0.01f;
  bool overlayActive = breakOverlay.active && breakOverlay.stage > 0;
  int overlayTile = kBreakTileBase + std::clamp(breakOverlay.stage, 1, kBreakStages) - 1;
  int overlayX = breakOverlay.block.x;
  int overlayY = breakOverlay.block.y;
  int overlayZ = breakOverlay.block.z;

  int baseX = chunk.cx * kChunkSize;
  int baseZ = chunk.cz * kChunkSize;

  for (int lz = 0; lz < kChunkSize; ++lz) {
    for (int lx = 0; lx < kChunkSize; ++lx) {
      for (int y = 0; y < kChunkHeight; ++y) {
        uint8_t blockType = chunk.blocks[static_cast<size_t>(localIndex(lx, y, lz))];
        if (blockType == kAir) {
          continue;
        }

        int x = baseX + lx;
        int z = baseZ + lz;

        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        float fz = static_cast<float>(z);
        float fx1 = fx + 1.0f;
        float fy1 = fy + 1.0f;
        float fz1 = fz + 1.0f;

        float heightFactor = 0.6f + 0.4f * (fy / (kChunkHeight - 1));

        bool isBreakTarget = overlayActive &&
                             overlayX == x && overlayY == y && overlayZ == z;

        if (getBlock(x + 1, y, z) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, true);
          addQuad({fx1, fy, fz}, {fx1, fy1, fz}, {fx1, fy1, fz1}, {fx1, fy, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(overlayOffset, 0.0f, 0.0f);
            addQuad(glm::vec3(fx1, fy, fz) + offset,
                    glm::vec3(fx1, fy1, fz) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (getBlock(x - 1, y, z) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 0, false);
          addQuad({fx, fy, fz}, {fx, fy, fz1}, {fx, fy1, fz1}, {fx, fy1, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(-overlayOffset, 0.0f, 0.0f);
            addQuad(glm::vec3(fx, fy, fz) + offset,
                    glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(fx, fy1, fz) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (getBlock(x, y + 1, z) == kAir) {
          float shade = 1.0f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, true);
          addQuad({fx, fy1, fz}, {fx, fy1, fz1}, {fx1, fy1, fz1}, {fx1, fy1, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, overlayOffset, 0.0f);
            addQuad(glm::vec3(fx, fy1, fz) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx1, fy1, fz) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (getBlock(x, y - 1, z) == kAir) {
          float shade = 0.5f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 1, false);
          addQuad({fx, fy, fz}, {fx1, fy, fz}, {fx1, fy, fz1}, {fx, fy, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, -overlayOffset, 0.0f);
            addQuad(glm::vec3(fx, fy, fz) + offset,
                    glm::vec3(fx1, fy, fz) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (getBlock(x, y, z + 1) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, true);
          addQuad({fx, fy, fz1}, {fx1, fy, fz1}, {fx1, fy1, fz1}, {fx, fy1, fz1}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, overlayOffset);
            addQuad(glm::vec3(fx, fy, fz1) + offset,
                    glm::vec3(fx1, fy, fz1) + offset,
                    glm::vec3(fx1, fy1, fz1) + offset,
                    glm::vec3(fx, fy1, fz1) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }

        if (getBlock(x, y, z - 1) == kAir) {
          float shade = 0.8f;
          glm::vec3 color = blockColor(blockType) * shade * heightFactor;
          int tile = tileFor(blockType, 2, false);
          addQuad({fx, fy, fz}, {fx, fy1, fz}, {fx1, fy1, fz}, {fx1, fy, fz}, color, tile);
          if (isBreakTarget) {
            glm::vec3 offset(0.0f, 0.0f, -overlayOffset);
            addQuad(glm::vec3(fx, fy, fz) + offset,
                    glm::vec3(fx, fy1, fz) + offset,
                    glm::vec3(fx1, fy1, fz) + offset,
                    glm::vec3(fx1, fy, fz) + offset,
                    glm::vec3(1.0f),
                    overlayTile);
          }
        }
      }
    }
  }

  chunk.dirty = false;
}

void World::buildMesh(std::vector<Vertex>& outVertices,
                      std::vector<uint32_t>& outIndices) {
  pumpChunkGeneration();

  outVertices.clear();
  outIndices.clear();

  for (auto& entry : chunks) {
    Chunk& chunk = entry.second;
    if (chunk.generating) {
      continue;
    }
    if (chunk.dirty) {
      buildChunkMesh(chunk);
    }

    uint32_t vertexOffset = static_cast<uint32_t>(outVertices.size());
    outVertices.insert(outVertices.end(),
                       chunk.meshVertices.begin(),
                       chunk.meshVertices.end());

    outIndices.reserve(outIndices.size() + chunk.meshIndices.size());
    for (uint32_t idx : chunk.meshIndices) {
      outIndices.push_back(idx + vertexOffset);
    }
  }
}

bool World::save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }

  const char magic[4] = {'C', 'U', 'B', '2'};
  uint32_t version = 3;
  uint32_t cs = static_cast<uint32_t>(kChunkSize);
  uint32_t ch = static_cast<uint32_t>(kChunkHeight);
  uint32_t seedValue = static_cast<uint32_t>(seed);

  uint32_t modifiedCount = 0;
  for (const auto& entry : chunks) {
    if (entry.second.modified) {
      ++modifiedCount;
    }
  }
  uint32_t storedCount = static_cast<uint32_t>(savedChunks.size()) + modifiedCount;

  out.write(magic, 4);
  out.write(reinterpret_cast<const char*>(&version), sizeof(version));
  out.write(reinterpret_cast<const char*>(&cs), sizeof(cs));
  out.write(reinterpret_cast<const char*>(&ch), sizeof(ch));
  out.write(reinterpret_cast<const char*>(&seedValue), sizeof(seedValue));
  out.write(reinterpret_cast<const char*>(&storedCount), sizeof(storedCount));

  for (const auto& entry : savedChunks) {
    uint64_t key = entry.first;
    int32_t cx = static_cast<int32_t>(static_cast<uint32_t>(key >> 32));
    int32_t cz = static_cast<int32_t>(static_cast<uint32_t>(key & 0xffffffffu));
    out.write(reinterpret_cast<const char*>(&cx), sizeof(cx));
    out.write(reinterpret_cast<const char*>(&cz), sizeof(cz));
    out.write(reinterpret_cast<const char*>(entry.second.data()),
              static_cast<std::streamsize>(entry.second.size()));
  }

  for (const auto& entry : chunks) {
    if (!entry.second.modified) {
      continue;
    }
    int32_t cx = static_cast<int32_t>(entry.second.cx);
    int32_t cz = static_cast<int32_t>(entry.second.cz);
    out.write(reinterpret_cast<const char*>(&cx), sizeof(cx));
    out.write(reinterpret_cast<const char*>(&cz), sizeof(cz));
    out.write(reinterpret_cast<const char*>(entry.second.blocks.data()),
              static_cast<std::streamsize>(entry.second.blocks.size()));
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
  uint32_t cs = 0;
  uint32_t ch = 0;
  uint32_t seedValue = 0;
  uint32_t storedCount = 0;

  in.read(magic, 4);
  in.read(reinterpret_cast<char*>(&version), sizeof(version));
  in.read(reinterpret_cast<char*>(&cs), sizeof(cs));
  in.read(reinterpret_cast<char*>(&ch), sizeof(ch));
  in.read(reinterpret_cast<char*>(&seedValue), sizeof(seedValue));
  in.read(reinterpret_cast<char*>(&storedCount), sizeof(storedCount));

  if (std::strncmp(magic, "CUB2", 4) != 0 || version != 3 ||
      cs != static_cast<uint32_t>(kChunkSize) ||
      ch != static_cast<uint32_t>(kChunkHeight)) {
    return false;
  }

  seed = static_cast<int>(seedValue);
  resetChunkGeneration();
  chunks.clear();
  savedChunks.clear();
  breakOverlay.active = false;
  breakOverlay.stage = 0;

  size_t blocksSize = static_cast<size_t>(kChunkSize * kChunkSize * kChunkHeight);
  for (uint32_t i = 0; i < storedCount; ++i) {
    int32_t cx = 0;
    int32_t cz = 0;
    in.read(reinterpret_cast<char*>(&cx), sizeof(cx));
    in.read(reinterpret_cast<char*>(&cz), sizeof(cz));
    std::vector<uint8_t> blocks(blocksSize);
    if (!in.read(reinterpret_cast<char*>(blocks.data()),
                 static_cast<std::streamsize>(blocks.size()))) {
      return false;
    }
    savedChunks[chunkKey(cx, cz)] = std::move(blocks);
  }

  meshDirty = true;
  return true;
}
