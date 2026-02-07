#include "app.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <stdexcept>

namespace {

constexpr float kSlotSize = 40.0f;
constexpr float kSlotPadding = 6.0f;
constexpr float kSlotBorder = 3.0f;
constexpr float kMarginBottom = 20.0f;
constexpr float kIconPadding = 6.0f;
constexpr float kPanelPadding = 12.0f;
constexpr int kChunkViewRadius = 6;
constexpr int kInventoryCols = 9;
constexpr int kInventoryRows = 3;
constexpr uint16_t kMaxStack = 64;
constexpr int kDigitWidth = 3;
constexpr int kDigitHeight = 5;
constexpr float kBreakDuration = 0.6f;
constexpr float kBreakMaxDistance = 6.0f;

constexpr uint8_t kDigitMap[10][kDigitHeight] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

int tileForBlock(uint8_t type) {
  switch (type) {
    case kGrass:
      return 0;
    case kDirt:
      return 2;
    case kSand:
      return 2;
    case kGravel:
      return 3;
    case kWood:
      return 2;
    case kLeaves:
      return 1;
    case kWater:
      return 2;
    case kCoalOre:
    case kIronOre:
    case kGoldOre:
    case kStone:
      return 3;
    default:
      return 3;
  }
}

glm::vec2 uvForTile(int tile, float u, float v) {
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
}

} // namespace

void App::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}

void App::initWindow() {
  if (glfwInit() != GLFW_TRUE) {
    throw std::runtime_error("Failed to initialize GLFW.");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(width, height, "CubeOS Voxel", nullptr, nullptr);
  if (!window) {
    throw std::runtime_error("Failed to create GLFW window.");
  }

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, App::framebufferResizeCallback);
  glfwSetCursorPosCallback(window, App::mouseCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void App::initVulkan() {
  if (!world.load("world.bin")) {
    world.generate();
  }
  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  world.updateActiveChunks(cx, cz, kChunkViewRadius);
  currentChunkX = cx;
  currentChunkZ = cz;
  chunkCenterValid = true;
  world.buildMesh(worldVertices, worldIndices);
  rebuildUiMesh();
  composeMeshData();
  vk.setMeshData(meshVertices, meshIndices, worldIndexCount, uiIndexCount);
  uiDirty = false;
  refreshSelectedBlock();
  vk.init(window, &framebufferResized);
}

void App::mainLoop() {
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;

    glfwPollEvents();
    processInput(deltaTime);
    if (!inventoryOpen) {
      updatePlayer(deltaTime);
    }
    updateStreaming();

    bool worldChanged = world.consumeMeshDirty();
    if (worldChanged) {
      world.buildMesh(worldVertices, worldIndices);
    }
    if (uiDirty) {
      rebuildUiMesh();
    }
    if (worldChanged || uiDirty) {
      composeMeshData();
      vk.updateMesh(meshVertices, meshIndices, worldIndexCount, uiIndexCount);
      uiDirty = false;
    }

    glm::vec3 eye = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    glm::vec3 front = cameraFront();
    glm::mat4 view = glm::lookAt(eye, eye + front, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(70.0f),
                                      width / static_cast<float>(height),
                                      0.1f,
                                      200.0f);
    proj[1][1] *= -1.0f;

    vk.setCameraMatrices(view, proj);
    vk.drawFrame();
  }

  vk.waitIdle();
}

void App::processInput(float deltaTime) {
  bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (escPressed && !escDown) {
    if (inventoryOpen) {
      setInventoryOpen(false);
    } else {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    escDown = true;
  } else if (!escPressed) {
    escDown = false;
  }

  bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
  if (tabPressed && !tabDown) {
    setInventoryOpen(!inventoryOpen);
    tabDown = true;
  } else if (!tabPressed) {
    tabDown = false;
  }

  int prevSlot = selectedSlot;
  for (int i = 0; i < static_cast<int>(hotbar.size()); ++i) {
    if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
      selectedSlot = i;
    }
  }
  if (selectedSlot != prevSlot) {
    refreshSelectedBlock();
    uiDirty = true;
  }

  bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  if (inventoryOpen) {
    if (leftPressed && !mouseLeftDown) {
      double xpos = 0.0;
      double ypos = 0.0;
      glfwGetCursorPos(window, &xpos, &ypos);
      if (handleInventoryClick(xpos, ypos, false)) {
        refreshSelectedBlock();
        uiDirty = true;
      }
    }
    if (rightPressed && !mouseRightDown) {
      double xpos = 0.0;
      double ypos = 0.0;
      glfwGetCursorPos(window, &xpos, &ypos);
      if (handleInventoryClick(xpos, ypos, true)) {
        refreshSelectedBlock();
        uiDirty = true;
      }
    }

    mouseLeftDown = leftPressed;
    mouseRightDown = rightPressed;

    if (breakingActive) {
      breakingActive = false;
      breakingProgress = 0.0f;
      breakingStage = 0;
      world.clearBreakOverlay();
    }

    (void)deltaTime;
    return;
  }

  glm::vec3 front = cameraFront();
  glm::vec3 forward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
  glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

  glm::vec3 wishDir{0.0f};
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    wishDir += forward;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    wishDir -= forward;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    wishDir += right;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    wishDir -= right;
  }

  if (glm::length(wishDir) > 0.001f) {
    wishDir = glm::normalize(wishDir);
  }

  const float moveSpeed = 6.0f;
  playerVel.x = wishDir.x * moveSpeed;
  playerVel.z = wishDir.z * moveSpeed;

  if (onGround && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    playerVel.y = 6.5f;
    onGround = false;
  }

  if (leftPressed) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    RaycastHit hit = raycast(origin, cameraFront(), kBreakMaxDistance);
    if (hit.hit) {
      if (!breakingActive || hit.block != breakingBlock) {
        breakingActive = true;
        breakingBlock = hit.block;
        breakingProgress = 0.0f;
        breakingStage = 0;
      }

      breakingProgress += deltaTime;
      int newStage = static_cast<int>((breakingProgress / kBreakDuration) * kBreakStages) + 1;
      newStage = std::clamp(newStage, 1, kBreakStages);
      if (newStage != breakingStage) {
        breakingStage = newStage;
        world.setBreakOverlay(breakingBlock, breakingStage);
      }

      if (breakingProgress >= kBreakDuration) {
        uint8_t removed = world.getBlock(hit.block.x, hit.block.y, hit.block.z);
        world.setBlock(hit.block.x, hit.block.y, hit.block.z, kAir);
        if (removed != kAir) {
          addToInventory(removed, 1);
          refreshSelectedBlock();
          uiDirty = true;
        }
        breakingActive = false;
        breakingProgress = 0.0f;
        breakingStage = 0;
        world.clearBreakOverlay();
      }
    } else if (breakingActive) {
      breakingActive = false;
      breakingProgress = 0.0f;
      breakingStage = 0;
      world.clearBreakOverlay();
    }
  } else if (breakingActive) {
    breakingActive = false;
    breakingProgress = 0.0f;
    breakingStage = 0;
    world.clearBreakOverlay();
  }

  if (rightPressed && !mouseRightDown) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    RaycastHit hit = raycast(origin, cameraFront(), kBreakMaxDistance);
    if (hit.hit) {
      glm::ivec3 target = hit.block + hit.normal;
      if (world.inBounds(target.x, target.y, target.z) &&
          world.getBlock(target.x, target.y, target.z) == kAir &&
          !blockIntersectsPlayer(target.x, target.y, target.z)) {
        ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
        if (stack.count > 0 && stack.type != kAir) {
          world.setBlock(target.x, target.y, target.z, stack.type);
          stack.count -= 1;
          if (stack.count == 0) {
            stack.type = kAir;
          }
          refreshSelectedBlock();
          uiDirty = true;
        }
      }
    }
  }

  mouseLeftDown = leftPressed;
  mouseRightDown = rightPressed;

  (void)deltaTime;
}

void App::updatePlayer(float deltaTime) {
  const float gravity = -18.0f;
  playerVel.y += gravity * deltaTime;

  glm::vec3 pos = playerPos;

  // X axis
  pos.x += playerVel.x * deltaTime;
  if (collidesAt(pos)) {
    pos.x = playerPos.x;
    playerVel.x = 0.0f;
  }

  // Y axis
  pos.y += playerVel.y * deltaTime;
  if (collidesAt(pos)) {
    if (playerVel.y < 0.0f) {
      onGround = true;
    }
    pos.y = playerPos.y;
    playerVel.y = 0.0f;
  } else {
    onGround = false;
  }

  // Z axis
  pos.z += playerVel.z * deltaTime;
  if (collidesAt(pos)) {
    pos.z = playerPos.z;
    playerVel.z = 0.0f;
  }

  playerPos = pos;
}

void App::updateStreaming() {
  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  if (!chunkCenterValid || cx != currentChunkX || cz != currentChunkZ) {
    currentChunkX = cx;
    currentChunkZ = cz;
    chunkCenterValid = true;
    world.updateActiveChunks(cx, cz, kChunkViewRadius);
  }
}

void App::rebuildWorldMesh() {
  world.buildMesh(worldVertices, worldIndices);
  composeMeshData();
}

void App::rebuildUiMesh() {
  uiVertices.clear();
  uiIndices.clear();

  if (width <= 0 || height <= 0) {
    return;
  }

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
  const float startY = static_cast<float>(height) - kMarginBottom - kSlotSize;

  auto toNdc = [&](float px, float py) -> glm::vec2 {
    float x = (px / static_cast<float>(width)) * 2.0f - 1.0f;
    float y = 1.0f - (py / static_cast<float>(height)) * 2.0f;
    return {x, y};
  };

  auto addQuad = [&](float x, float y, float w, float h,
                     const glm::vec3& color,
                     int tile) {
    glm::vec2 p0 = toNdc(x, y);
    glm::vec2 p1 = toNdc(x + w, y);
    glm::vec2 p2 = toNdc(x + w, y + h);
    glm::vec2 p3 = toNdc(x, y + h);

    glm::vec2 uv0 = uvForTile(tile, 0.0f, 0.0f);
    glm::vec2 uv1 = uvForTile(tile, 1.0f, 0.0f);
    glm::vec2 uv2 = uvForTile(tile, 1.0f, 1.0f);
    glm::vec2 uv3 = uvForTile(tile, 0.0f, 1.0f);

    uint32_t start = static_cast<uint32_t>(uiVertices.size());
    uiVertices.push_back({{p0.x, p0.y, 0.0f}, color, uv0});
    uiVertices.push_back({{p1.x, p1.y, 0.0f}, color, uv1});
    uiVertices.push_back({{p2.x, p2.y, 0.0f}, color, uv2});
    uiVertices.push_back({{p3.x, p3.y, 0.0f}, color, uv3});

    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 1);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 3);
  };

  const int backgroundTile = tileForBlock(kStone);

  auto drawDigit = [&](int digit, float x, float y, float pixel,
                       const glm::vec3& color, int tile) {
    if (digit < 0 || digit > 9) {
      return;
    }
    for (int row = 0; row < kDigitHeight; ++row) {
      uint8_t mask = kDigitMap[digit][row];
      for (int col = 0; col < kDigitWidth; ++col) {
        int bit = kDigitWidth - 1 - col;
        if (mask & (1u << bit)) {
          addQuad(x + static_cast<float>(col) * pixel,
                  y + static_cast<float>(row) * pixel,
                  pixel,
                  pixel,
                  color,
                  tile);
        }
      }
    }
  };

  auto drawNumber = [&](int value, float right, float bottom, float pixel,
                        const glm::vec3& color, int tile) {
    if (value <= 1) {
      return;
    }
    std::string text = std::to_string(value);
    float digitW = static_cast<float>(kDigitWidth) * pixel;
    float digitH = static_cast<float>(kDigitHeight) * pixel;
    float spacing = pixel;
    float totalW = digitW * static_cast<float>(text.size()) +
                   spacing * static_cast<float>(text.size() - 1);
    float startX = right - totalW;
    float startY = bottom - digitH;

    for (size_t i = 0; i < text.size(); ++i) {
      int digit = text[i] - '0';
      drawDigit(digit,
                startX + static_cast<float>(i) * (digitW + spacing),
                startY,
                pixel,
                color,
                tile);
    }
  };

  auto drawStack = [&](const ItemStack& stack, float x, float y) {
    if (stack.count == 0 || stack.type == kAir) {
      return;
    }
    int tile = tileForBlock(stack.type);
    addQuad(x + kIconPadding,
            y + kIconPadding,
            kSlotSize - kIconPadding * 2.0f,
            kSlotSize - kIconPadding * 2.0f,
            glm::vec3(1.0f),
            tile);
    drawNumber(static_cast<int>(stack.count),
               x + kSlotSize - 4.0f,
               y + kSlotSize - 4.0f,
               3.0f,
               glm::vec3(0.95f, 0.95f, 0.98f),
               backgroundTile);
  };

  if (inventoryOpen) {
    const float gridWidth =
      kSlotSize * static_cast<float>(kInventoryCols) +
      kSlotPadding * static_cast<float>(kInventoryCols - 1);
    const float gridHeight =
      kSlotSize * static_cast<float>(kInventoryRows) +
      kSlotPadding * static_cast<float>(kInventoryRows - 1);

    float gridX = (static_cast<float>(width) - gridWidth) * 0.5f;
    float gridY = (static_cast<float>(height) - gridHeight) * 0.5f - 30.0f;
    gridY = std::clamp(gridY, 20.0f, static_cast<float>(height) - gridHeight - 20.0f);

    addQuad(gridX - kPanelPadding,
            gridY - kPanelPadding,
            gridWidth + kPanelPadding * 2.0f,
            gridHeight + kPanelPadding * 2.0f,
            glm::vec3(0.15f, 0.15f, 0.18f),
            backgroundTile);

    size_t idx = 0;
    for (int row = 0; row < kInventoryRows; ++row) {
      for (int col = 0; col < kInventoryCols; ++col) {
        float x = gridX + static_cast<float>(col) * (kSlotSize + kSlotPadding);
        float y = gridY + static_cast<float>(row) * (kSlotSize + kSlotPadding);

        addQuad(x, y, kSlotSize, kSlotSize, glm::vec3(0.25f, 0.25f, 0.28f), backgroundTile);

        if (idx < inventory.size()) {
          drawStack(inventory[idx], x, y);
        }

        ++idx;
      }
    }
  }

  for (size_t i = 0; i < hotbar.size(); ++i) {
    float x = startX + static_cast<float>(i) * (kSlotSize + kSlotPadding);
    float y = startY;

    if (static_cast<int>(i) == selectedSlot) {
      addQuad(x - kSlotBorder,
              y - kSlotBorder,
              kSlotSize + kSlotBorder * 2.0f,
              kSlotSize + kSlotBorder * 2.0f,
              glm::vec3(0.90f, 0.90f, 0.95f),
              backgroundTile);
    }

    addQuad(x, y, kSlotSize, kSlotSize, glm::vec3(0.30f, 0.30f, 0.32f), backgroundTile);

    drawStack(hotbar[i], x, y);
  }

  if (inventoryOpen && cursorStack.count > 0 && cursorStack.type != kAir) {
    float cx = cursorFbX - kSlotSize * 0.5f;
    float cy = cursorFbY - kSlotSize * 0.5f;
    drawStack(cursorStack, cx, cy);
  }
}

void App::composeMeshData() {
  meshVertices = worldVertices;
  meshIndices = worldIndices;
  worldIndexCount = static_cast<uint32_t>(worldIndices.size());

  uint32_t vertexOffset = static_cast<uint32_t>(meshVertices.size());
  meshVertices.insert(meshVertices.end(), uiVertices.begin(), uiVertices.end());
  meshIndices.reserve(meshIndices.size() + uiIndices.size());
  for (uint32_t idx : uiIndices) {
    meshIndices.push_back(idx + vertexOffset);
  }
  uiIndexCount = static_cast<uint32_t>(uiIndices.size());
}

void App::refreshSelectedBlock() {
  if (selectedSlot < 0 || selectedSlot >= static_cast<int>(hotbar.size())) {
    selectedSlot = 0;
  }
  const ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
  if (stack.count == 0 || stack.type == kAir) {
    selectedBlock = kAir;
  } else {
    selectedBlock = stack.type;
  }
}

bool App::addToInventory(uint8_t type, uint16_t count) {
  if (type == kAir || count == 0) {
    return true;
  }

  bool changed = false;

  auto mergeInto = [&](auto& slots) {
    for (auto& slot : slots) {
      if (count == 0) {
        return;
      }
      if (slot.count > 0 && slot.type == type && slot.count < kMaxStack) {
        uint16_t space = static_cast<uint16_t>(kMaxStack - slot.count);
        uint16_t toMove = std::min(space, count);
        slot.count = static_cast<uint16_t>(slot.count + toMove);
        count = static_cast<uint16_t>(count - toMove);
        changed = true;
      }
    }
  };

  auto fillEmpty = [&](auto& slots) {
    for (auto& slot : slots) {
      if (count == 0) {
        return;
      }
      if (slot.count == 0 || slot.type == kAir) {
        uint16_t toMove = std::min<uint16_t>(kMaxStack, count);
        slot.type = type;
        slot.count = toMove;
        count = static_cast<uint16_t>(count - toMove);
        changed = true;
      }
    }
  };

  mergeInto(hotbar);
  mergeInto(inventory);
  fillEmpty(hotbar);
  fillEmpty(inventory);

  if (changed) {
    refreshSelectedBlock();
    uiDirty = true;
  }

  return count == 0;
}

void App::setInventoryOpen(bool open) {
  if (inventoryOpen == open) {
    return;
  }

  inventoryOpen = open;
  if (window) {
    glfwSetInputMode(window,
                     GLFW_CURSOR,
                     inventoryOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
  }
  if (inventoryOpen && window) {
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 fb = cursorToFramebuffer(xpos, ypos);
    cursorFbX = fb.x;
    cursorFbY = fb.y;
  }
  if (!inventoryOpen) {
    if (cursorStack.count > 0 && cursorStack.type != kAir) {
      addToInventory(cursorStack.type, cursorStack.count);
      cursorStack.type = kAir;
      cursorStack.count = 0;
    }
    firstMouse = true;
  }
  uiDirty = true;
}

glm::vec2 App::cursorToFramebuffer(double xpos, double ypos) const {
  int winW = 0;
  int winH = 0;
  glfwGetWindowSize(window, &winW, &winH);
  if (winW <= 0 || winH <= 0) {
    return {0.0f, 0.0f};
  }
  float scaleX = static_cast<float>(width) / static_cast<float>(winW);
  float scaleY = static_cast<float>(height) / static_cast<float>(winH);
  return {static_cast<float>(xpos) * scaleX, static_cast<float>(ypos) * scaleY};
}

bool App::hitTestHotbar(float x, float y, int& outSlot) const {
  if (width <= 0 || height <= 0) {
    return false;
  }

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
  const float startY = static_cast<float>(height) - kMarginBottom - kSlotSize;

  if (y < startY || y > startY + kSlotSize) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(hotbar.size()); ++i) {
    float slotX = startX + static_cast<float>(i) * (kSlotSize + kSlotPadding);
    if (x >= slotX && x <= slotX + kSlotSize) {
      outSlot = i;
      return true;
    }
  }

  return false;
}

bool App::hitTestInventory(float x, float y, int& outIndex) const {
  if (!inventoryOpen || width <= 0 || height <= 0) {
    return false;
  }

  const float gridWidth =
    kSlotSize * static_cast<float>(kInventoryCols) +
    kSlotPadding * static_cast<float>(kInventoryCols - 1);
  const float gridHeight =
    kSlotSize * static_cast<float>(kInventoryRows) +
    kSlotPadding * static_cast<float>(kInventoryRows - 1);

  float gridX = (static_cast<float>(width) - gridWidth) * 0.5f;
  float gridY = (static_cast<float>(height) - gridHeight) * 0.5f - 30.0f;
  gridY = std::clamp(gridY, 20.0f, static_cast<float>(height) - gridHeight - 20.0f);

  if (x < gridX || x > gridX + gridWidth || y < gridY || y > gridY + gridHeight) {
    return false;
  }

  float localX = x - gridX;
  float localY = y - gridY;
  int col = static_cast<int>(localX / (kSlotSize + kSlotPadding));
  int row = static_cast<int>(localY / (kSlotSize + kSlotPadding));

  if (col < 0 || col >= kInventoryCols || row < 0 || row >= kInventoryRows) {
    return false;
  }

  float colX = static_cast<float>(col) * (kSlotSize + kSlotPadding);
  float rowY = static_cast<float>(row) * (kSlotSize + kSlotPadding);
  if (localX > colX + kSlotSize || localY > rowY + kSlotSize) {
    return false;
  }

  int index = row * kInventoryCols + col;
  if (index < 0 || index >= static_cast<int>(inventory.size())) {
    return false;
  }

  outIndex = index;
  return true;
}

bool App::handleInventoryClick(double xpos, double ypos, bool rightClick) {
  glm::vec2 pos = cursorToFramebuffer(xpos, ypos);
  cursorFbX = pos.x;
  cursorFbY = pos.y;
  int index = -1;
  bool onHotbar = hitTestHotbar(pos.x, pos.y, index);
  bool onInventory = false;
  if (!onHotbar) {
    onInventory = hitTestInventory(pos.x, pos.y, index);
  }

  if (!onHotbar && !onInventory) {
    return false;
  }

  bool selectionChanged = false;
  if (onHotbar) {
    if (selectedSlot != index) {
      selectedSlot = index;
      selectionChanged = true;
    }
  }

  auto clearStack = [](ItemStack& stack) {
    stack.type = kAir;
    stack.count = 0;
  };

  ItemStack& slot = onHotbar
    ? hotbar[static_cast<size_t>(index)]
    : inventory[static_cast<size_t>(index)];

  bool slotEmpty = (slot.count == 0 || slot.type == kAir);
  bool cursorEmpty = (cursorStack.count == 0 || cursorStack.type == kAir);

  if (!rightClick) {
    if (cursorEmpty) {
      if (slotEmpty) {
        return false;
      }
      cursorStack = slot;
      clearStack(slot);
      return true;
    }

    if (slotEmpty) {
      slot = cursorStack;
      clearStack(cursorStack);
      return true;
    }

    if (slot.type == cursorStack.type) {
      uint16_t space = static_cast<uint16_t>(kMaxStack - slot.count);
      if (space == 0) {
        return false;
      }
      uint16_t toMove = std::min(space, cursorStack.count);
      slot.count = static_cast<uint16_t>(slot.count + toMove);
      cursorStack.count = static_cast<uint16_t>(cursorStack.count - toMove);
      if (cursorStack.count == 0) {
        cursorStack.type = kAir;
      }
      return true;
    }

    std::swap(slot, cursorStack);
    return true;
  }

  if (cursorEmpty) {
    if (slotEmpty) {
      return false;
    }
    uint16_t take = static_cast<uint16_t>((slot.count + 1) / 2);
    cursorStack.type = slot.type;
    cursorStack.count = take;
    slot.count = static_cast<uint16_t>(slot.count - take);
    if (slot.count == 0) {
      slot.type = kAir;
    }
    return true;
  }

  if (slotEmpty) {
    slot.type = cursorStack.type;
    slot.count = 1;
    cursorStack.count = static_cast<uint16_t>(cursorStack.count - 1);
    if (cursorStack.count == 0) {
      cursorStack.type = kAir;
    }
    return true;
  }

  if (slot.type == cursorStack.type && slot.count < kMaxStack) {
    slot.count = static_cast<uint16_t>(slot.count + 1);
    cursorStack.count = static_cast<uint16_t>(cursorStack.count - 1);
    if (cursorStack.count == 0) {
      cursorStack.type = kAir;
    }
    return true;
  }

  return selectionChanged;
}

bool App::collidesAt(const glm::vec3& pos) const {
  const float halfWidth = 0.3f;
  const float height = 1.8f;

  glm::vec3 min = {pos.x - halfWidth, pos.y, pos.z - halfWidth};
  glm::vec3 max = {pos.x + halfWidth, pos.y + height, pos.z + halfWidth};

  int minX = static_cast<int>(std::floor(min.x));
  int maxX = static_cast<int>(std::floor(max.x));
  int minY = static_cast<int>(std::floor(min.y));
  int maxY = static_cast<int>(std::floor(max.y));
  int minZ = static_cast<int>(std::floor(min.z));
  int maxZ = static_cast<int>(std::floor(max.z));

  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z)) {
          return true;
        }
        if (world.getBlock(x, y, z) != kAir) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::blockIntersectsPlayer(int x, int y, int z) const {
  const float halfWidth = 0.3f;
  const float height = 1.8f;
  glm::vec3 min = {playerPos.x - halfWidth, playerPos.y, playerPos.z - halfWidth};
  glm::vec3 max = {playerPos.x + halfWidth, playerPos.y + height, playerPos.z + halfWidth};

  return x + 1.0f > min.x && x < max.x &&
         y + 1.0f > min.y && y < max.y &&
         z + 1.0f > min.z && z < max.z;
}

glm::vec3 App::cameraFront() const {
  glm::vec3 front;
  front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  front.y = std::sin(glm::radians(pitch));
  front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  return glm::normalize(front);
}

App::RaycastHit App::raycast(const glm::vec3& origin,
                              const glm::vec3& dir,
                              float maxDist) const {
  RaycastHit result;
  glm::vec3 rayDir = glm::normalize(dir);

  glm::ivec3 step(0);
  glm::vec3 tMax(0.0f);
  glm::vec3 tDelta(0.0f);

  auto intBound = [](float s, float ds) {
    if (ds > 0) {
      return (std::floor(s + 1.0f) - s) / ds;
    }
    if (ds < 0) {
      return (s - std::floor(s)) / -ds;
    }
    return std::numeric_limits<float>::infinity();
  };

  glm::ivec3 voxel(static_cast<int>(std::floor(origin.x)),
                   static_cast<int>(std::floor(origin.y)),
                   static_cast<int>(std::floor(origin.z)));

  step.x = (rayDir.x > 0) ? 1 : (rayDir.x < 0 ? -1 : 0);
  step.y = (rayDir.y > 0) ? 1 : (rayDir.y < 0 ? -1 : 0);
  step.z = (rayDir.z > 0) ? 1 : (rayDir.z < 0 ? -1 : 0);

  tMax.x = intBound(origin.x, rayDir.x);
  tMax.y = intBound(origin.y, rayDir.y);
  tMax.z = intBound(origin.z, rayDir.z);

  tDelta.x = (rayDir.x == 0.0f) ? std::numeric_limits<float>::infinity()
                                : std::abs(1.0f / rayDir.x);
  tDelta.y = (rayDir.y == 0.0f) ? std::numeric_limits<float>::infinity()
                                : std::abs(1.0f / rayDir.y);
  tDelta.z = (rayDir.z == 0.0f) ? std::numeric_limits<float>::infinity()
                                : std::abs(1.0f / rayDir.z);

  glm::ivec3 lastStep(0);
  float dist = 0.0f;

  while (dist <= maxDist) {
    if (!world.inBounds(voxel.x, voxel.y, voxel.z)) {
      return result;
    }

    if (world.getBlock(voxel.x, voxel.y, voxel.z) != kAir) {
      result.hit = true;
      result.block = voxel;
      result.normal = -lastStep;
      return result;
    }

    if (tMax.x < tMax.y) {
      if (tMax.x < tMax.z) {
        voxel.x += step.x;
        dist = tMax.x;
        tMax.x += tDelta.x;
        lastStep = {step.x, 0, 0};
      } else {
        voxel.z += step.z;
        dist = tMax.z;
        tMax.z += tDelta.z;
        lastStep = {0, 0, step.z};
      }
    } else {
      if (tMax.y < tMax.z) {
        voxel.y += step.y;
        dist = tMax.y;
        tMax.y += tDelta.y;
        lastStep = {0, step.y, 0};
      } else {
        voxel.z += step.z;
        dist = tMax.z;
        tMax.z += tDelta.z;
        lastStep = {0, 0, step.z};
      }
    }
  }

  return result;
}

void App::cleanup() {
  world.save("world.bin");
  vk.cleanup();

  if (window) {
    glfwDestroyWindow(window);
    window = nullptr;
  }

  glfwTerminate();
}

void App::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }

  if (width == 0 || height == 0) {
    return;
  }

  app->width = width;
  app->height = height;
  app->framebufferResized = true;
  app->uiDirty = true;
}

void App::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }

  if (app->inventoryOpen) {
    glm::vec2 fb = app->cursorToFramebuffer(xpos, ypos);
    app->cursorFbX = fb.x;
    app->cursorFbY = fb.y;
    if (app->cursorStack.count > 0 && app->cursorStack.type != kAir) {
      app->uiDirty = true;
    }
    app->lastMouseX = static_cast<float>(xpos);
    app->lastMouseY = static_cast<float>(ypos);
    return;
  }

  if (app->firstMouse) {
    app->lastMouseX = static_cast<float>(xpos);
    app->lastMouseY = static_cast<float>(ypos);
    app->firstMouse = false;
  }

  float xoffset = static_cast<float>(xpos) - app->lastMouseX;
  float yoffset = app->lastMouseY - static_cast<float>(ypos);
  app->lastMouseX = static_cast<float>(xpos);
  app->lastMouseY = static_cast<float>(ypos);

  const float sensitivity = 0.1f;
  xoffset *= sensitivity;
  yoffset *= sensitivity;

  app->yaw += xoffset;
  app->pitch += yoffset;

  if (app->pitch > 89.0f) {
    app->pitch = 89.0f;
  }
  if (app->pitch < -89.0f) {
    app->pitch = -89.0f;
  }
}
