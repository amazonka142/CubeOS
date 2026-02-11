#pragma once

#include <array>

#include <glm/glm.hpp>
#include <volk.h>

constexpr int kAtlasTileSize = 16;
constexpr int kAtlasCols = 4;
constexpr int kAtlasRows = 6;

constexpr int kTileGrassTop = 0;
constexpr int kTileGrassSide = 1;
constexpr int kTileDirt = 2;
constexpr int kTileStone = 3;
constexpr int kBreakTileBase = 4;
constexpr int kBreakStages = 8;
constexpr int kTileWater = 12;
constexpr int kTileUiWhite = 13;
constexpr int kTileSand = 14;
constexpr int kTileGravel = 15;
constexpr int kTileWood = 16;
constexpr int kTileLeaves = 17;
constexpr int kTileCoalOre = 18;
constexpr int kTileIronOre = 19;
constexpr int kTileGoldOre = 20;
constexpr int kTileSeagrass = 21;
constexpr int kTileCoral = 22;

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 uv;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, uv);

    return attributeDescriptions;
  }
};
