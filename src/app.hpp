#pragma once

#include "vk_context.hpp"
#include "world.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <array>
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
  void run();

private:
  struct UserSettings {
    int graphicsQuality = 1;   // 0=Low, 1=Medium, 2=High
    float sensitivity = 0.10f; // mouse look sensitivity
    int audioVolume = 80;      // placeholder value, reserved for audio system
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
  bool loadPlayerStateForWorld(const std::string& worldPath,
                               glm::vec3& outPos,
                               float& outYaw,
                               float& outPitch) const;
  void beginCreateWorldFlow();
  void createWorldFromMenu();
  void updateWindowTitle();
  void onCharInput(unsigned int codepoint);
  void loadSettings();
  bool saveSettings() const;
  void applySettings(bool refreshWorldStreaming);
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
  static void charCallback(GLFWwindow* window, unsigned int codepoint);

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
  std::unordered_map<uint64_t, VulkanContext::WorldChunkMeshUpload> pendingWorldChunkUploads{};
  std::unordered_set<uint64_t> pendingWorldChunkRemovals{};
  double lastWorldChunkUploadTime = 0.0;
  float waterSimAccumulator = 0.0f;
  float waterSimBoostTimer = 0.0f;
  ScreenState screenState = ScreenState::kMainMenu;
  ScreenState settingsReturnState = ScreenState::kMainMenu;
  int mainMenuSelection = 0;
  int pauseMenuSelection = 0;
  int worldSelectSelection = 0;
  int worldSelectScroll = 0;
  int settingsSelection = 0;
  int createWorldSelection = 0;
  std::vector<WorldSelectEntry> worldSelectEntries;
  std::string pendingWorldName = "World";
  std::string pendingSeedText{};
  std::string currentWorldPath = "world.bin";
  WorldGenSettings pendingWorldSettings{};
  bool hasPendingPlayerResume = false;
  glm::vec3 pendingResumePlayerPos{0.0f};
  float pendingResumeYaw = -90.0f;
  float pendingResumePitch = 0.0f;
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
  float menuIntro = 1.0f;
  std::string selectedItemToastText{};
  float selectedItemToastTimer = 0.0f;
  float loadingWorldProgress = 0.0f;
  std::string loadingWorldMessage{};

  float yaw = -90.0f;
  float pitch = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
};
