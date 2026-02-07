#pragma once

#include "vk_context.hpp"
#include "world.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <array>
#include <vector>

struct GLFWwindow;

struct ItemStack {
  uint8_t type = kAir;
  uint16_t count = 0;
};

class App {
public:
  void run();

private:
  void refreshSelectedBlock();
  bool addToInventory(uint8_t type, uint16_t count);
  void initWindow();
  void initVulkan();
  void mainLoop();
  void processInput(float deltaTime);
  void updatePlayer(float deltaTime);
  void rebuildWorldMesh();
  void rebuildUiMesh();
  void composeMeshData();
  void updateStreaming();
  void setInventoryOpen(bool open);
  bool handleInventoryClick(double xpos, double ypos, bool rightClick);
  glm::vec2 cursorToFramebuffer(double xpos, double ypos) const;
  bool hitTestHotbar(float x, float y, int& outSlot) const;
  bool hitTestInventory(float x, float y, int& outIndex) const;
  bool collidesAt(const glm::vec3& pos) const;
  bool intersectsWaterAt(const glm::vec3& pos) const;
  bool blockIntersectsPlayer(int x, int y, int z) const;
  glm::vec3 cameraFront() const;
  struct RaycastHit {
    bool hit = false;
    glm::ivec3 block{};
    glm::ivec3 normal{};
  };
  RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const;
  void cleanup();
  static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
  static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

  GLFWwindow* window = nullptr;
  int width = 1280;
  int height = 720;
  bool framebufferResized = false;

  VulkanContext vk;

  World world{3, 3};
  std::vector<Vertex> skyVertices;
  std::vector<uint32_t> skyIndices;
  std::vector<Vertex> worldVertices;
  std::vector<uint32_t> worldIndices;
  std::vector<Vertex> uiVertices;
  std::vector<uint32_t> uiIndices;
  std::vector<Vertex> meshVertices;
  std::vector<uint32_t> meshIndices;
  uint32_t skyIndexCount = 0;
  uint32_t worldIndexCount = 0;
  uint32_t uiIndexCount = 0;

  glm::vec3 playerPos{8.0f, 30.0f, 8.0f};
  glm::vec3 playerVel{0.0f};
  bool onGround = false;
  int currentChunkX = 0;
  int currentChunkZ = 0;
  bool chunkCenterValid = false;
  bool breakingActive = false;
  glm::ivec3 breakingBlock{};
  float breakingProgress = 0.0f;
  int breakingStage = 0;
  uint8_t selectedBlock = kAir;
  std::array<ItemStack, 9> hotbar{};
  std::array<ItemStack, 27> inventory{};
  ItemStack cursorStack{};
  float cursorFbX = 0.0f;
  float cursorFbY = 0.0f;
  int selectedSlot = 0;
  bool inventoryOpen = false;
  bool tabDown = false;
  bool escDown = false;
  bool mouseLeftDown = false;
  bool mouseRightDown = false;
  bool uiDirty = true;
  float waterSimAccumulator = 0.0f;

  float yaw = -90.0f;
  float pitch = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
};
