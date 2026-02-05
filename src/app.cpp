#include "app.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

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
  rebuildWorldMesh();
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
    updatePlayer(deltaTime);

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
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
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

  if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
    selectedBlock = kGrass;
  } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
    selectedBlock = kDirt;
  } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
    selectedBlock = kStone;
  }

  bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  if (leftPressed && !mouseLeftDown) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    RaycastHit hit = raycast(origin, cameraFront(), 6.0f);
    if (hit.hit) {
      world.setBlock(hit.block.x, hit.block.y, hit.block.z, kAir);
      rebuildWorldMesh();
      vk.updateMesh(worldVertices, worldIndices);
    }
  }

  if (rightPressed && !mouseRightDown) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    RaycastHit hit = raycast(origin, cameraFront(), 6.0f);
    if (hit.hit) {
      glm::ivec3 target = hit.block + hit.normal;
      if (world.inBounds(target.x, target.y, target.z) &&
          world.getBlock(target.x, target.y, target.z) == kAir &&
          !blockIntersectsPlayer(target.x, target.y, target.z)) {
        world.setBlock(target.x, target.y, target.z, selectedBlock);
        rebuildWorldMesh();
        vk.updateMesh(worldVertices, worldIndices);
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

void App::rebuildWorldMesh() {
  world.buildMesh(worldVertices, worldIndices);
  vk.setMeshData(worldVertices, worldIndices);
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
}

void App::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
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
