#pragma once

#include "vk_context.hpp"
#include "world.hpp"

#include <glm/glm.hpp>
#include <array>
#include <vector>

struct GLFWwindow;

class App {
public:
  void run();

private:
  void initWindow();
  void initVulkan();
  void mainLoop();
  void processInput(float deltaTime);
  void updatePlayer(float deltaTime);
  void rebuildWorldMesh();
  void rebuildUiMesh();
  void composeMeshData();
  bool collidesAt(const glm::vec3& pos) const;
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
  std::vector<Vertex> worldVertices;
  std::vector<uint32_t> worldIndices;
  std::vector<Vertex> uiVertices;
  std::vector<uint32_t> uiIndices;
  std::vector<Vertex> meshVertices;
  std::vector<uint32_t> meshIndices;
  uint32_t worldIndexCount = 0;
  uint32_t uiIndexCount = 0;

  glm::vec3 playerPos{8.0f, 30.0f, 8.0f};
  glm::vec3 playerVel{0.0f};
  bool onGround = false;
  uint8_t selectedBlock = kGrass;
  std::array<uint8_t, 9> hotbar = {
    kGrass, kDirt, kStone, kGrass, kDirt, kStone, kGrass, kDirt, kStone
  };
  int selectedSlot = 0;
  bool mouseLeftDown = false;
  bool mouseRightDown = false;
  bool uiDirty = true;

  float yaw = -90.0f;
  float pitch = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
};
