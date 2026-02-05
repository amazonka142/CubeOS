#pragma once

#include "vk_context.hpp"
#include "world.hpp"

#include <glm/glm.hpp>
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
  bool collidesAt(const glm::vec3& pos) const;
  glm::vec3 cameraFront() const;
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

  glm::vec3 playerPos{8.0f, 30.0f, 8.0f};
  glm::vec3 playerVel{0.0f};
  bool onGround = false;

  float yaw = -90.0f;
  float pitch = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
};
