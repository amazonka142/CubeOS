#pragma once

#include "vk_context.hpp"

struct GLFWwindow;

class App {
public:
  void run();

private:
  void initWindow();
  void initVulkan();
  void mainLoop();
  void cleanup();
  static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

  GLFWwindow* window = nullptr;
  int width = 1280;
  int height = 720;
  bool framebufferResized = false;

  VulkanContext vk;
};
