#pragma once

#include "mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

class VulkanContext {
public:
  struct WorldChunkMeshUpload {
    uint64_t key = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
  };

  void init(GLFWwindow* window, bool* framebufferResizedFlag);
  void cleanup();
  void drawFrame();
  void waitIdle();
  void setMeshData(const std::vector<Vertex>& vertices,
                   const std::vector<uint32_t>& indices,
                   uint32_t skyIndexCount,
                   uint32_t worldIndexCount,
                   uint32_t uiIndexCount);
  void updateMesh(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices,
                  uint32_t skyIndexCount,
                  uint32_t worldIndexCount,
                  uint32_t uiIndexCount);
  void setWorldChunkMeshes(const std::vector<WorldChunkMeshUpload>& uploads);
  void updateWorldChunkMeshes(const std::vector<WorldChunkMeshUpload>& uploads,
                              const std::vector<uint64_t>& removedKeys);
  void setCameraMatrices(const glm::mat4& view, const glm::mat4& proj);
  void setCameraWorldState(const glm::vec3& eyePosition, bool underwater);
  void setEnvironmentState(float daylight, float weatherIntensity, float dayCycleTime);

private:
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }
  };

  struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createDescriptorSetLayout();
  void createSwapchain();
  void createImageViews();
  void createRenderPass();
  void createGraphicsPipeline();
  void createDepthResources();
  void createTextureImage();
  void createTextureImageView();
  void createTextureSampler();
  void createFramebuffers();
  void createCommandPool();
  void createVertexBuffer();
  void createIndexBuffer();
  void createUniformBuffers();
  void createDescriptorPool();
  void createDescriptorSets();
  void createCommandBuffers();
  void createSyncObjects();

  void cleanupSwapchain();
  void recreateSwapchain();
  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void updateUniformBuffer(uint32_t imageIndex);

  bool checkValidationLayerSupport() const;
  std::vector<const char*> getRequiredExtensions() const;
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) const;
  SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice) const;
  bool isDeviceSuitable(VkPhysicalDevice physicalDevice) const;
  bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice) const;
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
  VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

  VkShaderModule createShaderModule(const std::vector<char>& code) const;
  static std::vector<char> readFile(const std::string& path);

  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
  void createBuffer(VkDeviceSize size,
                    VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties,
                    VkBuffer& buffer,
                    VkDeviceMemory& bufferMemory) const;
  void createImage(uint32_t width,
                   uint32_t height,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties,
                   VkImage& image,
                   VkDeviceMemory& imageMemory) const;
  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
  void transitionImageLayout(VkImage image,
                             VkFormat format,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);
  void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  VkFormat findDepthFormat() const;
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features) const;
  struct ChunkGpuMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    int chunkX = 0;
    int chunkZ = 0;
    bool alwaysVisible = false;
  };
  bool uploadChunkGpuMesh(const WorldChunkMeshUpload& upload, ChunkGpuMesh& outMesh);
  void destroyChunkGpuMesh(ChunkGpuMesh& mesh);
  void retireChunkGpuMesh(ChunkGpuMesh& mesh);
  void collectRetiredWorldChunkMeshes(bool force);
  void clearWorldChunkMeshes();

  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> swapchainImages;
  VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchainExtent{};
  std::vector<VkImageView> swapchainImageViews;

  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  VkPipeline uiPipeline = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> swapchainFramebuffers;

  VkImage depthImage = VK_NULL_HANDLE;
  VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
  VkImageView depthImageView = VK_NULL_HANDLE;

  VkImage textureImage = VK_NULL_HANDLE;
  VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
  VkImageView textureImageView = VK_NULL_HANDLE;
  VkSampler textureSampler = VK_NULL_HANDLE;

  VkCommandPool commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;

  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptorSets;

  std::vector<Vertex> meshVertices;
  std::vector<uint32_t> meshIndices;
  uint32_t skyIndexCount = 0;
  uint32_t worldIndexCount = 0;
  uint32_t uiIndexCount = 0;
  std::unordered_map<uint64_t, ChunkGpuMesh> worldChunkMeshes;
  std::vector<uint64_t> worldChunkDrawOrder;
  std::unordered_map<uint64_t, size_t> worldChunkDrawOrderIndex;
  std::vector<ChunkGpuMesh> retiredWorldChunkMeshes;
  glm::mat4 cameraView{1.0f};
  glm::mat4 cameraProj{1.0f};
  glm::vec3 cameraWorldPos{0.0f};
  bool cameraUnderwater = false;
  float environmentDaylight = 1.0f;
  float environmentWeatherIntensity = 0.0f;
  float environmentDayCycleTime = 0.32f;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;
  size_t currentFrame = 0;

  GLFWwindow* window = nullptr;
  bool* framebufferResizedFlag = nullptr;
};
