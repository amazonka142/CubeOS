#include "vk_context.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr int kMaxFramesInFlight = 2;
constexpr int kChunkSizeBlocks = 16;
constexpr int kSectionKeyCoordBits = 30;
constexpr int64_t kSectionKeyCoordBias = static_cast<int64_t>(1) << (kSectionKeyCoordBits - 1);
constexpr uint64_t kSectionKeyCoordMask = (static_cast<uint64_t>(1) << kSectionKeyCoordBits) - 1u;
constexpr float kChunkDrawDistanceBlocks = 220.0f;
constexpr float kChunkDrawDistanceBlocksSq = kChunkDrawDistanceBlocks * kChunkDrawDistanceBlocks;

#ifdef CUBEOS_ENABLE_VALIDATION
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

const std::vector<const char*> kValidationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> kDeviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
  "VK_KHR_portability_subset",
#endif
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
  glm::vec4 params{0.0f};
  glm::vec4 cameraData{0.0f};
  glm::vec4 weatherData{0.0f};
};

const auto kStartTime = std::chrono::high_resolution_clock::now();

bool unpackSectionKey(uint64_t key, int& outCx, int& outCz, int& outSectionY) {
  uint64_t ux = (key >> 34) & kSectionKeyCoordMask;
  uint64_t uz = (key >> 4) & kSectionKeyCoordMask;
  uint64_t us = key & 0xFu;
  outCx = static_cast<int>(static_cast<int64_t>(ux) - kSectionKeyCoordBias);
  outCz = static_cast<int>(static_cast<int64_t>(uz) - kSectionKeyCoordBias);
  outSectionY = static_cast<int>(us);
  return outSectionY >= 0 && outSectionY < 16;
}

bool isSpecialWorldMeshKey(uint64_t key) {
  // Reserve the top key range for non-chunk meshes (dropped items, debug overlays, etc).
  return key >= (std::numeric_limits<uint64_t>::max() - 16ull);
}

void checkVk(VkResult result, const char* message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message);
  }
}


VkResult CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
  const VkAllocationCallbacks* allocator,
  VkDebugUtilsMessengerEXT* debugMessenger) {
  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (func) {
    return func(instance, createInfo, allocator, debugMessenger);
  }
  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT debugMessenger,
  const VkAllocationCallbacks* allocator) {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (func) {
    func(instance, debugMessenger, allocator);
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
  void* userData) {
  (void)messageSeverity;
  (void)messageType;
  (void)userData;
  std::cerr << "validation layer: " << callbackData->pMessage << "\n";
  return VK_FALSE;
}

std::string getExecutableDir() {
#ifdef _WIN32
  char buffer[MAX_PATH];
  DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  if (length == 0) {
    return ".";
  }
  std::string path(buffer, length);
  size_t pos = path.find_last_of("\\/");
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
#elif defined(__APPLE__)
  std::string path;

  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&getExecutableDir), &info) != 0 && info.dli_fname) {
    path = info.dli_fname;
  }

  if (path.empty()) {
    char pathbuf[PATH_MAX];
    uint32_t size = static_cast<uint32_t>(sizeof(pathbuf));
    if (_NSGetExecutablePath(pathbuf, &size) == 0) {
      path = pathbuf;
    } else {
      std::string temp(size, '\0');
      if (_NSGetExecutablePath(temp.data(), &size) != 0) {
        return ".";
      }
      temp.resize(std::strlen(temp.c_str()));
      path = temp;
    }
  }

  char resolved[PATH_MAX];
  if (realpath(path.c_str(), resolved)) {
    path = resolved;
  }
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
#else
  char buffer[4096];
  ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length <= 0) {
    return ".";
  }
  buffer[length] = '\0';
  std::string path(buffer);
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
#endif
}

std::string buildShaderPath(const char* filename) {
  const char* envDir = std::getenv("CUBEOS_SHADER_DIR");
  if (envDir && envDir[0] != '\0') {
    std::string envPath = std::string(envDir) + "/" + filename;
    std::ifstream envFile(envPath, std::ios::binary);
    if (envFile.is_open()) {
      return envPath;
    }
  }

  std::string base = getExecutableDir();
  std::string path = base + "/shaders/" + filename;
  std::ifstream direct(path, std::ios::binary);
  if (direct.is_open()) {
    return path;
  }
  std::string fallback = base + "/../shaders/" + filename;
  std::ifstream fallbackFile(fallback, std::ios::binary);
  if (fallbackFile.is_open()) {
    return fallback;
  }
  std::string cwdFallback = std::string("./shaders/") + filename;
  std::ifstream cwdFile(cwdFallback, std::ios::binary);
  if (cwdFile.is_open()) {
    return cwdFallback;
  }
  return path;
}

#ifdef __APPLE__
void* gBundledVulkanLibHandle = nullptr;

void configureBundledVulkanEnvironment() {
  std::string base = getExecutableDir();
  std::string bundledIcd = base + "/../Resources/vulkan/icd.d/MoltenVK_icd.json";

  std::ifstream icdFile(bundledIcd, std::ios::binary);
  if (!icdFile.is_open()) {
    return;
  }

  if (!std::getenv("VK_ICD_FILENAMES")) {
    setenv("VK_ICD_FILENAMES", bundledIcd.c_str(), 0);
  }
}

PFN_vkGetInstanceProcAddr loadBundledGetInstanceProcAddr() {
  std::string base = getExecutableDir();
  std::array<std::string, 3> candidates = {
    base + "/../Frameworks/libvulkan.1.dylib",
    std::string("libvulkan.1.dylib"),
    std::string("/usr/local/lib/libvulkan.1.dylib")
  };

  for (const std::string& path : candidates) {
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      continue;
    }
    auto proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      dlsym(handle, "vkGetInstanceProcAddr"));
    if (proc) {
      gBundledVulkanLibHandle = handle;
      return proc;
    }
    dlclose(handle);
  }

  return nullptr;
}
#endif

} // namespace

void VulkanContext::init(GLFWwindow* windowIn, bool* framebufferResizedFlagIn) {
  window = windowIn;
  framebufferResizedFlag = framebufferResizedFlagIn;

#ifdef __APPLE__
  configureBundledVulkanEnvironment();
  PFN_vkGetInstanceProcAddr getInstanceProc = loadBundledGetInstanceProcAddr();
  if (!getInstanceProc) {
    throw std::runtime_error("Failed to locate vkGetInstanceProcAddr on macOS.");
  }
  volkInitializeCustom(getInstanceProc);
  if (volkGetInstanceVersion() == 0) {
    throw std::runtime_error("Failed to initialize Vulkan loader (volk) on macOS.");
  }
#else
  if (volkInitialize() != VK_SUCCESS) {
    throw std::runtime_error("Failed to initialize Vulkan loader (volk).");
  }
#endif

  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createDescriptorSetLayout();
  createSwapchain();
  createImageViews();
  createRenderPass();
  createGraphicsPipeline();
  createDepthResources();
  createFramebuffers();
  createCommandPool();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  createVertexBuffer();
  createIndexBuffer();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
  createSyncObjects();
}

void VulkanContext::cleanup() {
  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
  }

  cleanupSwapchain();

  if (textureSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, textureSampler, nullptr);
    textureSampler = VK_NULL_HANDLE;
  }
  if (textureImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, textureImageView, nullptr);
    textureImageView = VK_NULL_HANDLE;
  }
  if (textureImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, textureImage, nullptr);
    textureImage = VK_NULL_HANDLE;
  }
  if (textureImageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, textureImageMemory, nullptr);
    textureImageMemory = VK_NULL_HANDLE;
  }

  if (indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, indexBuffer, nullptr);
    indexBuffer = VK_NULL_HANDLE;
  }
  if (indexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, indexBufferMemory, nullptr);
    indexBufferMemory = VK_NULL_HANDLE;
  }

  clearWorldChunkMeshes();

  if (vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vertexBuffer = VK_NULL_HANDLE;
  }
  if (vertexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vertexBufferMemory = VK_NULL_HANDLE;
  }

  if (descriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < imageAvailableSemaphores.size(); ++i) {
    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
    vkDestroyFence(device, inFlightFences[i], nullptr);
  }

  for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
  }

  if (commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, commandPool, nullptr);
    commandPool = VK_NULL_HANDLE;
  }

  if (device != VK_NULL_HANDLE) {
    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
  }

  if (kEnableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    debugMessenger = VK_NULL_HANDLE;
  }

  if (surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
  }

  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
  }

#ifdef __APPLE__
  if (gBundledVulkanLibHandle) {
    dlclose(gBundledVulkanLibHandle);
    gBundledVulkanLibHandle = nullptr;
  }
#endif
}

void VulkanContext::drawFrame() {
  vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex = 0;
  VkResult result = vkAcquireNextImageKHR(
    device,
    swapchain,
    UINT64_MAX,
    imageAvailableSemaphores[currentFrame],
    VK_NULL_HANDLE,
    &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire swapchain image.");
  }

  if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }

  imagesInFlight[imageIndex] = inFlightFences[currentFrame];

  updateUniformBuffer(imageIndex);

  vkResetFences(device, 1, &inFlightFences[currentFrame]);
  vkResetCommandBuffer(commandBuffers[imageIndex], 0);
  recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

  VkSemaphore imageAvailable = imageAvailableSemaphores[currentFrame];
  VkSemaphore renderFinished = renderFinishedSemaphores[imageIndex];
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAvailable;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinished;

  checkVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]),
          "Failed to submit draw command buffer.");

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinished;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(presentQueue, &presentInfo);

  bool resized = framebufferResizedFlag && *framebufferResizedFlag;
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
    if (framebufferResizedFlag) {
      *framebufferResizedFlag = false;
    }
    recreateSwapchain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to present swapchain image.");
  }

  currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
}

void VulkanContext::waitIdle() {
  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
  }
}

void VulkanContext::setMeshData(const std::vector<Vertex>& vertices,
                                const std::vector<uint32_t>& indices,
                                uint32_t skyIndexCountIn,
                                uint32_t worldIndexCountIn,
                                uint32_t uiIndexCountIn) {
  meshVertices = vertices;
  meshIndices = indices;
  skyIndexCount = skyIndexCountIn;
  worldIndexCount = worldIndexCountIn;
  uiIndexCount = uiIndexCountIn;
}

void VulkanContext::updateMesh(const std::vector<Vertex>& vertices,
                               const std::vector<uint32_t>& indices,
                               uint32_t skyIndexCountIn,
                               uint32_t worldIndexCountIn,
                               uint32_t uiIndexCountIn) {
  meshVertices = vertices;
  meshIndices = indices;
  skyIndexCount = skyIndexCountIn;
  worldIndexCount = worldIndexCountIn;
  uiIndexCount = uiIndexCountIn;
  if (device == VK_NULL_HANDLE) {
    return;
  }

  vkDeviceWaitIdle(device);

  if (vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vertexBuffer = VK_NULL_HANDLE;
  }
  if (vertexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vertexBufferMemory = VK_NULL_HANDLE;
  }
  if (indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, indexBuffer, nullptr);
    indexBuffer = VK_NULL_HANDLE;
  }
  if (indexBufferMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, indexBufferMemory, nullptr);
    indexBufferMemory = VK_NULL_HANDLE;
  }

  createVertexBuffer();
  createIndexBuffer();
}

bool VulkanContext::uploadChunkGpuMesh(const WorldChunkMeshUpload& upload, ChunkGpuMesh& outMesh) {
  outMesh.chunkX = 0;
  outMesh.chunkZ = 0;
  outMesh.alwaysVisible = isSpecialWorldMeshKey(upload.key);
  if (!outMesh.alwaysVisible) {
    int sectionY = 0;
    unpackSectionKey(upload.key, outMesh.chunkX, outMesh.chunkZ, sectionY);
    (void)sectionY;
  }

  if (upload.vertices.empty() || upload.indices.empty()) {
    outMesh.indexCount = 0;
    return true;
  }

  VkDeviceSize vertexSize = sizeof(Vertex) * upload.vertices.size();
  VkDeviceSize indexSize = sizeof(uint32_t) * upload.indices.size();
  if (vertexSize == 0 || indexSize == 0) {
    outMesh.indexCount = 0;
    return true;
  }

  createBuffer(vertexSize,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               outMesh.vertexBuffer,
               outMesh.vertexMemory);
  createBuffer(indexSize,
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               outMesh.indexBuffer,
               outMesh.indexMemory);

  void* vertexData = nullptr;
  vkMapMemory(device, outMesh.vertexMemory, 0, vertexSize, 0, &vertexData);
  std::memcpy(vertexData, upload.vertices.data(), static_cast<size_t>(vertexSize));
  vkUnmapMemory(device, outMesh.vertexMemory);

  void* indexData = nullptr;
  vkMapMemory(device, outMesh.indexMemory, 0, indexSize, 0, &indexData);
  std::memcpy(indexData, upload.indices.data(), static_cast<size_t>(indexSize));
  vkUnmapMemory(device, outMesh.indexMemory);

  outMesh.indexCount = static_cast<uint32_t>(upload.indices.size());
  return true;
}

void VulkanContext::destroyChunkGpuMesh(ChunkGpuMesh& mesh) {
  if (mesh.vertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
    mesh.vertexBuffer = VK_NULL_HANDLE;
  }
  if (mesh.vertexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, mesh.vertexMemory, nullptr);
    mesh.vertexMemory = VK_NULL_HANDLE;
  }
  if (mesh.indexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
    mesh.indexBuffer = VK_NULL_HANDLE;
  }
  if (mesh.indexMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, mesh.indexMemory, nullptr);
    mesh.indexMemory = VK_NULL_HANDLE;
  }
  mesh.indexCount = 0;
}

void VulkanContext::retireChunkGpuMesh(ChunkGpuMesh& mesh) {
  if (mesh.vertexBuffer == VK_NULL_HANDLE && mesh.vertexMemory == VK_NULL_HANDLE &&
      mesh.indexBuffer == VK_NULL_HANDLE && mesh.indexMemory == VK_NULL_HANDLE) {
    return;
  }
  retiredWorldChunkMeshes.push_back(mesh);
  mesh = {};
}

void VulkanContext::collectRetiredWorldChunkMeshes(bool force) {
  if (retiredWorldChunkMeshes.empty()) {
    return;
  }
  constexpr size_t kRetiredMeshCleanupThreshold = 1024;
  if (!force && retiredWorldChunkMeshes.size() < kRetiredMeshCleanupThreshold) {
    return;
  }
  vkDeviceWaitIdle(device);
  for (ChunkGpuMesh& mesh : retiredWorldChunkMeshes) {
    destroyChunkGpuMesh(mesh);
  }
  retiredWorldChunkMeshes.clear();
}

void VulkanContext::clearWorldChunkMeshes() {
  for (auto& entry : worldChunkMeshes) {
    destroyChunkGpuMesh(entry.second);
  }
  worldChunkMeshes.clear();
  worldChunkDrawOrder.clear();
  worldChunkDrawOrderIndex.clear();
  for (ChunkGpuMesh& mesh : retiredWorldChunkMeshes) {
    destroyChunkGpuMesh(mesh);
  }
  retiredWorldChunkMeshes.clear();
}

void VulkanContext::setWorldChunkMeshes(const std::vector<WorldChunkMeshUpload>& uploads) {
  if (device == VK_NULL_HANDLE) {
    return;
  }

  vkDeviceWaitIdle(device);
  clearWorldChunkMeshes();

  worldChunkDrawOrder.reserve(uploads.size());
  worldChunkDrawOrderIndex.reserve(uploads.size());
  for (const WorldChunkMeshUpload& upload : uploads) {
    ChunkGpuMesh mesh{};
    uploadChunkGpuMesh(upload, mesh);
    if (mesh.indexCount == 0) {
      continue;
    }
    worldChunkMeshes[upload.key] = mesh;
    worldChunkDrawOrderIndex[upload.key] = worldChunkDrawOrder.size();
    worldChunkDrawOrder.push_back(upload.key);
  }
}

void VulkanContext::updateWorldChunkMeshes(const std::vector<WorldChunkMeshUpload>& uploads,
                                           const std::vector<uint64_t>& removedKeys) {
  if (device == VK_NULL_HANDLE) {
    return;
  }
  if (uploads.empty() && removedKeys.empty()) {
    return;
  }

  auto removeDrawKey = [&](uint64_t key) {
    auto idxIt = worldChunkDrawOrderIndex.find(key);
    if (idxIt == worldChunkDrawOrderIndex.end()) {
      return;
    }

    size_t index = idxIt->second;
    size_t lastIndex = worldChunkDrawOrder.size() - 1;
    if (index != lastIndex) {
      uint64_t swappedKey = worldChunkDrawOrder[lastIndex];
      worldChunkDrawOrder[index] = swappedKey;
      worldChunkDrawOrderIndex[swappedKey] = index;
    }
    worldChunkDrawOrder.pop_back();
    worldChunkDrawOrderIndex.erase(idxIt);
  };

  for (uint64_t key : removedKeys) {
    auto it = worldChunkMeshes.find(key);
    if (it == worldChunkMeshes.end()) {
      continue;
    }
    retireChunkGpuMesh(it->second);
    worldChunkMeshes.erase(it);
    removeDrawKey(key);
  }

  for (const WorldChunkMeshUpload& upload : uploads) {
    auto existing = worldChunkMeshes.find(upload.key);
    if (existing != worldChunkMeshes.end()) {
      retireChunkGpuMesh(existing->second);
      worldChunkMeshes.erase(existing);
      removeDrawKey(upload.key);
    }

    ChunkGpuMesh mesh{};
    uploadChunkGpuMesh(upload, mesh);
    if (mesh.indexCount == 0) {
      continue;
    }
    worldChunkMeshes[upload.key] = mesh;
    worldChunkDrawOrderIndex[upload.key] = worldChunkDrawOrder.size();
    worldChunkDrawOrder.push_back(upload.key);
  }

  collectRetiredWorldChunkMeshes(false);
}

void VulkanContext::setCameraMatrices(const glm::mat4& view, const glm::mat4& proj) {
  cameraView = view;
  cameraProj = proj;
}

void VulkanContext::setCameraWorldState(const glm::vec3& eyePosition, bool underwater) {
  cameraWorldPos = eyePosition;
  cameraUnderwater = underwater;
}

void VulkanContext::setEnvironmentState(float daylight, float weatherIntensity, float dayCycleTime) {
  environmentDaylight = std::clamp(daylight, 0.0f, 1.0f);
  environmentWeatherIntensity = std::clamp(weatherIntensity, 0.0f, 1.0f);
  float wrappedDayCycle = dayCycleTime - std::floor(dayCycleTime);
  if (wrappedDayCycle < 0.0f) {
    wrappedDayCycle += 1.0f;
  }
  environmentDayCycleTime = wrappedDayCycle;
}

void VulkanContext::createInstance() {
  if (kEnableValidationLayers && !checkValidationLayerSupport()) {
    throw std::runtime_error("Validation layers requested, but not available.");
  }

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "CubeOS Voxel";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 1);
  appInfo.pEngineName = "CubeOS";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 1);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
#ifdef __APPLE__
  #ifndef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
  #define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
  #endif
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  auto extensions = getRequiredExtensions();
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  if (kEnableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
    createInfo.ppEnabledLayerNames = kValidationLayers.data();
    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = &debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  checkVk(vkCreateInstance(&createInfo, nullptr, &instance),
          "Failed to create Vulkan instance.");
  volkLoadInstance(instance);
}

void VulkanContext::setupDebugMessenger() {
  if (!kEnableValidationLayers) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  populateDebugMessengerCreateInfo(createInfo);
  checkVk(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger),
          "Failed to set up debug messenger.");
}

void VulkanContext::createSurface() {
  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface.");
  }
}

void VulkanContext::pickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("Failed to find GPUs with Vulkan support.");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  for (const auto& deviceCandidate : devices) {
    if (isDeviceSuitable(deviceCandidate)) {
      physicalDevice = deviceCandidate;
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find a suitable GPU.");
  }
}

void VulkanContext::createLogicalDevice() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {
    indices.graphicsFamily.value(),
    indices.presentFamily.value()
  };

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

  if (kEnableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
    createInfo.ppEnabledLayerNames = kValidationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  checkVk(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
          "Failed to create logical device.");
  volkLoadDevice(device);

  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanContext::createDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  uboLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  samplerLayoutBinding.pImmutableSamplers = nullptr;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
    uboLayoutBinding,
    samplerLayoutBinding
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  checkVk(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout),
          "Failed to create descriptor set layout.");
}

void VulkanContext::createSwapchain() {
  SwapchainSupportDetails support = querySwapchainSupport(physicalDevice);
  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
  VkExtent2D extent = chooseSwapExtent(support.capabilities);

  uint32_t imageCount = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      imageCount > support.capabilities.maxImageCount) {
    imageCount = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
  uint32_t queueFamilyIndices[] = {
    indices.graphicsFamily.value(),
    indices.presentFamily.value()
  };

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if ((support.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0) {
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    createInfo.preTransform = support.capabilities.currentTransform;
  }
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  checkVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain),
          "Failed to create swapchain.");

  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

  swapchainImageFormat = surfaceFormat.format;
  swapchainExtent = extent;
}

void VulkanContext::createImageViews() {
  swapchainImageViews.resize(swapchainImages.size());

  for (size_t i = 0; i < swapchainImages.size(); ++i) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapchainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapchainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    checkVk(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]),
            "Failed to create image views.");
  }
}

void VulkanContext::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = findDepthFormat();
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  checkVk(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass),
          "Failed to create render pass.");
}

std::vector<char> VulkanContext::readFile(const std::string& path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + path);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
  file.close();

  return buffer;
}

VkShaderModule VulkanContext::createShaderModule(const std::vector<char>& code) const {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  checkVk(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
          "Failed to create shader module.");
  return shaderModule;
}

void VulkanContext::createGraphicsPipeline() {
  auto vertShaderCode = readFile(buildShaderPath("cube.vert.spv"));
  auto fragShaderCode = readFile(buildShaderPath("cube.frag.spv"));
  auto uiVertShaderCode = readFile(buildShaderPath("ui.vert.spv"));

  VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
  VkShaderModule uiVertShaderModule = createShaderModule(uiVertShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo uiVertShaderStageInfo{};
  uiVertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  uiVertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  uiVertShaderStageInfo.module = uiVertShaderModule;
  uiVertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {
    vertShaderStageInfo,
    fragShaderStageInfo
  };

  VkPipelineShaderStageCreateInfo uiShaderStages[] = {
    uiVertShaderStageInfo,
    fragShaderStageInfo
  };

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineRasterizationStateCreateInfo rasterizerUi = rasterizer;
  rasterizerUi.cullMode = VK_CULL_MODE_NONE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkPipelineDepthStencilStateCreateInfo depthStencilUi = depthStencil;
  depthStencilUi.depthTestEnable = VK_FALSE;
  depthStencilUi.depthWriteEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  std::array<VkDynamicState, 2> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  checkVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
          "Failed to create pipeline layout.");

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  checkVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                    nullptr, &graphicsPipeline),
          "Failed to create graphics pipeline.");

  VkGraphicsPipelineCreateInfo uiPipelineInfo = pipelineInfo;
  uiPipelineInfo.pStages = uiShaderStages;
  uiPipelineInfo.pRasterizationState = &rasterizerUi;
  uiPipelineInfo.pDepthStencilState = &depthStencilUi;

  checkVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &uiPipelineInfo,
                                    nullptr, &uiPipeline),
          "Failed to create UI graphics pipeline.");

  vkDestroyShaderModule(device, fragShaderModule, nullptr);
  vkDestroyShaderModule(device, vertShaderModule, nullptr);
  vkDestroyShaderModule(device, uiVertShaderModule, nullptr);
}

void VulkanContext::createDepthResources() {
  VkFormat depthFormat = findDepthFormat();

  createImage(swapchainExtent.width,
              swapchainExtent.height,
              depthFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
              depthImage,
              depthImageMemory);

  depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanContext::createTextureImage() {
  const int texWidth = kAtlasTileSize * kAtlasCols;
  const int texHeight = kAtlasTileSize * kAtlasRows;
  std::vector<uint8_t> pixels(static_cast<size_t>(texWidth * texHeight * 4), 0);

  auto putPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    size_t idx = static_cast<size_t>((y * texWidth + x) * 4);
    pixels[idx + 0] = r;
    pixels[idx + 1] = g;
    pixels[idx + 2] = b;
    pixels[idx + 3] = a;
  };

  auto fillTile = [&](int tileX, int tileY, uint8_t r, uint8_t g, uint8_t b) {
    int startX = tileX * kAtlasTileSize;
    int startY = tileY * kAtlasTileSize;
    for (int y = 0; y < kAtlasTileSize; ++y) {
      for (int x = 0; x < kAtlasTileSize; ++x) {
        uint8_t shade = ((x + y) & 1) ? 12 : 0;
        putPixel(startX + x, startY + y,
                 static_cast<uint8_t>(std::min(255, r + shade)),
                 static_cast<uint8_t>(std::min(255, g + shade)),
                 static_cast<uint8_t>(std::min(255, b + shade)),
                 255);
      }
    }
  };

  // Core terrain tiles.
  fillTile(kTileGrassTop % kAtlasCols, kTileGrassTop / kAtlasCols, 90, 180, 60);
  fillTile(kTileDirt % kAtlasCols, kTileDirt / kAtlasCols, 110, 85, 50);
  fillTile(kTileStone % kAtlasCols, kTileStone / kAtlasCols, 130, 130, 130);

  // Grass side: top green strip + dirt body.
  int sideX = (kTileGrassSide % kAtlasCols) * kAtlasTileSize;
  int sideY = (kTileGrassSide / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      bool top = y < 4;
      uint8_t r = top ? 90 : 110;
      uint8_t g = top ? 180 : 85;
      uint8_t b = top ? 60 : 50;
      uint8_t shade = ((x + y) & 1) ? 10 : 0;
      putPixel(sideX + x, sideY + y,
               static_cast<uint8_t>(std::min(255, r + shade)),
               static_cast<uint8_t>(std::min(255, g + shade)),
               static_cast<uint8_t>(std::min(255, b + shade)),
               255);
    }
  }

  auto hash = [](int x, int y, int s) -> uint32_t {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(s) * 362437u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h;
  };

  auto drawBreakTile = [&](int tileIndex, int stage) {
    int tileX = tileIndex % kAtlasCols;
    int tileY = tileIndex / kAtlasCols;
    int startX = tileX * kAtlasTileSize;
    int startY = tileY * kAtlasTileSize;
    int size = kAtlasTileSize;
    int center = size / 2;

    for (int y = 0; y < size; ++y) {
      for (int x = 0; x < size; ++x) {
        bool crack = false;
        if (stage >= 1 && std::abs(x - y) <= 0) {
          crack = true;
        }
        if (stage >= 2 && std::abs((x + y) - (size - 1)) <= 0) {
          crack = true;
        }
        if (stage >= 3 && std::abs(x - center) <= 0) {
          crack = true;
        }
        if (stage >= 4 && std::abs(y - (size / 3)) <= 0) {
          crack = true;
        }
        if (stage >= 5 && std::abs(x - (2 * size / 3)) <= 0) {
          crack = true;
        }
        if (stage >= 6 && std::abs(y - (2 * size / 3)) <= 0) {
          crack = true;
        }
        if (stage >= 7) {
          uint32_t h = hash(x, y, stage);
          if ((h % 100u) < static_cast<uint32_t>(stage * 3)) {
            crack = true;
          }
        }
        if (crack) {
          putPixel(startX + x, startY + y, 255, 255, 255, 200);
        }
      }
    }
  };

  for (int stage = 0; stage < kBreakStages; ++stage) {
    int tileIndex = kBreakTileBase + stage;
    if (tileIndex >= kAtlasCols * kAtlasRows) {
      break;
    }
    drawBreakTile(tileIndex, stage + 1);
  }

  // Water tile: semi-transparent with simple wave texture pattern.
  const int waterTileIndex = kTileWater;
  const int waterTileX = (waterTileIndex % kAtlasCols) * kAtlasTileSize;
  const int waterTileY = (waterTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      bool stripe = ((x + y * 2) % 5) == 0;
      uint8_t r = stripe ? 44 : 34;
      uint8_t g = stripe ? 98 : 84;
      uint8_t b = stripe ? 196 : 180;
      uint8_t a = stripe ? 212 : 196;
      putPixel(waterTileX + x, waterTileY + y, r, g, b, a);
    }
  }

  // Solid white tile for HUD markers (crosshair).
  const int whiteTileIndex = kTileUiWhite;
  const int whiteTileX = (whiteTileIndex % kAtlasCols) * kAtlasTileSize;
  const int whiteTileY = (whiteTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      putPixel(whiteTileX + x, whiteTileY + y, 255, 255, 255, 255);
    }
  }

  // Sand tile: warm, light, slightly speckled.
  const int sandTileIndex = kTileSand;
  const int sandTileX = (sandTileIndex % kAtlasCols) * kAtlasTileSize;
  const int sandTileY = (sandTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      uint32_t h = hash(x, y, 140);
      int grain = static_cast<int>(h % 18u) - 9;
      uint8_t r = static_cast<uint8_t>(std::clamp(220 + grain, 0, 255));
      uint8_t g = static_cast<uint8_t>(std::clamp(198 + grain, 0, 255));
      uint8_t b = static_cast<uint8_t>(std::clamp(140 + grain, 0, 255));
      putPixel(sandTileX + x, sandTileY + y, r, g, b, 255);
    }
  }

  // Gravel tile: neutral gray with stronger mottling.
  const int gravelTileIndex = kTileGravel;
  const int gravelTileX = (gravelTileIndex % kAtlasCols) * kAtlasTileSize;
  const int gravelTileY = (gravelTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      uint32_t h = hash(x, y, 151);
      int grain = static_cast<int>(h % 38u) - 19;
      uint8_t r = static_cast<uint8_t>(std::clamp(132 + grain, 0, 255));
      uint8_t g = static_cast<uint8_t>(std::clamp(128 + grain, 0, 255));
      uint8_t b = static_cast<uint8_t>(std::clamp(124 + grain, 0, 255));
      putPixel(gravelTileX + x, gravelTileY + y, r, g, b, 255);
    }
  }

  // Wood tile: warm bark with vertical grain.
  const int woodTileIndex = kTileWood;
  const int woodTileX = (woodTileIndex % kAtlasCols) * kAtlasTileSize;
  const int woodTileY = (woodTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      uint32_t h = hash(x, y, 161);
      int grain = static_cast<int>(h % 24u) - 12;
      bool barkLine = ((x + static_cast<int>((h >> 5) & 3u)) % 5) == 0;
      int r = 126 + grain + (barkLine ? -20 : 0);
      int g = 92 + grain / 2 + (barkLine ? -16 : 0);
      int b = 58 + grain / 3 + (barkLine ? -12 : 0);
      putPixel(woodTileX + x,
               woodTileY + y,
               static_cast<uint8_t>(std::clamp(r, 0, 255)),
               static_cast<uint8_t>(std::clamp(g, 0, 255)),
               static_cast<uint8_t>(std::clamp(b, 0, 255)),
               255);
    }
  }

  // Leaves tile: rich green with mottled highlights.
  const int leavesTileIndex = kTileLeaves;
  const int leavesTileX = (leavesTileIndex % kAtlasCols) * kAtlasTileSize;
  const int leavesTileY = (leavesTileIndex / kAtlasCols) * kAtlasTileSize;
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      uint32_t h = hash(x, y, 171);
      int grain = static_cast<int>(h % 30u) - 15;
      bool highlight = ((h >> 4) & 7u) == 0u;
      int r = 56 + grain / 3 + (highlight ? 18 : 0);
      int g = 146 + grain + (highlight ? 24 : 0);
      int b = 54 + grain / 4 + (highlight ? 12 : 0);
      putPixel(leavesTileX + x,
               leavesTileY + y,
               static_cast<uint8_t>(std::clamp(r, 0, 255)),
               static_cast<uint8_t>(std::clamp(g, 0, 255)),
               static_cast<uint8_t>(std::clamp(b, 0, 255)),
               255);
    }
  }

  auto clearTileAlpha = [&](int tileIndex) {
    int tileX = (tileIndex % kAtlasCols) * kAtlasTileSize;
    int tileY = (tileIndex / kAtlasCols) * kAtlasTileSize;
    for (int y = 0; y < kAtlasTileSize; ++y) {
      for (int x = 0; x < kAtlasTileSize; ++x) {
        putPixel(tileX + x, tileY + y, 0, 0, 0, 0);
      }
    }
  };

  // Seagrass tile: alpha-cutout strands for crossed underwater plants.
  const int seagrassTileIndex = kTileSeagrass;
  const int seagrassTileX = (seagrassTileIndex % kAtlasCols) * kAtlasTileSize;
  const int seagrassTileY = (seagrassTileIndex / kAtlasCols) * kAtlasTileSize;
  clearTileAlpha(seagrassTileIndex);
  std::array<int, 4> stalkBases = {2, 6, 10, 13};
  for (size_t i = 0; i < stalkBases.size(); ++i) {
    int sx = stalkBases[i];
    int h = 7 + static_cast<int>(hash(sx, static_cast<int>(i), 211) % 8u);
    int sway = static_cast<int>(hash(sx, 7, 223) % 3u) - 1;
    for (int step = 0; step < h; ++step) {
      int y = kAtlasTileSize - 1 - step;
      int drift = (step > 4) ? sway : 0;
      int x = std::clamp(sx + drift, 0, kAtlasTileSize - 1);
      uint32_t grainHash = hash(x, y, 227 + static_cast<int>(i) * 7);
      int r = 32 + static_cast<int>(grainHash % 14u);
      int g = 148 + static_cast<int>((grainHash >> 3) % 44u);
      int b = 64 + static_cast<int>((grainHash >> 5) % 26u);
      putPixel(seagrassTileX + x,
               seagrassTileY + y,
               static_cast<uint8_t>(std::clamp(r, 0, 255)),
               static_cast<uint8_t>(std::clamp(g, 0, 255)),
               static_cast<uint8_t>(std::clamp(b, 0, 255)),
               255);

      if (step > 2 && (grainHash & 3u) == 0u) {
        int side = (grainHash & 4u) ? 1 : -1;
        int leafX = std::clamp(x + side, 0, kAtlasTileSize - 1);
        putPixel(seagrassTileX + leafX,
                 seagrassTileY + y,
                 static_cast<uint8_t>(std::clamp(r + 8, 0, 255)),
                 static_cast<uint8_t>(std::clamp(g + 12, 0, 255)),
                 static_cast<uint8_t>(std::clamp(b + 6, 0, 255)),
                 255);
      }
    }
  }

  // Coral tile: compact alpha-cutout cluster with warm colors.
  const int coralTileIndex = kTileCoral;
  const int coralTileX = (coralTileIndex % kAtlasCols) * kAtlasTileSize;
  const int coralTileY = (coralTileIndex / kAtlasCols) * kAtlasTileSize;
  clearTileAlpha(coralTileIndex);
  for (int y = 0; y < kAtlasTileSize; ++y) {
    for (int x = 0; x < kAtlasTileSize; ++x) {
      uint32_t h = hash(x, y, 239);
      int fromBottom = (kAtlasTileSize - 1) - y;
      bool base = fromBottom <= 2 && (h % 5u) != 0u;
      bool branch = fromBottom >= 2 && fromBottom <= 10 &&
                    ((h % 13u) == 0u || (h % 17u) == 1u);
      bool cluster = fromBottom >= 3 && fromBottom <= 9 &&
                     std::abs(x - (kAtlasTileSize / 2)) <= 4 &&
                     ((h % 9u) <= 1u);
      if (!base && !branch && !cluster) {
        continue;
      }

      int r = 196 + static_cast<int>(h % 34u);
      int g = 82 + static_cast<int>((h >> 2) % 34u);
      int b = 70 + static_cast<int>((h >> 4) % 38u);
      putPixel(coralTileX + x,
               coralTileY + y,
               static_cast<uint8_t>(std::clamp(r, 0, 255)),
               static_cast<uint8_t>(std::clamp(g, 0, 255)),
               static_cast<uint8_t>(std::clamp(b, 0, 255)),
               255);
    }
  }

  auto drawOreTile = [&](int tileIndex, int oreR, int oreG, int oreB, int salt) {
    int tileX = (tileIndex % kAtlasCols) * kAtlasTileSize;
    int tileY = (tileIndex / kAtlasCols) * kAtlasTileSize;
    for (int y = 0; y < kAtlasTileSize; ++y) {
      for (int x = 0; x < kAtlasTileSize; ++x) {
        uint32_t h = hash(x, y, salt);
        int stoneGrain = static_cast<int>(h % 22u) - 11;
        int r = 130 + stoneGrain;
        int g = 130 + stoneGrain;
        int b = 130 + stoneGrain;
        if ((h % 11u) < 3u) {
          r = oreR + static_cast<int>((h >> 8) % 18u) - 9;
          g = oreG + static_cast<int>((h >> 13) % 18u) - 9;
          b = oreB + static_cast<int>((h >> 18) % 18u) - 9;
        }
        putPixel(tileX + x,
                 tileY + y,
                 static_cast<uint8_t>(std::clamp(r, 0, 255)),
                 static_cast<uint8_t>(std::clamp(g, 0, 255)),
                 static_cast<uint8_t>(std::clamp(b, 0, 255)),
                 255);
      }
    }
  };

  drawOreTile(kTileCoalOre, 42, 42, 42, 181);
  drawOreTile(kTileIronOre, 184, 128, 92, 191);
  drawOreTile(kTileGoldOre, 214, 176, 52, 201);

  VkDeviceSize imageSize = static_cast<VkDeviceSize>(pixels.size());

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
  createBuffer(imageSize,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer,
               stagingBufferMemory);

  void* dataPtr = nullptr;
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &dataPtr);
  std::memcpy(dataPtr, pixels.data(), static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingBufferMemory);

  createImage(static_cast<uint32_t>(texWidth),
              static_cast<uint32_t>(texHeight),
              VK_FORMAT_R8G8B8A8_SRGB,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
              textureImage,
              textureImageMemory);

  transitionImageLayout(textureImage,
                        VK_FORMAT_R8G8B8A8_SRGB,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer, textureImage,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(textureImage,
                        VK_FORMAT_R8G8B8A8_SRGB,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanContext::createTextureImageView() {
  textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanContext::createTextureSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

  checkVk(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler),
          "Failed to create texture sampler.");
}

void VulkanContext::createFramebuffers() {
  swapchainFramebuffers.resize(swapchainImageViews.size());

  for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
    VkImageView attachments[] = {swapchainImageViews[i], depthImageView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = swapchainExtent.width;
    framebufferInfo.height = swapchainExtent.height;
    framebufferInfo.layers = 1;

    checkVk(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]),
            "Failed to create framebuffer.");
  }
}

void VulkanContext::createCommandPool() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  checkVk(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool),
          "Failed to create command pool.");
}

void VulkanContext::createVertexBuffer() {
  if (meshVertices.empty()) {
    return;
  }
  VkDeviceSize bufferSize = sizeof(meshVertices[0]) * meshVertices.size();
  if (bufferSize == 0) {
    return;
  }
  createBuffer(bufferSize,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               vertexBuffer,
               vertexBufferMemory);

  void* data = nullptr;
  vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
  std::memcpy(data, meshVertices.data(), static_cast<size_t>(bufferSize));
  vkUnmapMemory(device, vertexBufferMemory);
}

void VulkanContext::createIndexBuffer() {
  if (meshIndices.empty()) {
    return;
  }
  VkDeviceSize bufferSize = sizeof(meshIndices[0]) * meshIndices.size();
  if (bufferSize == 0) {
    return;
  }
  createBuffer(bufferSize,
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               indexBuffer,
               indexBufferMemory);

  void* data = nullptr;
  vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
  std::memcpy(data, meshIndices.data(), static_cast<size_t>(bufferSize));
  vkUnmapMemory(device, indexBufferMemory);
}

void VulkanContext::createUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  uniformBuffers.resize(swapchainImages.size());
  uniformBuffersMemory.resize(swapchainImages.size());
  uniformBuffersMapped.resize(swapchainImages.size());

  for (size_t i = 0; i < swapchainImages.size(); ++i) {
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniformBuffers[i],
                 uniformBuffersMemory[i]);

    vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
  }
}

void VulkanContext::createDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(swapchainImages.size());
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(swapchainImages.size());

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());

  checkVk(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool),
          "Failed to create descriptor pool.");
}

void VulkanContext::createDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(swapchainImages.size(), descriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
  allocInfo.pSetLayouts = layouts.data();

  descriptorSets.resize(swapchainImages.size());
  checkVk(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()),
          "Failed to allocate descriptor sets.");

  for (size_t i = 0; i < swapchainImages.size(); ++i) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView;
    imageInfo.sampler = textureSampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }
}

void VulkanContext::createCommandBuffers() {
  commandBuffers.resize(swapchainFramebuffers.size());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  checkVk(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
          "Failed to allocate command buffers.");
}

void VulkanContext::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
          "Failed to begin recording command buffer.");

  float daylight = std::clamp(environmentDaylight, 0.0f, 1.0f);
  float weather = std::clamp(environmentWeatherIntensity, 0.0f, 1.0f);
  float cycle = environmentDayCycleTime - std::floor(environmentDayCycleTime);
  if (cycle < 0.0f) {
    cycle += 1.0f;
  }
  glm::vec3 clearNight = glm::vec3(0.02f, 0.03f, 0.09f);
  glm::vec3 clearDay = glm::vec3(0.52f, 0.72f, 0.97f);
  glm::vec3 clearStorm = glm::vec3(0.18f, 0.22f, 0.30f);
  glm::vec3 clearColor = glm::mix(clearNight, clearDay, daylight);
  float sunPhase = std::sin((cycle - 0.25f) * 6.28318530718f);
  float twilight = std::clamp(1.0f - std::abs(sunPhase), 0.0f, 1.0f);
  twilight = twilight * twilight * (3.0f - 2.0f * twilight);
  twilight *= (1.0f - weather * 0.6f);
  clearColor = glm::mix(clearColor, glm::vec3(0.84f, 0.45f, 0.22f), twilight * 0.25f);
  clearColor = glm::mix(clearColor, clearStorm, weather * 0.72f);

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{clearColor.r, clearColor.g, clearColor.b, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchainExtent;
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchainExtent.width);
  viewport.height = static_cast<float>(swapchainExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchainExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  if (!meshIndices.empty() && vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (skyIndexCount > 0 && uiPipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline);
      vkCmdBindDescriptorSets(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout,
                              0,
                              1,
                              &descriptorSets[imageIndex],
                              0,
                              nullptr);
      vkCmdDrawIndexed(commandBuffer, skyIndexCount, 1, 0, 0, 0);
    }

  }

  if (!worldChunkDrawOrder.empty() && graphicsPipeline != VK_NULL_HANDLE) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &descriptorSets[imageIndex],
                            0,
                            nullptr);

    VkDeviceSize offset = 0;
    for (uint64_t key : worldChunkDrawOrder) {
      auto it = worldChunkMeshes.find(key);
      if (it == worldChunkMeshes.end()) {
        continue;
      }
      const ChunkGpuMesh& mesh = it->second;
      if (mesh.vertexBuffer == VK_NULL_HANDLE || mesh.indexBuffer == VK_NULL_HANDLE || mesh.indexCount == 0) {
        continue;
      }
      if (!mesh.alwaysVisible) {
        float chunkCenterX = static_cast<float>(mesh.chunkX * kChunkSizeBlocks + kChunkSizeBlocks / 2);
        float chunkCenterZ = static_cast<float>(mesh.chunkZ * kChunkSizeBlocks + kChunkSizeBlocks / 2);
        float dx = chunkCenterX - cameraWorldPos.x;
        float dz = chunkCenterZ - cameraWorldPos.z;
        float distSq = dx * dx + dz * dz;
        if (distSq > kChunkDrawDistanceBlocksSq) {
          continue;
        }
      }
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer, &offset);
      vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }
  }

  if (!meshIndices.empty() && vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE &&
      uiIndexCount > 0 && uiPipeline != VK_NULL_HANDLE) {
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &descriptorSets[imageIndex],
                            0,
                            nullptr);
    vkCmdDrawIndexed(commandBuffer,
                     uiIndexCount,
                     1,
                     skyIndexCount + worldIndexCount,
                     0,
                     0);
  }
  vkCmdEndRenderPass(commandBuffer);

  checkVk(vkEndCommandBuffer(commandBuffer),
          "Failed to record command buffer.");
}

void VulkanContext::updateUniformBuffer(uint32_t imageIndex) {
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float>(currentTime - kStartTime).count();
  float daylight = std::clamp(environmentDaylight, 0.0f, 1.0f);
  float weather = std::clamp(environmentWeatherIntensity, 0.0f, 1.0f);
  float dayCycle = environmentDayCycleTime - std::floor(environmentDayCycleTime);
  if (dayCycle < 0.0f) {
    dayCycle += 1.0f;
  }

  UniformBufferObject ubo{};
  ubo.model = glm::mat4(1.0f);
  ubo.view = cameraView;
  ubo.proj = cameraProj;
  ubo.params = glm::vec4(time,
                         std::max(1.0f, static_cast<float>(swapchainExtent.width)),
                         std::max(1.0f, static_cast<float>(swapchainExtent.height)),
                         daylight);
  ubo.cameraData = glm::vec4(cameraWorldPos, cameraUnderwater ? 1.0f : 0.0f);
  ubo.weatherData = glm::vec4(weather,
                              std::clamp(0.22f + weather * 0.78f, 0.0f, 1.0f),
                              dayCycle,
                              0.0f);

  std::memcpy(uniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
}

void VulkanContext::createSyncObjects() {
  imageAvailableSemaphores.resize(kMaxFramesInFlight);
  inFlightFences.resize(kMaxFramesInFlight);
  renderFinishedSemaphores.resize(swapchainImages.size());
  imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < kMaxFramesInFlight; ++i) {
    checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
            "Failed to create semaphore.");
    checkVk(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]),
            "Failed to create fence.");
  }

  for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
    checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]),
            "Failed to create semaphore.");
  }
}

void VulkanContext::cleanupSwapchain() {
  for (auto framebuffer : swapchainFramebuffers) {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }
  swapchainFramebuffers.clear();

  if (!commandBuffers.empty()) {
    vkFreeCommandBuffers(device, commandPool,
                         static_cast<uint32_t>(commandBuffers.size()),
                         commandBuffers.data());
    commandBuffers.clear();
  }

  if (descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < uniformBuffers.size(); ++i) {
    if (uniformBuffersMapped[i]) {
      vkUnmapMemory(device, uniformBuffersMemory[i]);
      uniformBuffersMapped[i] = nullptr;
    }
    vkDestroyBuffer(device, uniformBuffers[i], nullptr);
    vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
  }
  uniformBuffers.clear();
  uniformBuffersMemory.clear();
  uniformBuffersMapped.clear();

  if (depthImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, depthImageView, nullptr);
    depthImageView = VK_NULL_HANDLE;
  }
  if (depthImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, depthImage, nullptr);
    depthImage = VK_NULL_HANDLE;
  }
  if (depthImageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, depthImageMemory, nullptr);
    depthImageMemory = VK_NULL_HANDLE;
  }

  if (graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    graphicsPipeline = VK_NULL_HANDLE;
  }
  if (uiPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, uiPipeline, nullptr);
    uiPipeline = VK_NULL_HANDLE;
  }

  if (pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }

  if (renderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;
  }

  for (auto imageView : swapchainImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }
  swapchainImageViews.clear();

  if (swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
  }
}

void VulkanContext::recreateSwapchain() {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while (width == 0 || height == 0) {
    glfwWaitEvents();
    glfwGetFramebufferSize(window, &width, &height);
  }

  vkDeviceWaitIdle(device);
  cleanupSwapchain();
  for (auto semaphore : renderFinishedSemaphores) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }
  renderFinishedSemaphores.clear();

  createSwapchain();
  createImageViews();
  createRenderPass();
  createGraphicsPipeline();
  createDepthResources();
  createFramebuffers();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();

  renderFinishedSemaphores.resize(swapchainImages.size());
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
    checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]),
            "Failed to create semaphore.");
  }

  imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);
}

bool VulkanContext::checkValidationLayerSupport() const {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char* layerName : kValidationLayers) {
    bool layerFound = false;

    for (const auto& layerProperties : availableLayers) {
      if (std::strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

std::vector<const char*> VulkanContext::getRequiredExtensions() const {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

  if (kEnableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
#ifdef __APPLE__
  extensions.push_back("VK_KHR_portability_enumeration");
#endif

  return extensions;
}

void VulkanContext::populateDebugMessengerCreateInfo(
  VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(
  VkPhysicalDevice physicalDevice) const {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

  int index = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = static_cast<uint32_t>(index);
    }

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice,
                                         static_cast<uint32_t>(index),
                                         surface,
                                         &presentSupport);
    if (presentSupport) {
      indices.presentFamily = static_cast<uint32_t>(index);
    }

    if (indices.isComplete()) {
      break;
    }

    ++index;
  }

  return indices;
}

VulkanContext::SwapchainSupportDetails VulkanContext::querySwapchainSupport(
  VkPhysicalDevice physicalDevice) const {
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                              details.presentModes.data());
  }

  return details;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice physicalDevice) const {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
  bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);
  bool swapchainAdequate = false;
  if (extensionsSupported) {
    SwapchainSupportDetails support = querySwapchainSupport(physicalDevice);
    swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice) const {
  uint32_t extensionCount = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(kDeviceExtensions.begin(),
                                           kDeviceExtensions.end());

  for (const auto& extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR>& formats) const {
  for (const auto& availableFormat : formats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return formats[0];
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(
  const std::vector<VkPresentModeKHR>& modes) const {
  // Prefer FIFO (vsync) to avoid aggressive frame spinning and high CPU usage.
  for (const auto& availablePresentMode : modes) {
    if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR) {
      return availablePresentMode;
    }
  }
  // Fallback to a mode that should exist on all Vulkan implementations.
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(
  const VkSurfaceCapabilitiesKHR& capabilities) const {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D actualExtent = {
    static_cast<uint32_t>(width),
    static_cast<uint32_t>(height)
  };

  actualExtent.width = std::clamp(actualExtent.width,
                                  capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
  actualExtent.height = std::clamp(actualExtent.height,
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);

  return actualExtent;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter,
                                       VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1u << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type.");
}

void VulkanContext::createBuffer(VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer,
                                 VkDeviceMemory& bufferMemory) const {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  checkVk(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
          "Failed to create buffer.");

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  checkVk(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory),
          "Failed to allocate buffer memory.");

  vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanContext::createImage(uint32_t width,
                                uint32_t height,
                                VkFormat format,
                                VkImageTiling tiling,
                                VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties,
                                VkImage& image,
                                VkDeviceMemory& imageMemory) const {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  checkVk(vkCreateImage(device, &imageInfo, nullptr, &image),
          "Failed to create image.");

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  checkVk(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory),
          "Failed to allocate image memory.");

  vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanContext::createImageView(VkImage image,
                                           VkFormat format,
                                           VkImageAspectFlags aspectFlags) const {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView = VK_NULL_HANDLE;
  checkVk(vkCreateImageView(device, &viewInfo, nullptr, &imageView),
          "Failed to create image view.");
  return imageView;
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  checkVk(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer),
          "Failed to allocate command buffer.");

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
          "Failed to begin command buffer.");

  return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
  checkVk(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  checkVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE),
          "Failed to submit command buffer.");
  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanContext::transitionImageLayout(VkImage image,
                                          VkFormat format,
                                          VkImageLayout oldLayout,
                                          VkImageLayout newLayout) {
  (void)format;
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::runtime_error("Unsupported layout transition.");
  }

  vkCmdPipelineBarrier(commandBuffer,
                       sourceStage,
                       destinationStage,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);

  endSingleTimeCommands(commandBuffer);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer,
                                      VkImage image,
                                      uint32_t width,
                                      uint32_t height) {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer,
                         buffer,
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  endSingleTimeCommands(commandBuffer);
}

VkFormat VulkanContext::findDepthFormat() const {
  return findSupportedFormat(
    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat VulkanContext::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                            VkImageTiling tiling,
                                            VkFormatFeatureFlags features) const {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("Failed to find supported format.");
}
