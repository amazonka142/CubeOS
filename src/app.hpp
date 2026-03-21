#pragma once

#include "audio.hpp"
#include "vk_context.hpp"
#include "world.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <array>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct GLFWwindow;

struct ItemStack {
  uint8_t type = kAir;
  uint16_t count = 0;
};

class App {
public:
  static constexpr size_t kHotbarSlotCount = 9;
  static constexpr size_t kInventorySlotCount = 27;
  static constexpr size_t kAchievementCount = 13;
  static constexpr size_t kCraftingSlotCount = 9;
  static constexpr size_t kFurnaceSlotCount = 3;

  void run();

private:
  struct SavedFurnace {
    glm::ivec3 pos{};
    ItemStack input{};
    ItemStack fuel{};
    ItemStack output{};
    float burnTime = 0.0f;
    float burnDuration = 0.0f;
    float smeltProgress = 0.0f;
  };

  struct PlayerSaveData {
    glm::vec3 pos{8.0f, 30.0f, 8.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    int selectedSlot = 0;
    std::array<ItemStack, kHotbarSlotCount> hotbar{};
    std::array<ItemStack, kInventorySlotCount> inventory{};
    uint32_t achievementMask = 0;
    std::vector<SavedFurnace> furnaces{};
  };

  struct UserSettings {
    int graphicsQuality = 1;   // 0=Low, 1=Medium, 2=High
    int renderDistance = 8;    // chunk view radius
    float sensitivity = 0.10f; // mouse look sensitivity
    int audioVolume = 80;      // placeholder value, reserved for audio system
    float uiScale = 1.0f;      // logical UI scale multiplier
    int language = 0;          // 0=English, 1=Russian
    bool blockGuides = true;   // block outline + placement preview
  };

  enum class ScreenState : uint8_t {
    kMainMenu = 0,
    kWorldSelect = 1,
    kSettings = 2,
    kCreateWorld = 3,
    kPlaying = 4,
    kPaused = 5,
    kLoadingWorld = 6
  };

  struct WorldSelectEntry {
    std::string displayName;
    std::string path;
  };

  void refreshSelectedBlock();
  void showSelectedItemToast();
  void showToast(const std::string& text, float duration = 2.0f);
  void renderLoadingFrame(float progress, const std::string& message);
  bool addToInventory(uint8_t type, uint16_t count, uint16_t* outRemaining = nullptr);
  void initWindow();
  void initVulkan();
  void setupGameplaySession();
  void setScreenState(ScreenState state);
  void processMenuInput(float deltaTime);
  void beginWorldSelectFlow();
  void refreshWorldSelectEntries();
  void loadWorldFromSelection(int entryIndex);
  void saveCurrentPlayerState() const;
  bool loadPlayerStateForWorld(const std::string& worldPath, PlayerSaveData& outState) const;
  void beginCreateWorldFlow();
  void createWorldFromMenu();
  void updateWindowTitle();
  void onCharInput(unsigned int codepoint);
  void loadSettings();
  bool saveSettings() const;
  void saveSettingsWithWarning(const char* reason) const;
  void applySettings(bool refreshWorldStreaming);
  void mainLoop();
  void processInput(float deltaTime);
  void updatePlayer(float deltaTime);
  void updateEnvironment(float deltaTime);
  void updateFurnaces(float deltaTime);
  void syncAudioState();
  void resetEnvironmentForSession();
  float computeDaylightFactor() const;
  void spawnDroppedItem(uint8_t type, const glm::ivec3& blockPos);
  void spawnDroppedStack(const ItemStack& stack, const glm::ivec3& blockPos);
  void spawnDroppedItemWithPhysics(uint8_t type,
                                   const glm::vec3& position,
                                   const glm::vec3& velocity,
                                   float pickupDelay,
                                   uint16_t count = 1);
  void updateDroppedItems(float deltaTime);
  void syncDroppedItemMesh(bool force);
  void updateInteractionOverlayMesh();
  void clearInteractionOverlayMesh();
  void rebuildWorldMesh();
  void rebuildUiMesh();
  void composeMeshData();
  void updateStreaming();
  std::vector<glm::vec4> collectTorchLights(size_t maxLights) const;
  void refreshAchievementsProgress();
  bool unlockAchievement(uint8_t id);
  bool claimLootCache(const glm::ivec3& block);
  bool hasWorkbenchAccess() const;
  bool hasFurnaceAccess() const;
  int activeCraftGridSize() const;
  void returnCraftingItemsToInventory();
  bool tryReturnCraftingItemsToInventory();
  int countStoredItem(uint8_t type) const;
  bool consumeStoredItem(uint8_t type, uint16_t count);
  void setInventoryOpen(bool open);
  void setAchievementTreeOpen(bool open);
  void refreshCursorMode();
  bool canUseWorkbenchAt(const glm::ivec3& block) const;
  bool canUseFurnaceAt(const glm::ivec3& block) const;
  bool handleInventoryClick(double xpos, double ypos, bool rightClick);
  float uiScaleFactor() const;
  int uiLayoutWidth() const;
  int uiLayoutHeight() const;
  glm::vec2 cursorToFramebuffer(double xpos, double ypos) const;
  bool hitTestHotbar(float x, float y, int& outSlot) const;
  bool hitTestInventory(float x, float y, int& outIndex) const;
  bool hitTestCraftInput(float x, float y, int& outIndex) const;
  bool hitTestCraftResult(float x, float y) const;
  bool hitTestFurnaceSlot(float x, float y, int& outIndex) const;
  bool collidesAt(const glm::vec3& pos) const;
  bool collidesAtWithPlacedBlock(const glm::vec3& pos, int blockX, int blockY, int blockZ, uint8_t placedType) const;
  bool droppedItemCollidesAt(const glm::vec3& pos) const;
  bool intersectsWaterAt(const glm::vec3& pos) const;
  bool blockIntersectsPlayer(int x, int y, int z) const;
  bool canPlaceBlockAt(int x, int y, int z, uint8_t placedType, glm::vec3* outAdjustedPlayerPos = nullptr) const;
  glm::vec3 cameraFront() const;
  float cameraEyeHeight() const;
  struct RaycastHit {
    bool hit = false;
    glm::ivec3 block{};
    glm::ivec3 normal{};
  };

  struct DroppedItemEntity {
    uint8_t type = kAir;
    uint16_t count = 0;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float age = 0.0f;
    float pickupDelay = 0.0f;
    float spinPhase = 0.0f;
    bool onGround = false;
  };

  struct FurnaceState {
    ItemStack input{};
    ItemStack fuel{};
    ItemStack output{};
    float burnTime = 0.0f;
    float burnDuration = 0.0f;
    float smeltProgress = 0.0f;
  };

public:

  struct ProfilerMetric {
    double lastMs = 0.0;
    double avgMs = 0.0;
    double maxMs = 0.0;
  };

  struct FrameProfilerState {
    uint64_t frameCount = 0;
    double targetFps = 0.0;
    double targetFrameMs = 0.0;
    size_t torchLights = 0;
    size_t chunkUpdates = 0;
    size_t chunkRemovals = 0;
    size_t pendingGpuUploads = 0;
    size_t pendingGpuRemovals = 0;
    ProfilerMetric frame{};
    ProfilerMetric eventsInput{};
    ProfilerMetric environment{};
    ProfilerMetric player{};
    ProfilerMetric streaming{};
    ProfilerMetric simulation{};
    ProfilerMetric gameplay{};
    ProfilerMetric mesh{};
    ProfilerMetric ui{};
    ProfilerMetric torch{};
    ProfilerMetric draw{};
    ProfilerMetric sleep{};
  };

private:
  RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const;
  void cleanup();
  static void windowSizeCallback(GLFWwindow* window, int width, int height);
  static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
  static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  static void charCallback(GLFWwindow* window, unsigned int codepoint);

  GLFWwindow* window = nullptr;
  int width = 1280;
  int height = 720;
  int framebufferWidth = 1280;
  int framebufferHeight = 720;
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
  bool vkReady = false;

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
  std::array<ItemStack, kHotbarSlotCount> hotbar{};
  std::array<ItemStack, kInventorySlotCount> inventory{};
  std::array<ItemStack, kCraftingSlotCount> craftingSlots{};
  ItemStack cursorStack{};
  float cursorFbX = 0.0f;
  float cursorFbY = 0.0f;
  int selectedSlot = 0;
  bool inventoryOpen = false;
  bool workbenchOpen = false;
  glm::ivec3 activeWorkbenchBlock{};
  bool furnaceOpen = false;
  glm::ivec3 activeFurnaceBlock{};
  bool achievementTreeOpen = false;
  float achievementTreeScrollX = 0.0f;
  float achievementTreeScrollY = 0.0f;
  bool achievementTreeDragging = false;
  float achievementTreeDragLastX = 0.0f;
  float achievementTreeDragLastY = 0.0f;
  bool tabDown = false;
  bool escDown = false;
  bool achievementToggleDown = false;
  bool dropOneDown = false;
  bool mouseLeftDown = false;
  bool mouseRightDown = false;
  bool crouching = false;
  bool sprinting = false;
  bool uiDirty = true;
  std::unordered_map<uint64_t, VulkanContext::WorldChunkMeshUpload> pendingWorldChunkUploads{};
  std::unordered_set<uint64_t> pendingWorldChunkRemovals{};
  std::vector<DroppedItemEntity> droppedItems{};
  bool droppedItemMeshUploaded = false;
  float droppedItemMeshTimer = 0.0f;
  double lastWorldChunkUploadTime = 0.0;
  float waterSimAccumulator = 0.0f;
  float waterSimBoostTimer = 0.0f;
  float waterSwimSoundTimer = 0.0f;
  bool wasPlayerInWater = false;
  float dayCycleTime = 0.32f;
  float dayLightFactor = 1.0f;
  float weatherIntensity = 0.0f;
  float weatherTargetIntensity = 0.0f;
  float weatherDecisionTimer = 45.0f;
  uint32_t weatherRngState = 0xA341316Cu;
  ScreenState screenState = ScreenState::kMainMenu;
  ScreenState settingsReturnState = ScreenState::kMainMenu;
  int mainMenuSelection = 0;
  int pauseMenuSelection = 0;
  int worldSelectSelection = 0;
  int worldSelectScroll = 0;
  int settingsSelection = 0;
  int settingsScroll = 0;
  int settingsCategory = 0;
  int settingsOptionSelection = 0;
  int settingsActionSelection = 0;
  int settingsFocusArea = 1;
  int createWorldSelection = 0;
  std::vector<WorldSelectEntry> worldSelectEntries;
  std::string pendingWorldName = "World";
  std::string pendingSeedText{};
  std::string currentWorldPath = "world.bin";
  WorldGenSettings pendingWorldSettings{};
  bool hasPendingPlayerResume = false;
  PlayerSaveData pendingPlayerState{};
  bool menuUpDown = false;
  bool menuDownDown = false;
  bool menuLeftDown = false;
  bool menuRightDown = false;
  bool menuEnterDown = false;
  bool menuBackspaceDown = false;
  bool menuEscDown = false;
  UserSettings appliedSettings{};
  UserSettings pendingSettings{};
  bool settingsDirty = false;
  int activeChunkViewRadius = 8;
  int loadedChunkViewRadius = 8;
  double nextStreamingRadiusExpandTime = 0.0;
  float menuIntro = 1.0f;
  std::string selectedItemToastText{};
  float selectedItemToastTimer = 0.0f;
  uint32_t achievementMask = 0;
  bool achievementPopupVisible = false;
  uint8_t activeAchievementPopupId = 0;
  float achievementPopupTimer = 0.0f;
  std::deque<uint8_t> achievementPopupQueue{};
  std::unordered_map<std::string, FurnaceState> furnaceStates{};
  bool interactionOverlayUploaded = false;
  bool interactionOutlineActive = false;
  glm::ivec3 interactionOutlineBlock{};
  bool interactionPreviewActive = false;
  glm::ivec3 interactionPreviewBlock{};
  uint8_t interactionPreviewType = kAir;
  float loadingWorldProgress = 0.0f;
  std::string loadingWorldMessage{};
  float fpsSampleAccum = 0.0f;
  int fpsSampleFrames = 0;
  int fpsDisplayValue = 0;
  float craftResultFlashTimer = 0.0f;
  std::vector<glm::vec4> cachedTorchLights{};
  float torchLightRefreshTimer = 0.0f;
  glm::vec3 torchLightSampleEye{0.0f};
  glm::vec3 torchLightSampleForward{0.0f, 0.0f, 1.0f};
  uint8_t torchLightSampleSelectedBlock = kAir;
  bool torchLightsCacheValid = false;
  bool debugWorldgenOverlay = false;
  int debugWorldgenOverlayMode = 0;
  int debugDensitySliceOffset = 0;
  bool debugProfilerOverlay = false;
  bool debugToggleDown = false;
  bool debugModeDown = false;
  bool debugSliceUpDown = false;
  bool debugSliceDownDown = false;
  bool debugProfilerToggleDown = false;
  FrameProfilerState profiler{};
  AudioSystem audio{};

  float yaw = -90.0f;
  float pitch = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
};
