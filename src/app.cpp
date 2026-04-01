#include "app.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <stdexcept>
#include <thread>
#include <utility>

#ifndef CUBEOS_RELEASE_TAG
#define CUBEOS_RELEASE_TAG "v0.3.0-snapshot.2"
#endif

namespace {

constexpr float kSlotSize = 40.0f;
constexpr float kSlotPadding = 6.0f;
constexpr float kSlotBorder = 3.0f;
constexpr float kMarginBottom = 20.0f;
constexpr float kIconPadding = 6.0f;
constexpr float kPanelPadding = 12.0f;
constexpr int kInventoryCols = 9;
constexpr int kInventoryRows = 3;
constexpr int kCraftGridMaxSize = 3;
constexpr uint16_t kMaxStack = 64;
constexpr int kDigitWidth = 3;
constexpr int kDigitHeight = 5;
constexpr float kBreakDuration = 0.6f;
constexpr float kBreakMaxDistance = 6.0f;
constexpr float kMenuButtonWidth = 320.0f;
constexpr float kMenuButtonHeight = 44.0f;
constexpr float kMenuButtonGap = 14.0f;
constexpr char kSettingsFilePath[] = "settings.cfg";
constexpr int kMinRenderDistance = 4;
constexpr int kMaxRenderDistance = 14;
constexpr uint64_t kDroppedItemsMeshKey = std::numeric_limits<uint64_t>::max() - 1ull;
constexpr uint64_t kInteractionOverlayMeshKey = std::numeric_limits<uint64_t>::max() - 2ull;
constexpr uint64_t kFirstPersonMeshKey = std::numeric_limits<uint64_t>::max() - 3ull;
constexpr uint64_t kSheepMeshKey = std::numeric_limits<uint64_t>::max() - 4ull;
constexpr int kMaxTorchLights = 16;
constexpr float kTorchLightRange = 8.5f;
constexpr float kTorchLightCandidateRange = 512.0f;
constexpr float kDroppedItemHalfSize = 0.19f;
constexpr float kDroppedItemHalfHeight = 0.19f;
constexpr float kDroppedItemGravity = -16.5f;
constexpr float kDroppedItemPickupRadius = 1.35f;
constexpr float kThrownItemSpawnAhead = 1.15f;
constexpr float kThrownItemForwardSpeed = 5.6f;
constexpr float kThrownItemUpwardSpeed = 2.4f;
constexpr float kThrownItemPickupDelay = 0.85f;
constexpr float kDroppedItemMeshUpdateInterval = 1.0f / 30.0f;
constexpr float kSheepMeshUpdateInterval = 1.0f / 24.0f;
constexpr float kDayNightCycleDurationSec = 14.0f * 60.0f;
constexpr float kWeatherMinDecisionSec = 38.0f;
constexpr float kWeatherMaxDecisionSec = 110.0f;
constexpr float kTau = 6.28318530718f;
constexpr float kCraftPanelGap = 16.0f;
constexpr float kAchievementNodeSize = 42.0f;
constexpr float kCraftResultGap = 26.0f;
constexpr float kAchievementPopupDuration = 3.15f;
constexpr float kFurnaceSmeltDuration = 3.8f;
constexpr float kCraftResultFlashDuration = 0.34f;
constexpr int kWorldSelectMaxVisibleRows = 8;
constexpr int kSettingsCategoryCount = 4;
constexpr int kSettingsActionCount = 3;
constexpr float kMinUiScale = 0.75f;
constexpr float kMaxUiScale = 2.00f;
constexpr float kUiScaleStep = 0.05f;
constexpr float kUiTextLineHeightPerPixel = 5.6f;
constexpr float kStreamingLookaheadBlocks = 12.0f;
constexpr float kStreamingProbeBlocks = 9.0f;
constexpr int kStreamingWarmRadius = 1;
constexpr float kTorchLightRefreshMoveThreshold = 0.55f;
constexpr float kTorchLightRefreshForwardDot = 0.992f;
constexpr float kFirstPersonEquipRaiseSpeed = 6.8f;
constexpr float kFirstPersonEquipLowerSpeed = 10.5f;
constexpr float kFirstPersonSwingDuration = 0.26f;
constexpr float kFirstPersonUseDuration = 0.20f;
constexpr float kFirstPersonProjectionFovDeg = 62.0f;
constexpr char kSnapshotVersionLabel[] = CUBEOS_RELEASE_TAG;
constexpr int kMaxPlayerHealth = 40;
constexpr float kFlightToggleWindowSec = 0.28f;
constexpr float kCreativeFlySpeed = 8.8f;
constexpr float kCreativeFlyBoostSpeed = 12.0f;
constexpr float kCreativeVerticalFlySpeed = 5.8f;
constexpr float kCreativeVerticalFlyBoostSpeed = 8.4f;
constexpr size_t kCommandBoxMaxChars = 96;
constexpr float kSheepHalfWidth = 0.42f;
constexpr float kSheepHalfLength = 0.62f;
constexpr float kSheepHeight = 1.20f;
constexpr float kSheepGravity = -16.0f;
constexpr float kSheepMaxSpeed = 1.35f;
constexpr float kSheepSpawnRadius = 42.0f;
constexpr size_t kTargetSheepCount = 5;
constexpr float kBedCollisionHeight = 0.5625f;
constexpr std::array<uint8_t, 22> kCreativeCatalogItems = {{
  kGrass, kDirt, kStone, kSand, kGravel, kWood, kLeaves, kWater, kCoalOre,
  kIronOre, kGoldOre, kDiamondOre, kSeagrass, kCoral, kWorkbench, kFurnace,
  kPlanks, kTorch, kWool, kRawMutton, kCookedMutton, kBed
}};
constexpr std::array<WorldPreset, 3> kCreateWorldPresetOptions = {{
  WorldPreset::kMinecraftStyle,
  WorldPreset::kClassicFlat,
  WorldPreset::kAprilFools
}};

bool isCreativeWorldMode(const WorldGenSettings& settings) {
  return settings.startInventoryMode != 0;
}

size_t createWorldPresetIndex(WorldPreset preset) {
  for (size_t i = 0; i < kCreateWorldPresetOptions.size(); ++i) {
    if (kCreateWorldPresetOptions[i] == preset) {
      return i;
    }
  }
  return 0;
}

WorldPreset cycleCreateWorldPreset(WorldPreset preset, bool forward) {
  size_t index = createWorldPresetIndex(preset);
  if (forward) {
    index = (index + 1) % kCreateWorldPresetOptions.size();
  } else {
    index = (index + kCreateWorldPresetOptions.size() - 1) % kCreateWorldPresetOptions.size();
  }
  return kCreateWorldPresetOptions[index];
}

std::string worldPresetDisplayName(WorldPreset preset, bool ru) {
  switch (preset) {
    case WorldPreset::kClassicFlat:
      return ru ? "Классический плоский" : "Classic Flat";
    case WorldPreset::kAprilFools:
      return ru ? "Первоапрельский" : "April Fools";
    case WorldPreset::kMinecraftStyle:
    default:
      return ru ? "Minecraft-стиль" : "Minecraft-style";
  }
}

std::string worldPresetButtonLabel(WorldPreset preset, bool ru) {
  switch (preset) {
    case WorldPreset::kClassicFlat:
      return ru ? "ПЛОСКИЙ" : "FLAT";
    case WorldPreset::kAprilFools:
      return "APRIL";
    case WorldPreset::kMinecraftStyle:
    default:
      return "MC";
  }
}

glm::vec3 worldPresetButtonColor(WorldPreset preset, bool selected) {
  switch (preset) {
    case WorldPreset::kClassicFlat:
      return selected ? glm::vec3(0.40f, 0.34f, 0.22f) : glm::vec3(0.23f, 0.21f, 0.18f);
    case WorldPreset::kAprilFools:
      return selected ? glm::vec3(0.50f, 0.23f, 0.36f) : glm::vec3(0.24f, 0.17f, 0.20f);
    case WorldPreset::kMinecraftStyle:
    default:
      return selected ? glm::vec3(0.30f, 0.36f, 0.45f) : glm::vec3(0.20f, 0.23f, 0.28f);
  }
}

void populateCreativeHotbar(std::array<ItemStack, App::kHotbarSlotCount>& hotbar) {
  for (ItemStack& slot : hotbar) {
    slot.type = kAir;
    slot.count = 0;
  }

  for (size_t i = 0; i < hotbar.size() && i < kCreativeCatalogItems.size(); ++i) {
    hotbar[i].type = kCreativeCatalogItems[i];
    hotbar[i].count = 64;
  }
}

float collisionTopForBlock(uint8_t type, int blockY) {
  if (isBedBlock(type)) {
    return static_cast<float>(blockY) + kBedCollisionHeight;
  }
  return static_cast<float>(blockY + 1);
}

bool solidBlockIntersectsAabb(uint8_t type,
                              int x,
                              int y,
                              int z,
                              const glm::vec3& min,
                              const glm::vec3& max) {
  if (type == kAir || isWaterBlock(type) || isDecorationBlock(type)) {
    return false;
  }

  float blockMinX = static_cast<float>(x);
  float blockMinY = static_cast<float>(y);
  float blockMinZ = static_cast<float>(z);
  float blockMaxX = blockMinX + 1.0f;
  float blockMaxY = collisionTopForBlock(type, y);
  float blockMaxZ = blockMinZ + 1.0f;

  return blockMaxX > min.x && blockMinX < max.x &&
         blockMaxY > min.y && blockMinY < max.y &&
         blockMaxZ > min.z && blockMinZ < max.z;
}

float clampUiScaleValue(float value) {
  float clamped = std::clamp(value, kMinUiScale, kMaxUiScale);
  float snapped = std::round(clamped / kUiScaleStep) * kUiScaleStep;
  return std::clamp(snapped, kMinUiScale, kMaxUiScale);
}

std::filesystem::path defaultUserHomeDirectory() {
#if defined(_WIN32)
  const char* userProfile = std::getenv("USERPROFILE");
  if (userProfile && userProfile[0] != '\0') {
    return std::filesystem::path(userProfile);
  }
#endif
  const char* home = std::getenv("HOME");
  if (home && home[0] != '\0') {
    return std::filesystem::path(home);
  }
  return {};
}

std::filesystem::path cubeosUserDataDir() {
#if defined(_WIN32)
  const char* appData = std::getenv("APPDATA");
  if (appData && appData[0] != '\0') {
    return std::filesystem::path(appData) / "CubeOS";
  }
  std::filesystem::path home = defaultUserHomeDirectory();
  if (!home.empty()) {
    return home / "AppData" / "Roaming" / "CubeOS";
  }
#elif defined(__APPLE__)
  std::filesystem::path home = defaultUserHomeDirectory();
  if (!home.empty()) {
    return home / "Library" / "Application Support" / "CubeOS";
  }
#else
  const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
  if (xdgDataHome && xdgDataHome[0] != '\0') {
    return std::filesystem::path(xdgDataHome) / "CubeOS";
  }
  std::filesystem::path home = defaultUserHomeDirectory();
  if (!home.empty()) {
    return home / ".local" / "share" / "CubeOS";
  }
#endif

  std::error_code ec;
  std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (ec || cwd.empty()) {
    return std::filesystem::path("CubeOSData");
  }
  return cwd / "CubeOSData";
}

std::filesystem::path cubeosSettingsPath() {
  return cubeosUserDataDir() / kSettingsFilePath;
}

std::filesystem::path cubeosSavesDir() {
  return cubeosUserDataDir() / "saves";
}

bool pathsResolveToSameLocation(const std::filesystem::path& a, const std::filesystem::path& b) {
  std::error_code aec;
  std::error_code bec;
  std::filesystem::path absA = std::filesystem::absolute(a, aec);
  std::filesystem::path absB = std::filesystem::absolute(b, bec);
  return !aec && !bec && absA == absB;
}

void copyDirectoryContentsIfMissing(const std::filesystem::path& sourceDir,
                                    const std::filesystem::path& targetDir) {
  std::error_code ec;
  if (!std::filesystem::exists(sourceDir, ec) || ec) {
    return;
  }
  if (pathsResolveToSameLocation(sourceDir, targetDir)) {
    return;
  }

  std::filesystem::create_directories(targetDir, ec);
  ec.clear();

  for (std::filesystem::recursive_directory_iterator it(sourceDir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::filesystem::directory_entry& entry = *it;
    std::filesystem::path relativePath = std::filesystem::relative(entry.path(), sourceDir, ec);
    if (ec) {
      ec.clear();
      continue;
    }

    std::filesystem::path targetPath = targetDir / relativePath;
    if (entry.is_directory(ec)) {
      std::filesystem::create_directories(targetPath, ec);
      ec.clear();
      continue;
    }
    if (!entry.is_regular_file(ec)) {
      ec.clear();
      continue;
    }

    std::filesystem::create_directories(targetPath.parent_path(), ec);
    ec.clear();
    std::filesystem::copy_file(entry.path(),
                               targetPath,
                               std::filesystem::copy_options::skip_existing,
                               ec);
    ec.clear();
  }
}

void preparePersistentStorage() {
  std::filesystem::path userDataDir = cubeosUserDataDir();
  std::filesystem::path settingsPath = cubeosSettingsPath();
  std::filesystem::path savesPath = cubeosSavesDir();

  std::error_code ec;
  std::filesystem::create_directories(userDataDir, ec);
  ec.clear();
  std::filesystem::create_directories(savesPath, ec);
  ec.clear();

  std::filesystem::path legacySettingsPath(kSettingsFilePath);
  if (!pathsResolveToSameLocation(legacySettingsPath, settingsPath) &&
      std::filesystem::exists(legacySettingsPath, ec) &&
      !ec) {
    std::filesystem::copy_file(legacySettingsPath,
                               settingsPath,
                               std::filesystem::copy_options::skip_existing,
                               ec);
    ec.clear();
  } else {
    ec.clear();
  }

  copyDirectoryContentsIfMissing(std::filesystem::path("saves"), savesPath);
}

int maxTorchLightsForQuality(int quality) {
  static constexpr int kQualityToMaxTorchLights[3] = {6, 10, 16};
  return kQualityToMaxTorchLights[std::clamp(quality, 0, 2)];
}

float torchLightRefreshIntervalForQuality(int quality) {
  static constexpr float kQualityToRefreshInterval[3] = {0.20f, 0.10f, 0.05f};
  return kQualityToRefreshInterval[std::clamp(quality, 0, 2)];
}

double focusedPlayingFpsForQuality(int quality) {
  static constexpr double kQualityToFocusedFps[3] = {48.0, 60.0, 72.0};
  return kQualityToFocusedFps[std::clamp(quality, 0, 2)];
}

double unfocusedPlayingFpsForQuality(int quality) {
  static constexpr double kQualityToUnfocusedFps[3] = {24.0, 30.0, 36.0};
  return kQualityToUnfocusedFps[std::clamp(quality, 0, 2)];
}

double focusedMenuFpsForQuality(int quality) {
  static constexpr double kQualityToFocusedFps[3] = {24.0, 30.0, 42.0};
  return kQualityToFocusedFps[std::clamp(quality, 0, 2)];
}

double unfocusedMenuFpsForQuality(int quality) {
  static constexpr double kQualityToUnfocusedFps[3] = {12.0, 18.0, 24.0};
  return kQualityToUnfocusedFps[std::clamp(quality, 0, 2)];
}

int startupViewRadiusForQuality(int quality, int targetRadius) {
  static constexpr int kQualityToStartupRadius[3] = {4, 5, 6};
  int startupRadius = kQualityToStartupRadius[std::clamp(quality, 0, 2)];
  return std::clamp(std::min(targetRadius, startupRadius), kMinRenderDistance, kMaxRenderDistance);
}

double streamingExpansionIntervalForQuality(int quality) {
  static constexpr double kQualityToInterval[3] = {1.10, 0.80, 0.55};
  return kQualityToInterval[std::clamp(quality, 0, 2)];
}

void recordProfilerMetric(App::ProfilerMetric& metric, double sampleMs, uint64_t frameCount) {
  metric.lastMs = sampleMs;
  if (frameCount <= 1) {
    metric.avgMs = sampleMs;
  } else {
    metric.avgMs = metric.avgMs * 0.90 + sampleMs * 0.10;
  }
  metric.maxMs = std::max(metric.maxMs, sampleMs);
}

int computeVisibleMenuRows(float availableHeight,
                           float rowHeight,
                           float rowGap,
                           int totalRows,
                           int maxVisibleRows) {
  if (totalRows <= 0) {
    return 0;
  }
  float safeAvailable = std::max(0.0f, availableHeight);
  float step = rowHeight + rowGap;
  int visibleRows = static_cast<int>(std::floor((safeAvailable + rowGap) / std::max(1.0f, step)));
  visibleRows = std::clamp(visibleRows, 1, totalRows);
  return std::min(visibleRows, maxVisibleRows);
}

int computeWorldSelectVisibleRows(int uiHeight, int totalRows) {
  float panelHeight = std::min(static_cast<float>(uiHeight) * 0.78f, 540.0f);
  return computeVisibleMenuRows(panelHeight - 116.0f, 40.0f, 10.0f, totalRows, kWorldSelectMaxVisibleRows);
}

enum class SettingsCategoryTab : int {
  kGraphics = 0,
  kAudio = 1,
  kControls = 2,
  kInterface = 3
};

enum class SettingsEntryId : int {
  kGraphicsQuality = 0,
  kRenderDistance = 1,
  kAudioVolume = 2,
  kSensitivity = 3,
  kUiScale = 4,
  kLanguage = 5,
  kBlockGuides = 6
};

enum class SettingsFocusArea : int {
  kCategories = 0,
  kOptions = 1,
  kActions = 2
};

enum class SettingsActionId : int {
  kApply = 0,
  kReset = 1,
  kBack = 2
};

struct SettingsUiLayout {
  float panelX = 0.0f;
  float panelY = 0.0f;
  float panelW = 0.0f;
  float panelH = 0.0f;
  float sidebarX = 0.0f;
  float sidebarY = 0.0f;
  float sidebarW = 0.0f;
  float categoryH = 44.0f;
  float categoryGap = 12.0f;
  float contentX = 0.0f;
  float contentY = 0.0f;
  float contentW = 0.0f;
  float optionH = 68.0f;
  float optionGap = 14.0f;
  float actionY = 0.0f;
  float actionW = 172.0f;
  float actionH = 42.0f;
  float actionGap = 12.0f;
};

bool pointInRect(float px, float py, const glm::vec4& rect) {
  return px >= rect.x &&
         px <= rect.x + rect.z &&
         py >= rect.y &&
         py <= rect.y + rect.w;
}

int settingsEntryCountForCategory(int category) {
  switch (static_cast<SettingsCategoryTab>(std::clamp(category, 0, kSettingsCategoryCount - 1))) {
    case SettingsCategoryTab::kGraphics:
      return 2;
    case SettingsCategoryTab::kAudio:
      return 1;
    case SettingsCategoryTab::kControls:
      return 1;
    case SettingsCategoryTab::kInterface:
    default:
      return 3;
  }
}

SettingsEntryId settingsEntryForCategory(int category, int optionIndex) {
  SettingsCategoryTab tab = static_cast<SettingsCategoryTab>(std::clamp(category, 0, kSettingsCategoryCount - 1));
  switch (tab) {
    case SettingsCategoryTab::kGraphics:
      return optionIndex <= 0 ? SettingsEntryId::kGraphicsQuality : SettingsEntryId::kRenderDistance;
    case SettingsCategoryTab::kAudio:
      return SettingsEntryId::kAudioVolume;
    case SettingsCategoryTab::kControls:
      return SettingsEntryId::kSensitivity;
    case SettingsCategoryTab::kInterface:
    default:
      if (optionIndex <= 0) {
        return SettingsEntryId::kUiScale;
      }
      return optionIndex == 1 ? SettingsEntryId::kLanguage : SettingsEntryId::kBlockGuides;
  }
}

SettingsUiLayout makeSettingsUiLayout(float panelX, float panelY, float panelW, float panelH) {
  SettingsUiLayout layout;
  layout.panelX = panelX;
  layout.panelY = panelY;
  layout.panelW = panelW;
  layout.panelH = panelH;
  layout.sidebarX = layout.panelX + 28.0f;
  layout.sidebarY = layout.panelY + 92.0f;
  layout.sidebarW = 184.0f;
  layout.contentX = layout.sidebarX + layout.sidebarW + 26.0f;
  layout.contentY = layout.sidebarY;
  layout.contentW = layout.panelW - (layout.contentX - layout.panelX) - 28.0f;
  layout.actionY = layout.panelY + layout.panelH - layout.actionH - 28.0f;
  return layout;
}

glm::vec4 settingsCategoryRect(const SettingsUiLayout& layout, int categoryIndex) {
  float y = layout.sidebarY + static_cast<float>(categoryIndex) * (layout.categoryH + layout.categoryGap);
  return {layout.sidebarX, y, layout.sidebarW, layout.categoryH};
}

glm::vec4 settingsOptionRect(const SettingsUiLayout& layout, int optionIndex) {
  float y = layout.contentY + static_cast<float>(optionIndex) * (layout.optionH + layout.optionGap);
  return {layout.contentX, y, layout.contentW, layout.optionH};
}

glm::vec4 settingsActionRect(const SettingsUiLayout& layout, int actionIndex) {
  float totalW = static_cast<float>(kSettingsActionCount) * layout.actionW +
                 static_cast<float>(kSettingsActionCount - 1) * layout.actionGap;
  float x = layout.contentX + (layout.contentW - totalW) * 0.5f +
            static_cast<float>(actionIndex) * (layout.actionW + layout.actionGap);
  return {x, layout.actionY, layout.actionW, layout.actionH};
}

glm::vec4 settingsControlRect(const glm::vec4& optionRect, float width = 248.0f, float height = 28.0f) {
  return {
    optionRect.x + optionRect.z - width - 18.0f,
    optionRect.y + optionRect.w - height - 18.0f,
    width,
    height
  };
}

glm::vec4 settingsSegmentRect(const glm::vec4& controlRect, int segments, int index, float gap = 8.0f) {
  float totalGap = static_cast<float>(std::max(0, segments - 1)) * gap;
  float segmentW = (controlRect.z - totalGap) / static_cast<float>(std::max(1, segments));
  float x = controlRect.x + static_cast<float>(index) * (segmentW + gap);
  return {x, controlRect.y, segmentW, controlRect.w};
}

glm::vec4 settingsSliderTrackRect(const glm::vec4& optionRect) {
  return settingsControlRect(optionRect, 248.0f, 14.0f);
}

float normalizedValueFromRect(float px, const glm::vec4& rect) {
  if (rect.z <= 1.0f) {
    return 0.0f;
  }
  return std::clamp((px - rect.x) / rect.z, 0.0f, 1.0f);
}

int scrollStepsFromOffset(double yoffset) {
  return std::max(1, static_cast<int>(std::lround(std::abs(yoffset))));
}

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

float nextUnitRandom(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return static_cast<float>((state >> 8) & 0x00FFFFFFu) / 16777215.0f;
}

float moveToward(float current, float target, float maxStep) {
  if (current < target) {
    return std::min(current + maxStep, target);
  }
  return std::max(current - maxStep, target);
}

constexpr int kGlyphWidth = 3;
constexpr int kGlyphHeight = 5;
constexpr uint8_t kLatinAlphabetMap[26][kGlyphHeight] = {
  {0b111, 0b101, 0b111, 0b101, 0b101}, // A
  {0b110, 0b101, 0b110, 0b101, 0b110}, // B
  {0b111, 0b100, 0b100, 0b100, 0b111}, // C
  {0b110, 0b101, 0b101, 0b101, 0b110}, // D
  {0b111, 0b100, 0b110, 0b100, 0b111}, // E
  {0b111, 0b100, 0b110, 0b100, 0b100}, // F
  {0b111, 0b100, 0b101, 0b101, 0b111}, // G
  {0b101, 0b101, 0b111, 0b101, 0b101}, // H
  {0b111, 0b010, 0b010, 0b010, 0b111}, // I
  {0b001, 0b001, 0b001, 0b101, 0b111}, // J
  {0b101, 0b101, 0b110, 0b101, 0b101}, // K
  {0b100, 0b100, 0b100, 0b100, 0b111}, // L
  {0b101, 0b111, 0b111, 0b101, 0b101}, // M
  {0b101, 0b111, 0b111, 0b111, 0b101}, // N
  {0b111, 0b101, 0b101, 0b101, 0b111}, // O
  {0b111, 0b101, 0b111, 0b100, 0b100}, // P
  {0b111, 0b101, 0b101, 0b111, 0b001}, // Q
  {0b111, 0b101, 0b111, 0b110, 0b101}, // R
  {0b111, 0b100, 0b111, 0b001, 0b111}, // S
  {0b111, 0b010, 0b010, 0b010, 0b010}, // T
  {0b101, 0b101, 0b101, 0b101, 0b111}, // U
  {0b101, 0b101, 0b101, 0b101, 0b010}, // V
  {0b101, 0b101, 0b111, 0b111, 0b101}, // W
  {0b101, 0b101, 0b010, 0b101, 0b101}, // X
  {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
  {0b111, 0b001, 0b010, 0b100, 0b111}  // Z
};

constexpr uint8_t kCyrillicAlphabetMap[33][kGlyphHeight] = {
  {0b111, 0b101, 0b111, 0b101, 0b101}, // А
  {0b111, 0b100, 0b110, 0b101, 0b111}, // Б
  {0b110, 0b101, 0b110, 0b101, 0b110}, // В
  {0b111, 0b100, 0b100, 0b100, 0b100}, // Г
  {0b011, 0b101, 0b101, 0b101, 0b111}, // Д
  {0b111, 0b100, 0b110, 0b100, 0b111}, // Е
  {0b111, 0b100, 0b110, 0b100, 0b111}, // Ё
  {0b101, 0b101, 0b010, 0b101, 0b101}, // Ж
  {0b111, 0b001, 0b011, 0b001, 0b111}, // З
  {0b101, 0b111, 0b111, 0b111, 0b101}, // И
  {0b101, 0b111, 0b111, 0b111, 0b101}, // Й
  {0b101, 0b101, 0b110, 0b101, 0b101}, // К
  {0b011, 0b101, 0b101, 0b101, 0b101}, // Л
  {0b101, 0b111, 0b111, 0b101, 0b101}, // М
  {0b101, 0b101, 0b111, 0b101, 0b101}, // Н
  {0b111, 0b101, 0b101, 0b101, 0b111}, // О
  {0b111, 0b101, 0b101, 0b101, 0b101}, // П
  {0b111, 0b101, 0b111, 0b100, 0b100}, // Р
  {0b111, 0b100, 0b100, 0b100, 0b111}, // С
  {0b111, 0b010, 0b010, 0b010, 0b010}, // Т
  {0b101, 0b101, 0b010, 0b010, 0b010}, // У
  {0b010, 0b111, 0b111, 0b111, 0b010}, // Ф
  {0b101, 0b101, 0b010, 0b101, 0b101}, // Х
  {0b101, 0b101, 0b101, 0b111, 0b001}, // Ц
  {0b101, 0b101, 0b111, 0b001, 0b001}, // Ч
  {0b101, 0b101, 0b111, 0b111, 0b101}, // Ш
  {0b101, 0b101, 0b111, 0b111, 0b001}, // Щ
  {0b110, 0b010, 0b011, 0b010, 0b111}, // Ъ
  {0b101, 0b101, 0b111, 0b101, 0b111}, // Ы
  {0b100, 0b100, 0b110, 0b101, 0b110}, // Ь
  {0b111, 0b001, 0b111, 0b001, 0b111}, // Э
  {0b101, 0b111, 0b111, 0b111, 0b101}, // Ю
  {0b111, 0b101, 0b111, 0b011, 0b101}  // Я
};

uint32_t toUpperCodepoint(uint32_t codepoint) {
  if (codepoint >= 'a' && codepoint <= 'z') {
    return codepoint - 32u;
  }
  if (codepoint >= 0x430u && codepoint <= 0x44Fu) {
    return codepoint - 0x20u;
  }
  if (codepoint == 0x451u) { // ё
    return 0x401u;
  }
  return codepoint;
}

bool nextUtf8Codepoint(const std::string& text, size_t& index, uint32_t& outCodepoint) {
  if (index >= text.size()) {
    return false;
  }

  unsigned char c0 = static_cast<unsigned char>(text[index++]);
  if ((c0 & 0x80u) == 0) {
    outCodepoint = c0;
    return true;
  }

  auto takeCont = [&](uint32_t& outPart) -> bool {
    if (index >= text.size()) {
      return false;
    }
    unsigned char cx = static_cast<unsigned char>(text[index]);
    if ((cx & 0xC0u) != 0x80u) {
      return false;
    }
    ++index;
    outPart = static_cast<uint32_t>(cx & 0x3Fu);
    return true;
  };

  if ((c0 & 0xE0u) == 0xC0u) {
    uint32_t c1 = 0;
    if (!takeCont(c1)) {
      outCodepoint = '?';
      return true;
    }
    outCodepoint = ((static_cast<uint32_t>(c0) & 0x1Fu) << 6) | c1;
    return true;
  }

  if ((c0 & 0xF0u) == 0xE0u) {
    uint32_t c1 = 0;
    uint32_t c2 = 0;
    if (!takeCont(c1) || !takeCont(c2)) {
      outCodepoint = '?';
      return true;
    }
    outCodepoint = ((static_cast<uint32_t>(c0) & 0x0Fu) << 12) | (c1 << 6) | c2;
    return true;
  }

  if ((c0 & 0xF8u) == 0xF0u) {
    uint32_t c1 = 0;
    uint32_t c2 = 0;
    uint32_t c3 = 0;
    if (!takeCont(c1) || !takeCont(c2) || !takeCont(c3)) {
      outCodepoint = '?';
      return true;
    }
    outCodepoint = ((static_cast<uint32_t>(c0) & 0x07u) << 18) | (c1 << 12) | (c2 << 6) | c3;
    return true;
  }

  outCodepoint = '?';
  return true;
}

const uint8_t* glyphForCodepoint(uint32_t codepoint) {
  if (codepoint >= 'A' && codepoint <= 'Z') {
    return kLatinAlphabetMap[static_cast<size_t>(codepoint - 'A')];
  }
  if (codepoint >= '0' && codepoint <= '9') {
    return kDigitMap[static_cast<size_t>(codepoint - '0')];
  }
  if (codepoint >= 0x410u && codepoint <= 0x415u) {
    return kCyrillicAlphabetMap[static_cast<size_t>(codepoint - 0x410u)];
  }
  if (codepoint == 0x401u) {
    return kCyrillicAlphabetMap[6];
  }
  if (codepoint >= 0x416u && codepoint <= 0x42Fu) {
    return kCyrillicAlphabetMap[static_cast<size_t>(7u + (codepoint - 0x416u))];
  }
  return nullptr;
}

enum class ToolTier : uint8_t {
  kNone = 0,
  kWood = 1,
  kStone = 2,
  kIron = 3
};

enum AchievementId : uint8_t {
  kAchievementGetWood = 0,
  kAchievementCraftPlanks = 1,
  kAchievementCraftSticks = 2,
  kAchievementCraftWorkbench = 3,
  kAchievementCraftWoodPickaxe = 4,
  kAchievementGetStone = 5,
  kAchievementCraftStonePickaxe = 6,
  kAchievementCraftFurnace = 7,
  kAchievementCraftTorch = 8,
  kAchievementFindCave = 9,
  kAchievementSmeltIron = 10,
  kAchievementCraftIronPickaxe = 11,
  kAchievementGetDiamond = 12,
  kAchievementTouchSuspiciousGrass = 13,
  kAchievementDrinkTheFloor = 14,
  kAchievementStoneTree = 15
};

constexpr uint32_t achievementBit(AchievementId id) {
  return 1u << static_cast<uint32_t>(id);
}

struct CraftRecipeDefinition {
  uint8_t output = kAir;
  uint16_t outputCount = 0;
  AchievementId achievement = kAchievementGetWood;
  bool shapeless = false;
  int width = 0;
  int height = 0;
  std::array<uint8_t, 9> cells{};
};

struct CraftMatch {
  bool matched = false;
  uint8_t output = kAir;
  uint16_t outputCount = 0;
  AchievementId achievement = kAchievementGetWood;
  std::array<uint8_t, App::kCraftingSlotCount> useCounts{};
  int maxCrafts = 0;
};

const std::array<CraftRecipeDefinition, 9> kCraftRecipes = {{
  {kPlanks, 4, kAchievementCraftPlanks, true, 1, 1, {kWood, kAir, kAir, kAir, kAir, kAir, kAir, kAir, kAir}},
  {kStick, 4, kAchievementCraftSticks, false, 1, 2, {kPlanks, kPlanks, kAir, kAir, kAir, kAir, kAir, kAir, kAir}},
  {kWorkbench, 1, kAchievementCraftWorkbench, false, 2, 2, {kPlanks, kPlanks, kPlanks, kPlanks, kAir, kAir, kAir, kAir, kAir}},
  {kFurnace, 1, kAchievementCraftFurnace, false, 3, 3, {kStone, kStone, kStone, kStone, kAir, kStone, kStone, kStone, kStone}},
  {kTorch, 4, kAchievementCraftTorch, false, 1, 2, {kCoalOre, kStick, kAir, kAir, kAir, kAir, kAir, kAir, kAir}},
  {kWoodPickaxe, 1, kAchievementCraftWoodPickaxe, false, 3, 3, {kPlanks, kPlanks, kPlanks, kAir, kStick, kAir, kAir, kStick, kAir}},
  {kStonePickaxe, 1, kAchievementCraftStonePickaxe, false, 3, 3, {kStone, kStone, kStone, kAir, kStick, kAir, kAir, kStick, kAir}},
  {kIronPickaxe, 1, kAchievementCraftIronPickaxe, false, 3, 3, {kIronIngot, kIronIngot, kIronIngot, kAir, kStick, kAir, kAir, kStick, kAir}},
  {kBed, 1, kAchievementCraftWorkbench, false, 3, 2, {kWool, kWool, kWool, kPlanks, kPlanks, kPlanks, kAir, kAir, kAir}}
}};

constexpr int craftingSlotIndex(int row, int col) {
  return row * kCraftGridMaxSize + col;
}

struct CraftUiLayout {
  float panelX = 0.0f;
  float panelY = 0.0f;
  float panelWidth = 0.0f;
  float panelHeight = 0.0f;
  float inputX = 0.0f;
  float inputY = 0.0f;
  float inputWidth = 0.0f;
  float inputHeight = 0.0f;
  float resultX = 0.0f;
  float resultY = 0.0f;
  float infoX = 0.0f;
  float infoY = 0.0f;
  int gridSize = 2;
};

struct FurnaceUiLayout {
  float panelX = 0.0f;
  float panelY = 0.0f;
  float panelWidth = 0.0f;
  float panelHeight = 0.0f;
  float inputX = 0.0f;
  float inputY = 0.0f;
  float fuelX = 0.0f;
  float fuelY = 0.0f;
  float outputX = 0.0f;
  float outputY = 0.0f;
  float flameX = 0.0f;
  float flameY = 0.0f;
  float flameW = 0.0f;
  float flameH = 0.0f;
  float progressX = 0.0f;
  float progressY = 0.0f;
  float progressW = 0.0f;
  float progressH = 0.0f;
  float fuelBarX = 0.0f;
  float fuelBarY = 0.0f;
  float fuelBarW = 0.0f;
  float fuelBarH = 0.0f;
  float infoX = 0.0f;
  float infoY = 0.0f;
};

CraftUiLayout makeCraftUiLayout(int framebufferWidth, int framebufferHeight, int gridSize) {
  CraftUiLayout layout;
  layout.gridSize = std::clamp(gridSize, 2, kCraftGridMaxSize);
  const float gridWidth =
    kSlotSize * static_cast<float>(kInventoryCols) +
    kSlotPadding * static_cast<float>(kInventoryCols - 1);
  const float gridHeight =
    kSlotSize * static_cast<float>(kInventoryRows) +
    kSlotPadding * static_cast<float>(kInventoryRows - 1);
  float inventoryX = (static_cast<float>(framebufferWidth) - gridWidth) * 0.5f;
  float inventoryY = (static_cast<float>(framebufferHeight) - gridHeight) * 0.5f - 30.0f;
  inventoryY = std::clamp(inventoryY, 20.0f, static_cast<float>(framebufferHeight) - gridHeight - 20.0f);

  layout.inputWidth =
    kSlotSize * static_cast<float>(layout.gridSize) +
    kSlotPadding * static_cast<float>(layout.gridSize - 1);
  layout.inputHeight = layout.inputWidth;
  layout.panelWidth = gridWidth + kPanelPadding * 2.0f;
  layout.panelHeight = std::max(layout.inputHeight + 72.0f, 202.0f);
  layout.panelX = inventoryX - kPanelPadding;
  layout.panelY = std::max(8.0f, inventoryY - kCraftPanelGap - layout.panelHeight);
  layout.inputX = inventoryX;
  layout.inputY = layout.panelY + kPanelPadding + 18.0f;
  layout.resultX = layout.inputX + layout.inputWidth + kCraftResultGap;
  layout.resultY = layout.inputY + (layout.inputHeight - kSlotSize) * 0.5f;
  layout.infoX = layout.resultX + kSlotSize + 18.0f;
  layout.infoY = layout.inputY + 4.0f;
  return layout;
}

FurnaceUiLayout makeFurnaceUiLayout(int framebufferWidth, int framebufferHeight) {
  FurnaceUiLayout layout;
  const float gridWidth =
    kSlotSize * static_cast<float>(kInventoryCols) +
    kSlotPadding * static_cast<float>(kInventoryCols - 1);
  const float gridHeight =
    kSlotSize * static_cast<float>(kInventoryRows) +
    kSlotPadding * static_cast<float>(kInventoryRows - 1);
  float inventoryX = (static_cast<float>(framebufferWidth) - gridWidth) * 0.5f;
  float inventoryY = (static_cast<float>(framebufferHeight) - gridHeight) * 0.5f - 30.0f;
  inventoryY = std::clamp(inventoryY, 20.0f, static_cast<float>(framebufferHeight) - gridHeight - 20.0f);

  layout.panelWidth = gridWidth + kPanelPadding * 2.0f;
  layout.panelHeight = 190.0f;
  layout.panelX = inventoryX - kPanelPadding;
  layout.panelY = std::max(8.0f, inventoryY - kCraftPanelGap - layout.panelHeight);
  layout.inputX = inventoryX + 24.0f;
  layout.inputY = layout.panelY + 54.0f;
  layout.fuelX = layout.inputX;
  layout.fuelY = layout.inputY + kSlotSize + 24.0f;
  layout.outputX = layout.inputX + 168.0f;
  layout.outputY = layout.inputY + 34.0f;
  layout.flameX = layout.inputX + 54.0f;
  layout.flameY = layout.fuelY + 2.0f;
  layout.flameW = 18.0f;
  layout.flameH = 30.0f;
  layout.progressX = layout.inputX + 84.0f;
  layout.progressY = layout.inputY + 12.0f;
  layout.progressW = 62.0f;
  layout.progressH = 12.0f;
  layout.fuelBarX = layout.progressX;
  layout.fuelBarY = layout.progressY + 36.0f;
  layout.fuelBarW = 62.0f;
  layout.fuelBarH = 12.0f;
  layout.infoX = layout.outputX + kSlotSize + 26.0f;
  layout.infoY = layout.panelY + 46.0f;
  return layout;
}

bool isFurnaceFuel(uint8_t type) {
  switch (type) {
    case kCoalOre:
    case kWood:
    case kPlanks:
    case kStick:
      return true;
    default:
      return false;
  }
}

float furnaceFuelDuration(uint8_t type) {
  switch (type) {
    case kCoalOre:
      return 15.0f;
    case kWood:
      return 8.0f;
    case kPlanks:
      return 6.0f;
    case kStick:
      return 2.8f;
    default:
      return 0.0f;
  }
}

bool furnaceResultForInput(uint8_t inputType, uint8_t& outType) {
  switch (inputType) {
    case kIronOre:
      outType = kIronIngot;
      return true;
    case kRawMutton:
      outType = kCookedMutton;
      return true;
    default:
      outType = kAir;
      return false;
  }
}

std::string furnaceKeyForBlock(const glm::ivec3& block) {
  return std::to_string(block.x) + ":" + std::to_string(block.y) + ":" + std::to_string(block.z);
}

int countNonEmptyCraftSlots(const std::array<ItemStack, App::kCraftingSlotCount>& slots, int gridSize) {
  int count = 0;
  for (int row = 0; row < gridSize; ++row) {
    for (int col = 0; col < gridSize; ++col) {
      const ItemStack& slot = slots[static_cast<size_t>(craftingSlotIndex(row, col))];
      if (slot.type != kAir && slot.count > 0) {
        ++count;
      }
    }
  }
  return count;
}

bool matchCraftRecipe(const CraftRecipeDefinition& recipe,
                      const std::array<ItemStack, App::kCraftingSlotCount>& slots,
                      int activeGridSize,
                      CraftMatch& outMatch) {
  outMatch = {};
  if (activeGridSize < recipe.width || activeGridSize < recipe.height) {
    return false;
  }

  if (recipe.shapeless) {
    std::array<uint8_t, 256> required{};
    int requiredTotal = 0;
    for (uint8_t type : recipe.cells) {
      if (type == kAir) {
        continue;
      }
      ++required[static_cast<size_t>(type)];
      ++requiredTotal;
    }

    if (countNonEmptyCraftSlots(slots, activeGridSize) != requiredTotal) {
      return false;
    }

    int maxCrafts = std::numeric_limits<int>::max();
    for (int row = 0; row < activeGridSize; ++row) {
      for (int col = 0; col < activeGridSize; ++col) {
        int slotIndex = craftingSlotIndex(row, col);
        const ItemStack& slot = slots[static_cast<size_t>(slotIndex)];
        if (slot.type == kAir || slot.count == 0) {
          continue;
        }
        if (required[static_cast<size_t>(slot.type)] == 0) {
          return false;
        }
        outMatch.useCounts[static_cast<size_t>(slotIndex)] = 1;
        maxCrafts = std::min(maxCrafts, static_cast<int>(slot.count));
        --required[static_cast<size_t>(slot.type)];
      }
    }

    for (uint8_t remain : required) {
      if (remain != 0) {
        return false;
      }
    }

    outMatch.matched = true;
    outMatch.output = recipe.output;
    outMatch.outputCount = recipe.outputCount;
    outMatch.achievement = recipe.achievement;
    outMatch.maxCrafts = std::max(1, maxCrafts);
    return true;
  }

  for (int offsetY = 0; offsetY <= activeGridSize - recipe.height; ++offsetY) {
    for (int offsetX = 0; offsetX <= activeGridSize - recipe.width; ++offsetX) {
      CraftMatch candidate{};
      bool ok = true;
      int maxCrafts = std::numeric_limits<int>::max();
      for (int row = 0; row < activeGridSize && ok; ++row) {
        for (int col = 0; col < activeGridSize; ++col) {
          int slotIndex = craftingSlotIndex(row, col);
          const ItemStack& slot = slots[static_cast<size_t>(slotIndex)];
          uint8_t expected = kAir;
          if (row >= offsetY && row < offsetY + recipe.height &&
              col >= offsetX && col < offsetX + recipe.width) {
            int recipeIndex = (row - offsetY) * recipe.width + (col - offsetX);
            expected = recipe.cells[static_cast<size_t>(recipeIndex)];
          }

          if (expected == kAir) {
            if (slot.type != kAir && slot.count > 0) {
              ok = false;
              break;
            }
            continue;
          }

          if (slot.type != expected || slot.count == 0) {
            ok = false;
            break;
          }

          candidate.useCounts[static_cast<size_t>(slotIndex)] = 1;
          maxCrafts = std::min(maxCrafts, static_cast<int>(slot.count));
        }
      }

      if (!ok) {
        continue;
      }

      candidate.matched = true;
      candidate.output = recipe.output;
      candidate.outputCount = recipe.outputCount;
      candidate.achievement = recipe.achievement;
      candidate.maxCrafts = std::max(1, maxCrafts);
      outMatch = candidate;
      return true;
    }
  }

  return false;
}

bool findCraftMatch(const std::array<ItemStack, App::kCraftingSlotCount>& slots,
                    int activeGridSize,
                    CraftMatch& outMatch) {
  for (const CraftRecipeDefinition& recipe : kCraftRecipes) {
    if (matchCraftRecipe(recipe, slots, activeGridSize, outMatch)) {
      return true;
    }
  }
  outMatch = {};
  return false;
}

ToolTier toolTierForItem(uint8_t type) {
  switch (type) {
    case kWoodPickaxe:
      return ToolTier::kWood;
    case kStonePickaxe:
      return ToolTier::kStone;
    case kIronPickaxe:
      return ToolTier::kIron;
    default:
      return ToolTier::kNone;
  }
}

ToolTier requiredToolTierForBlock(uint8_t type) {
  switch (type) {
    case kStone:
    case kCoalOre:
      return ToolTier::kWood;
    case kIronOre:
      return ToolTier::kStone;
    case kGoldOre:
    case kDiamondOre:
      return ToolTier::kIron;
    default:
      return ToolTier::kNone;
  }
}

bool canMineBlock(uint8_t type, ToolTier toolTier) {
  return static_cast<int>(toolTier) >= static_cast<int>(requiredToolTierForBlock(type));
}

bool breaksInstantly(uint8_t type) {
  return isUnderwaterPlantBlock(type);
}

float breakDurationForBlock(uint8_t type, ToolTier toolTier, bool inWater) {
  if (isBedBlock(type)) {
    float duration = 0.42f;
    if (inWater) {
      duration *= 2.0f;
    }
    return duration;
  }
  if (isTorchBlock(type)) {
    float duration = 0.16f;
    if (inWater) {
      duration *= 2.0f;
    }
    return duration;
  }

  float duration = kBreakDuration;
  switch (type) {
    case kLeaves:
    case kSeagrass:
    case kCoral:
      duration = 0.18f;
      break;
    case kSuspiciousGlass:
      duration = 0.34f;
      break;
    case kWood:
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
    case kLootCache:
      duration = 0.72f;
      break;
    case kPlanks:
      duration = 0.52f;
      break;
    case kStone:
    case kCoalOre:
      duration = !canMineBlock(type, toolTier) ? 2.8f
                 : toolTier == ToolTier::kWood ? 0.86f
                 : toolTier == ToolTier::kStone ? 0.54f
                                                : 0.36f;
      break;
    case kIronOre:
      duration = !canMineBlock(type, toolTier) ? 4.2f
                 : toolTier == ToolTier::kStone ? 1.08f
                                                : 0.70f;
      break;
    case kGoldOre:
      duration = !canMineBlock(type, toolTier) ? 4.8f : 0.82f;
      break;
    case kDiamondOre:
      duration = !canMineBlock(type, toolTier) ? 5.0f : 0.92f;
      break;
    default:
      break;
  }

  if (inWater) {
    duration *= 2.0f;
  }
  return duration;
}

uint8_t droppedItemForBlock(uint8_t type) {
  switch (type) {
    case kDiamondOre:
      return kDiamond;
    case kLootCache:
      return kAir;
    default:
      return itemTypeForPlacedBlock(type);
  }
}

bool shouldDropBrokenBlock(uint8_t type, ToolTier toolTier) {
  if (type == kLootCache) {
    return false;
  }
  ToolTier required = requiredToolTierForBlock(type);
  return required == ToolTier::kNone || canMineBlock(type, toolTier);
}

std::string achievementTitle(AchievementId id, bool russian) {
  switch (id) {
    case kAchievementGetWood:
      return russian ? "ПЕРВОЕ ДЕРЕВО" : "FIRST WOOD";
    case kAchievementCraftPlanks:
      return russian ? "ДОСКИ" : "PLANKS";
    case kAchievementCraftSticks:
      return russian ? "ПАЛКИ" : "STICKS";
    case kAchievementCraftWorkbench:
      return russian ? "ВЕРСТАК" : "WORKBENCH";
    case kAchievementCraftWoodPickaxe:
      return russian ? "ДЕРЕВЯННАЯ КИРКА" : "WOOD PICKAXE";
    case kAchievementGetStone:
      return russian ? "КАМЕННЫЙ ВЕК" : "STONE AGE";
    case kAchievementCraftStonePickaxe:
      return russian ? "КАМЕННАЯ КИРКА" : "STONE PICKAXE";
    case kAchievementCraftFurnace:
      return russian ? "ПЕЧКА" : "FURNACE";
    case kAchievementCraftTorch:
      return russian ? "ФАКЕЛЫ" : "TORCHES";
    case kAchievementFindCave:
      return russian ? "В ПЕЩЕРУ" : "INTO THE CAVE";
    case kAchievementSmeltIron:
      return russian ? "ПЛАВКА ЖЕЛЕЗА" : "SMELT IRON";
    case kAchievementCraftIronPickaxe:
      return russian ? "ЖЕЛЕЗНАЯ КИРКА" : "IRON PICKAXE";
    case kAchievementGetDiamond:
      return russian ? "АЛМАЗЫ" : "DIAMONDS";
    case kAchievementTouchSuspiciousGrass:
      return russian ? "ПОТРОГАЙ ПОДОЗРИТЕЛЬНУЮ ТРАВУ" : "TOUCH SUSPICIOUS GRASS";
    case kAchievementDrinkTheFloor:
      return russian ? "ВЫПЕЙ ПОЛ" : "DRINK THE FLOOR";
    case kAchievementStoneTree:
      return russian ? "ПОЧЕМУ ДЕРЕВО ИЗ КАМНЯ?" : "WHY IS THE TREE MADE OF STONE?";
    default:
      return russian ? "ДОСТИЖЕНИЕ" : "ACHIEVEMENT";
  }
}

std::string achievementDescription(AchievementId id, bool russian) {
  switch (id) {
    case kAchievementGetWood:
      return russian ? "СРУБИ ПЕРВОЕ ДЕРЕВО" : "CHOP YOUR FIRST TREE";
    case kAchievementCraftPlanks:
      return russian ? "СДЕЛАЙ ДОСКИ ИЗ ДЕРЕВА" : "CRAFT PLANKS FROM WOOD";
    case kAchievementCraftSticks:
      return russian ? "СДЕЛАЙ ПАЛКИ ИЗ ДОСОК" : "CRAFT STICKS FROM PLANKS";
    case kAchievementCraftWorkbench:
      return russian ? "СКРАФТИ И ПОСТАВЬ ВЕРСТАК" : "CRAFT AND PLACE A WORKBENCH";
    case kAchievementCraftWoodPickaxe:
      return russian ? "СДЕЛАЙ ПЕРВУЮ КИРКУ" : "CRAFT YOUR FIRST PICKAXE";
    case kAchievementGetStone:
      return russian ? "ДОБУДЬ КАМЕНЬ КИРКОЙ" : "MINE STONE WITH A PICKAXE";
    case kAchievementCraftStonePickaxe:
      return russian ? "УЛУЧШИ КИРКУ ДО КАМНЯ" : "UPGRADE TO STONE";
    case kAchievementCraftFurnace:
      return russian ? "СКРАФТИ ПЕЧКУ ИЗ КАМНЯ" : "CRAFT A FURNACE FROM STONE";
    case kAchievementCraftTorch:
      return russian ? "СОБЕРИ ФАКЕЛЫ ДЛЯ ПЕЩЕР" : "CRAFT TORCHES FOR CAVES";
    case kAchievementFindCave:
      return russian ? "СПУСТИСЬ В ПЕЩЕРУ" : "GO DOWN INTO A CAVE";
    case kAchievementSmeltIron:
      return russian ? "ПЕРЕПЛАВЬ ЖЕЛЕЗНУЮ РУДУ" : "SMELT IRON ORE";
    case kAchievementCraftIronPickaxe:
      return russian ? "СОБЕРИ ЖЕЛЕЗНУЮ КИРКУ" : "CRAFT AN IRON PICKAXE";
    case kAchievementGetDiamond:
      return russian ? "ДОБУДЬ ПЕРВЫЙ АЛМАЗ" : "MINE YOUR FIRST DIAMOND";
    case kAchievementTouchSuspiciousGrass:
      return russian ? "ПРОЙДИСЬ ПО АПРЕЛЬСКОЙ ТРАВЕ" : "STEP ON APRIL FOOLS GRASS";
    case kAchievementDrinkTheFloor:
      return russian ? "ОКУНИСЬ В ЖИДКУЮ ЗЕМЛЮ" : "SWIM IN THE LIQUID GROUND";
    case kAchievementStoneTree:
      return russian ? "НАЙДИ ДЕРЕВО С КАМЕННЫМ СТВОЛОМ" : "FIND A TREE WITH A STONE TRUNK";
    default:
      return russian ? "ИДИ ПО ЦЕПОЧКЕ КРАФТА" : "FOLLOW THE CRAFTING CHAIN";
  }
}

int achievementIconTile(AchievementId id) {
  switch (id) {
    case kAchievementGetWood:
      return kTileWood;
    case kAchievementCraftPlanks:
      return kTilePlanks;
    case kAchievementCraftSticks:
      return kTileStick;
    case kAchievementCraftWorkbench:
      return kTileWorkbench;
    case kAchievementCraftWoodPickaxe:
      return kTileWoodPickaxe;
    case kAchievementGetStone:
      return kTileStone;
    case kAchievementCraftStonePickaxe:
      return kTileStonePickaxe;
    case kAchievementCraftFurnace:
      return kTileFurnaceFront;
    case kAchievementCraftTorch:
      return kTileTorch;
    case kAchievementFindCave:
      return kTileCoalOre;
    case kAchievementSmeltIron:
      return kTileIronIngot;
    case kAchievementCraftIronPickaxe:
      return kTileIronPickaxe;
    case kAchievementGetDiamond:
      return kTileDiamond;
    case kAchievementTouchSuspiciousGrass:
      return kTileGrassTop;
    case kAchievementDrinkTheFloor:
      return kTileWater;
    case kAchievementStoneTree:
      return kTileStone;
    default:
      return kTileStone;
  }
}

struct AchievementTreeNodeView {
  AchievementId id;
  float x;
  float y;
  uint32_t prereqMask;
};

struct AchievementTreeUiLayout {
  float frameX = 0.0f;
  float frameY = 0.0f;
  float frameW = 0.0f;
  float frameH = 0.0f;
  float headerX = 0.0f;
  float headerY = 0.0f;
  float headerW = 0.0f;
  float headerH = 0.0f;
  float viewportX = 0.0f;
  float viewportY = 0.0f;
  float viewportW = 0.0f;
  float viewportH = 0.0f;
  float contentPadX = 56.0f;
  float contentPadY = 54.0f;
  float spacingX = 88.0f;
  float spacingY = 94.0f;
  float contentW = 0.0f;
  float contentH = 0.0f;
  float graphW = 0.0f;
  float graphH = 0.0f;
};

constexpr std::array<AchievementTreeNodeView, App::kAchievementCount> kAchievementTreeNodes = {{
  {kAchievementGetWood, 0.0f, 1.0f, 0u},
  {kAchievementCraftPlanks, 1.2f, 1.0f, achievementBit(kAchievementGetWood)},
  {kAchievementCraftSticks, 2.4f, 0.0f, achievementBit(kAchievementCraftPlanks)},
  {kAchievementCraftWorkbench, 2.4f, 2.0f, achievementBit(kAchievementCraftPlanks)},
  {kAchievementCraftWoodPickaxe, 3.8f, 1.0f, achievementBit(kAchievementCraftSticks) | achievementBit(kAchievementCraftWorkbench)},
  {kAchievementGetStone, 5.2f, 1.0f, achievementBit(kAchievementCraftWoodPickaxe)},
  {kAchievementCraftStonePickaxe, 6.4f, 2.0f, achievementBit(kAchievementGetStone)},
  {kAchievementCraftFurnace, 6.4f, 0.0f, achievementBit(kAchievementGetStone)},
  {kAchievementCraftTorch, 7.7f, 0.0f, achievementBit(kAchievementCraftSticks) | achievementBit(kAchievementCraftFurnace)},
  {kAchievementFindCave, 7.7f, 2.0f, achievementBit(kAchievementGetStone)},
  {kAchievementSmeltIron, 9.0f, 1.0f, achievementBit(kAchievementCraftFurnace) | achievementBit(kAchievementFindCave)},
  {kAchievementCraftIronPickaxe, 10.3f, 2.0f, achievementBit(kAchievementSmeltIron)},
  {kAchievementGetDiamond, 11.6f, 1.0f, achievementBit(kAchievementCraftIronPickaxe)},
  {kAchievementTouchSuspiciousGrass, 1.2f, 3.2f, achievementBit(kAchievementGetWood)},
  {kAchievementDrinkTheFloor, 2.6f, 3.2f, achievementBit(kAchievementTouchSuspiciousGrass)},
  {kAchievementStoneTree, 4.0f, 3.2f, achievementBit(kAchievementTouchSuspiciousGrass)}
}};

constexpr std::array<std::pair<int, int>, 18> kAchievementTreeEdges = {{
  {0, 1},
  {1, 2},
  {1, 3},
  {2, 4},
  {3, 4},
  {4, 5},
  {5, 6},
  {5, 7},
  {2, 8},
  {7, 8},
  {5, 9},
  {7, 10},
  {9, 10},
  {10, 11},
  {11, 12},
  {0, 13},
  {13, 14},
  {13, 15}
}};

AchievementTreeUiLayout makeAchievementTreeLayout(int uiWidth, int uiHeight) {
  AchievementTreeUiLayout layout;
  float maxFrameW = std::max(480.0f, static_cast<float>(uiWidth) - 16.0f);
  float maxFrameH = std::max(360.0f, static_cast<float>(uiHeight) - 16.0f);
  layout.frameW = std::min(1160.0f, std::max(640.0f, static_cast<float>(uiWidth) - 72.0f));
  layout.frameH = std::min(760.0f, std::max(420.0f, static_cast<float>(uiHeight) - 72.0f));
  layout.frameW = std::min(layout.frameW, maxFrameW);
  layout.frameH = std::min(layout.frameH, maxFrameH);
  layout.frameX = (static_cast<float>(uiWidth) - layout.frameW) * 0.5f;
  layout.frameY = std::max(10.0f, (static_cast<float>(uiHeight) - layout.frameH) * 0.5f + 18.0f);
  layout.headerX = layout.frameX + 20.0f;
  layout.headerY = layout.frameY + 18.0f;
  layout.headerW = layout.frameW - 40.0f;
  layout.headerH = 54.0f;
  layout.viewportX = layout.frameX + 28.0f;
  layout.viewportY = layout.frameY + 86.0f;
  layout.viewportW = layout.frameW - 56.0f;
  layout.viewportH = layout.frameH - 134.0f;
  float maxNodeX = 0.0f;
  float maxNodeY = 0.0f;
  for (const AchievementTreeNodeView& node : kAchievementTreeNodes) {
    maxNodeX = std::max(maxNodeX, node.x);
    maxNodeY = std::max(maxNodeY, node.y);
  }
  layout.graphW = maxNodeX * layout.spacingX + kAchievementNodeSize;
  layout.graphH = maxNodeY * layout.spacingY + kAchievementNodeSize;
  layout.contentW = layout.graphW + layout.contentPadX * 2.0f + 72.0f;
  layout.contentH = std::max(layout.viewportH + 1.0f, layout.graphH + layout.contentPadY * 2.0f + 28.0f);
  return layout;
}

glm::vec2 clampAchievementTreeScroll(const AchievementTreeUiLayout& layout, glm::vec2 scroll) {
  float maxX = std::max(0.0f, layout.contentW - layout.viewportW);
  float maxY = std::max(0.0f, layout.contentH - layout.viewportH);
  return {
    std::clamp(scroll.x, 0.0f, maxX),
    std::clamp(scroll.y, 0.0f, maxY)
  };
}

glm::vec4 achievementTreeTabRect(const AchievementTreeUiLayout& layout, int index) {
  float tabW = 62.0f;
  float tabH = 52.0f;
  float gap = 10.0f;
  float x = layout.frameX + 18.0f + static_cast<float>(index) * (tabW + gap);
  float y = layout.frameY - 28.0f + (index == 0 ? -4.0f : 2.0f);
  return {x, y, tabW, tabH};
}

glm::vec2 achievementTreeTabTargetScroll(const AchievementTreeUiLayout& layout, int index) {
  float maxX = std::max(0.0f, layout.contentW - layout.viewportW);
  if (index <= 0) {
    return {0.0f, 0.0f};
  }
  if (index == 1) {
    return {std::min(maxX, maxX * 0.46f), 0.0f};
  }
  return {maxX, 0.0f};
}

int activeAchievementTreeTab(const AchievementTreeUiLayout& layout, float scrollX) {
  float maxX = std::max(1.0f, layout.contentW - layout.viewportW);
  float t = std::clamp(scrollX / maxX, 0.0f, 1.0f);
  if (t < 0.33f) {
    return 0;
  }
  if (t < 0.72f) {
    return 1;
  }
  return 2;
}

uint8_t placementFacingFromLook(const glm::vec3& lookDir) {
  glm::vec2 horizontal(-lookDir.x, -lookDir.z);
  if (std::abs(horizontal.x) > std::abs(horizontal.y)) {
    return horizontal.x >= 0.0f ? kFacingEast : kFacingWest;
  }
  return horizontal.y >= 0.0f ? kFacingSouth : kFacingNorth;
}

uint8_t facingFromPlacementNormal(const glm::ivec3& normal) {
  if (normal.x > 0) {
    return kFacingEast;
  }
  if (normal.x < 0) {
    return kFacingWest;
  }
  if (normal.z > 0) {
    return kFacingSouth;
  }
  return kFacingNorth;
}

uint8_t placedBlockTypeForItem(uint8_t itemType,
                               const glm::vec3& lookDir,
                               const glm::ivec3& placementNormal) {
  uint8_t facing = placementFacingFromLook(lookDir);
  if (itemType == kBed) {
    return bedBlockForFacing(facing, false);
  }
  if (itemType == kWorkbench) {
    return workbenchBlockForFacing(facing);
  }
  if (itemType == kFurnace) {
    return furnaceBlockForFacing(facing);
  }
  if (itemType == kTorch) {
    if (placementNormal.y > 0) {
      return kTorch;
    }
    if (placementNormal.x != 0 || placementNormal.z != 0) {
      return torchBlockForFacing(facingFromPlacementNormal(placementNormal));
    }
    return kAir;
  }
  return itemType;
}

bool hasTorchSupportAt(const World& world, int x, int y, int z, uint8_t torchType) {
  glm::ivec3 supportOffset = torchSupportOffset(torchType);
  uint8_t support = world.getBlock(x + supportOffset.x, y + supportOffset.y, z + supportOffset.z);
  return support != kAir && !isWaterBlock(support) && !isDecorationBlock(support);
}

bool targetCanHostUnderwaterPlant(uint8_t targetType) {
  return isWaterBlock(targetType) || isUnderwaterPlantBlock(targetType);
}

bool hasUnderwaterPlantSupportAt(const World& world,
                                 int x,
                                 int y,
                                 int z,
                                 uint8_t plantType,
                                 uint8_t targetType) {
  if (!isUnderwaterPlantBlock(plantType)) {
    return true;
  }
  if (!targetCanHostUnderwaterPlant(targetType)) {
    return false;
  }

  uint8_t ground = world.getBlock(x, y - 1, z);
  if (ground == kAir || isWaterBlock(ground) || isDecorationBlock(ground)) {
    return false;
  }
  return canSupportUnderwaterPlant(plantType, ground);
}

bool hasPlacementSupportAt(const World& world,
                           int x,
                           int y,
                           int z,
                           uint8_t placedType,
                           uint8_t targetType) {
  if (isTorchBlock(placedType)) {
    return hasTorchSupportAt(world, x, y, z, placedType);
  }
  if (isUnderwaterPlantBlock(placedType)) {
    return hasUnderwaterPlantSupportAt(world, x, y, z, placedType, targetType);
  }
  return true;
}

glm::vec3 torchLightWorldPosition(int x, int y, int z, uint8_t torchType) {
  return glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) +
         torchLightOffset(torchType);
}

enum class BlockAudioMaterial : uint8_t {
  kDirt = 0,
  kSand = 1,
  kWood = 2,
  kStone = 3
};

enum class FootstepSurface : uint8_t {
  kNone = 0,
  kGrass = 1,
  kStone = 2
};

constexpr int kGrassFootstepVariantCount = 6;
constexpr int kStoneFootstepVariantCount = 3;

BlockAudioMaterial audioMaterialForBlock(uint8_t type) {
  if (isBedBlock(type)) {
    return BlockAudioMaterial::kWood;
  }
  if (isWorkbenchBlock(type) ||
      type == kWood ||
      type == kPlanks ||
      type == kLootCache ||
      type == kBed ||
      isTorchBlock(type)) {
    return BlockAudioMaterial::kWood;
  }
  if (isFurnaceBlock(type) ||
      type == kStone ||
      type == kSuspiciousGlass ||
      type == kCoalOre ||
      type == kIronOre ||
      type == kGoldOre ||
      type == kDiamondOre) {
    return BlockAudioMaterial::kStone;
  }
  if (type == kSand || type == kSeagrass || type == kCoral) {
    return BlockAudioMaterial::kSand;
  }
  return BlockAudioMaterial::kDirt;
}

FootstepSurface footstepSurfaceForBlock(uint8_t type) {
  if (type == kAir || isWaterBlock(type) || isDecorationBlock(type)) {
    return FootstepSurface::kNone;
  }

  switch (audioMaterialForBlock(type)) {
    case BlockAudioMaterial::kStone:
    case BlockAudioMaterial::kWood:
      return FootstepSurface::kStone;
    case BlockAudioMaterial::kSand:
    case BlockAudioMaterial::kDirt:
    default:
      return FootstepSurface::kGrass;
  }
}

FootstepSurface footstepSurfaceUnderPlayer(const World& world, const glm::vec3& playerPos) {
  constexpr std::array<glm::vec2, 5> kFootSamples{{
    {0.0f, 0.0f},
    {0.22f, 0.22f},
    {-0.22f, 0.22f},
    {0.22f, -0.22f},
    {-0.22f, -0.22f}
  }};

  auto sampleSurface = [&](float offsetX, float offsetZ) {
    int x = static_cast<int>(std::floor(playerPos.x + offsetX));
    int y = static_cast<int>(std::floor(playerPos.y - 0.08f));
    int z = static_cast<int>(std::floor(playerPos.z + offsetZ));
    if (!world.inBounds(x, y, z)) {
      return FootstepSurface::kNone;
    }

    uint8_t block = world.getBlock(x, y, z);
    if ((block == kAir || isWaterBlock(block) || isDecorationBlock(block)) &&
        world.inBounds(x, y - 1, z)) {
      block = world.getBlock(x, y - 1, z);
    }
    return footstepSurfaceForBlock(block);
  };

  FootstepSurface centerSurface = sampleSurface(0.0f, 0.0f);
  int grassHits = 0;
  int stoneHits = 0;
  for (const glm::vec2& sample : kFootSamples) {
    FootstepSurface surface = sampleSurface(sample.x, sample.y);
    if (surface == FootstepSurface::kGrass) {
      ++grassHits;
    } else if (surface == FootstepSurface::kStone) {
      ++stoneHits;
    }
  }

  if (grassHits == 0 && stoneHits == 0) {
    return FootstepSurface::kNone;
  }
  if (grassHits == stoneHits) {
    return centerSurface;
  }
  return stoneHits > grassHits ? FootstepSurface::kStone : FootstepSurface::kGrass;
}

uint32_t advanceFootstepRng(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

int chooseFootstepVariant(uint32_t& rngState, int variantCount, int previousVariant) {
  if (variantCount <= 1) {
    return 0;
  }

  uint32_t roll = advanceFootstepRng(rngState);
  int variant = static_cast<int>(roll % static_cast<uint32_t>(variantCount));
  if (variant == previousVariant) {
    variant = (variant + 1 + static_cast<int>((roll / static_cast<uint32_t>(variantCount)) %
                                              static_cast<uint32_t>(variantCount - 1))) % variantCount;
  }
  return variant;
}

AudioSystem::Cue footstepCueForVariant(FootstepSurface surface, int variant) {
  if (surface == FootstepSurface::kGrass) {
    int clamped = std::clamp(variant, 0, kGrassFootstepVariantCount - 1);
    return static_cast<AudioSystem::Cue>(static_cast<int>(AudioSystem::Cue::kFootstepGrass1) + clamped);
  }
  int clamped = std::clamp(variant, 0, kStoneFootstepVariantCount - 1);
  return static_cast<AudioSystem::Cue>(static_cast<int>(AudioSystem::Cue::kFootstepStone1) + clamped);
}

AudioSystem::Cue breakCueForBlock(uint8_t type) {
  switch (audioMaterialForBlock(type)) {
    case BlockAudioMaterial::kSand:
      return AudioSystem::Cue::kBlockBreakSand;
    case BlockAudioMaterial::kWood:
      return AudioSystem::Cue::kBlockBreakWood;
    case BlockAudioMaterial::kStone:
      return AudioSystem::Cue::kBlockBreakStone;
    case BlockAudioMaterial::kDirt:
    default:
      return AudioSystem::Cue::kBlockBreakDirt;
  }
}

AudioSystem::Cue placeCueForBlock(uint8_t type) {
  switch (audioMaterialForBlock(type)) {
    case BlockAudioMaterial::kSand:
      return AudioSystem::Cue::kBlockPlaceSand;
    case BlockAudioMaterial::kWood:
      return AudioSystem::Cue::kBlockPlaceWood;
    case BlockAudioMaterial::kStone:
      return AudioSystem::Cue::kBlockPlaceStone;
    case BlockAudioMaterial::kDirt:
    default:
      return AudioSystem::Cue::kBlockPlaceDirt;
  }
}

float breakGainForBlock(uint8_t type) {
  switch (audioMaterialForBlock(type)) {
    case BlockAudioMaterial::kSand:
      return 1.06f;
    case BlockAudioMaterial::kWood:
      return 0.98f;
    case BlockAudioMaterial::kStone:
      return 0.94f;
    case BlockAudioMaterial::kDirt:
    default:
      return 1.0f;
  }
}

float placeGainForBlock(uint8_t type) {
  switch (audioMaterialForBlock(type)) {
    case BlockAudioMaterial::kSand:
      return 0.98f;
    case BlockAudioMaterial::kWood:
      return 0.92f;
    case BlockAudioMaterial::kStone:
      return 0.88f;
    case BlockAudioMaterial::kDirt:
    default:
      return 0.94f;
  }
}

int tileForBlock(uint8_t type) {
  if (isWaterBlock(type)) {
    return kTileWater;
  }
  if (isTorchBlock(type)) {
    return kTileTorch;
  }
  if (type == kBed || isBedBlock(type)) {
    return kTileBed;
  }

  switch (type) {
    case kGrass:
      return kTileGrassTop;
    case kDirt:
      return kTileDirt;
    case kSand:
      return kTileSand;
    case kGravel:
      return kTileGravel;
    case kWood:
      return kTileWood;
    case kLeaves:
      return kTileLeaves;
    case kSeagrass:
      return kTileSeagrass;
    case kCoral:
      return kTileCoral;
    case kCoalOre:
      return kTileCoalOre;
    case kIronOre:
      return kTileIronOre;
    case kGoldOre:
      return kTileGoldOre;
    case kDiamondOre:
      return kTileDiamondOre;
    case kSuspiciousGlass:
      return kTileSuspiciousGlass;
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
      return kTileWorkbench;
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
      return kTileFurnaceFront;
    case kLootCache:
      return kTileLootCache;
    case kWool:
      return kTileWool;
    case kRawMutton:
      return kTileRawMutton;
    case kCookedMutton:
      return kTileCookedMutton;
    case kPlanks:
      return kTilePlanks;
    case kStick:
      return kTileStick;
    case kIronIngot:
      return kTileIronIngot;
    case kDiamond:
      return kTileDiamond;
    case kWoodPickaxe:
      return kTileWoodPickaxe;
    case kStonePickaxe:
      return kTileStonePickaxe;
    case kIronPickaxe:
      return kTileIronPickaxe;
    case kStone:
      return kTileStone;
    default:
      return kTileStone;
  }
}

std::string displayNameForBlock(uint8_t type, bool russian) {
  if (isWaterBlock(type)) {
    return russian ? "ВОДА" : "WATER";
  }
  if (isTorchBlock(type)) {
    return russian ? "ФАКЕЛ" : "TORCH";
  }
  if (type == kBed || isBedBlock(type)) {
    return russian ? "КРОВАТЬ" : "BED";
  }

  switch (type) {
    case kAir:
      return russian ? "ПУСТЫЕ РУКИ" : "EMPTY HAND";
    case kGrass:
      return russian ? "ТРАВА" : "GRASS BLOCK";
    case kDirt:
      return russian ? "ЗЕМЛЯ" : "DIRT";
    case kStone:
      return russian ? "КАМЕНЬ" : "STONE";
    case kSuspiciousGlass:
      return russian ? "ПОДОЗРИТЕЛЬНОЕ СТЕКЛО" : "SUSPICIOUS GLASS";
    case kSand:
      return russian ? "ПЕСОК" : "SAND";
    case kGravel:
      return russian ? "ГРАВИЙ" : "GRAVEL";
    case kWood:
      return russian ? "ДЕРЕВО" : "WOOD";
    case kPlanks:
      return russian ? "ДОСКИ" : "PLANKS";
    case kWool:
      return russian ? "ШЕРСТЬ" : "WOOL";
    case kRawMutton:
      return russian ? "СЫРАЯ БАРАНИНА" : "RAW MUTTON";
    case kCookedMutton:
      return russian ? "ЖАРЕНАЯ БАРАНИНА" : "COOKED MUTTON";
    case kStick:
      return russian ? "ПАЛКИ" : "STICKS";
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
      return russian ? "ВЕРСТАК" : "WORKBENCH";
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
      return russian ? "ПЕЧКА" : "FURNACE";
    case kLootCache:
      return russian ? "СУНДУК" : "CHEST";
    case kLeaves:
      return russian ? "ЛИСТВА" : "LEAVES";
    case kSeagrass:
      return russian ? "МОРСКАЯ ТРАВА" : "SEAGRASS";
    case kCoral:
      return russian ? "КОРАЛЛ" : "CORAL";
    case kCoalOre:
      return russian ? "УГОЛЬНАЯ РУДА" : "COAL ORE";
    case kIronOre:
      return russian ? "ЖЕЛЕЗНАЯ РУДА" : "IRON ORE";
    case kIronIngot:
      return russian ? "ЖЕЛЕЗО" : "IRON INGOT";
    case kGoldOre:
      return russian ? "ЗОЛОТАЯ РУДА" : "GOLD ORE";
    case kDiamondOre:
      return russian ? "АЛМАЗНАЯ РУДА" : "DIAMOND ORE";
    case kDiamond:
      return russian ? "АЛМАЗ" : "DIAMOND";
    case kWoodPickaxe:
      return russian ? "ДЕРЕВЯННАЯ КИРКА" : "WOOD PICKAXE";
    case kStonePickaxe:
      return russian ? "КАМЕННАЯ КИРКА" : "STONE PICKAXE";
    case kIronPickaxe:
      return russian ? "ЖЕЛЕЗНАЯ КИРКА" : "IRON PICKAXE";
    default:
      return russian ? "БЛОК" : "BLOCK";
  }
}

std::string normalizeCommandToken(const std::string& value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (char c : value) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc)) {
      normalized.push_back(static_cast<char>(std::tolower(uc)));
    }
  }
  return normalized;
}

uint8_t itemTypeForCommandToken(const std::string& token) {
  const std::string key = normalizeCommandToken(token);
  if (key == "grass" || key == "grassblock") return kGrass;
  if (key == "dirt") return kDirt;
  if (key == "stone") return kStone;
  if (key == "sand") return kSand;
  if (key == "gravel") return kGravel;
  if (key == "wood" || key == "log") return kWood;
  if (key == "leaves" || key == "leaf") return kLeaves;
  if (key == "water") return kWater;
  if (key == "coalore" || key == "coal") return kCoalOre;
  if (key == "ironore") return kIronOre;
  if (key == "goldore") return kGoldOre;
  if (key == "diamondore") return kDiamondOre;
  if (key == "seagrass") return kSeagrass;
  if (key == "coral") return kCoral;
  if (key == "workbench" || key == "craftingtable") return kWorkbench;
  if (key == "furnace") return kFurnace;
  if (key == "planks") return kPlanks;
  if (key == "wool") return kWool;
  if (key == "rawmutton" || key == "mutton") return kRawMutton;
  if (key == "cookedmutton" || key == "cookedmeat" || key == "roastmutton") return kCookedMutton;
  if (key == "bed") return kBed;
  if (key == "stick" || key == "sticks") return kStick;
  if (key == "torch" || key == "torches") return kTorch;
  if (key == "ironingot" || key == "iron") return kIronIngot;
  if (key == "diamond") return kDiamond;
  if (key == "woodpickaxe" || key == "woodpick") return kWoodPickaxe;
  if (key == "stonepickaxe" || key == "stonepick") return kStonePickaxe;
  if (key == "ironpickaxe" || key == "ironpick") return kIronPickaxe;
  return kAir;
}

bool isEdibleItem(uint8_t type) {
  return type == kRawMutton || type == kCookedMutton;
}

int foodHealAmount(uint8_t type) {
  switch (type) {
    case kRawMutton:
      return 3;
    case kCookedMutton:
      return 8;
    default:
      return 0;
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

float fract01(float value) {
  return value - std::floor(value);
}

glm::vec3 droppedItemColor(uint8_t type) {
  if (isWaterBlock(type)) {
    return {0.22f, 0.45f, 0.88f};
  }
  if (isTorchBlock(type)) {
    return {0.98f, 0.78f, 0.34f};
  }
  if (type == kBed || isBedBlock(type)) {
    return {0.86f, 0.24f, 0.22f};
  }

  switch (type) {
    case kGrass:
      return {0.20f, 0.80f, 0.20f};
    case kDirt:
      return {0.55f, 0.35f, 0.20f};
    case kSand:
      return {0.86f, 0.78f, 0.50f};
    case kGravel:
      return {0.46f, 0.44f, 0.42f};
    case kSuspiciousGlass:
      return {0.58f, 0.96f, 0.86f};
    case kWood:
      return {0.49f, 0.33f, 0.16f};
    case kLeaves:
      return {0.16f, 0.58f, 0.16f};
    case kSeagrass:
      return {0.14f, 0.62f, 0.34f};
    case kCoral:
      return {0.88f, 0.48f, 0.40f};
    case kCoalOre:
      return {0.22f, 0.22f, 0.22f};
    case kIronOre:
      return {0.73f, 0.54f, 0.40f};
    case kIronIngot:
      return {0.84f, 0.86f, 0.90f};
    case kGoldOre:
      return {0.92f, 0.75f, 0.22f};
    case kDiamondOre:
    case kDiamond:
      return {0.32f, 0.92f, 0.98f};
    case kWorkbench:
    case kWorkbenchNorth:
    case kWorkbenchEast:
    case kWorkbenchSouth:
    case kWorkbenchWest:
      return {0.62f, 0.42f, 0.18f};
    case kWool:
      return {0.94f, 0.94f, 0.96f};
    case kRawMutton:
      return {0.84f, 0.38f, 0.42f};
    case kCookedMutton:
      return {0.56f, 0.28f, 0.18f};
    case kFurnace:
    case kFurnaceNorth:
    case kFurnaceEast:
    case kFurnaceSouth:
    case kFurnaceWest:
      return {0.58f, 0.60f, 0.66f};
    case kLootCache:
      return {0.92f, 0.76f, 0.30f};
    case kPlanks:
      return {0.76f, 0.57f, 0.34f};
    case kStick:
      return {0.76f, 0.61f, 0.38f};
    case kWoodPickaxe:
      return {0.84f, 0.66f, 0.38f};
    case kStonePickaxe:
      return {0.72f, 0.72f, 0.76f};
    case kIronPickaxe:
      return {0.90f, 0.92f, 0.96f};
    case kStone:
      return {0.60f, 0.60f, 0.60f};
    default:
      return {0.90f, 0.90f, 0.90f};
  }
}

void appendDroppedItemQuad(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           const glm::vec3& v0,
                           const glm::vec3& v1,
                           const glm::vec3& v2,
                           const glm::vec3& v3,
                           const glm::vec3& color,
                           int tile) {
  glm::vec2 uv0 = uvForTile(tile, 0.0f, 0.0f);
  glm::vec2 uv1 = uvForTile(tile, 1.0f, 0.0f);
  glm::vec2 uv2 = uvForTile(tile, 1.0f, 1.0f);
  glm::vec2 uv3 = uvForTile(tile, 0.0f, 1.0f);

  uint32_t start = static_cast<uint32_t>(vertices.size());
  vertices.push_back({v0, color, uv0});
  vertices.push_back({v1, color, uv1});
  vertices.push_back({v2, color, uv2});
  vertices.push_back({v3, color, uv3});

  indices.push_back(start + 0);
  indices.push_back(start + 1);
  indices.push_back(start + 2);
  indices.push_back(start + 0);
  indices.push_back(start + 2);
  indices.push_back(start + 3);
}

void appendDroppedItemCube(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           const glm::vec3& center,
                           float spinY,
                           uint8_t type) {
  const float s = kDroppedItemHalfSize;
  const float h = kDroppedItemHalfHeight;
  const glm::vec3 base = droppedItemColor(type);
  const int tile = tileForBlock(type);
  const float c = std::cos(spinY);
  const float sRot = std::sin(spinY);

  auto rot = [&](const glm::vec3& local) {
    glm::vec3 p = local;
    float x = p.x * c - p.z * sRot;
    float z = p.x * sRot + p.z * c;
    return center + glm::vec3(x, p.y, z);
  };

  glm::vec3 p000 = rot({-s, -h, -s});
  glm::vec3 p001 = rot({-s, -h, s});
  glm::vec3 p010 = rot({-s, h, -s});
  glm::vec3 p011 = rot({-s, h, s});
  glm::vec3 p100 = rot({s, -h, -s});
  glm::vec3 p101 = rot({s, -h, s});
  glm::vec3 p110 = rot({s, h, -s});
  glm::vec3 p111 = rot({s, h, s});

  appendDroppedItemQuad(vertices, indices, p100, p110, p111, p101, base * 0.84f, tile); // +X
  appendDroppedItemQuad(vertices, indices, p001, p011, p010, p000, base * 0.84f, tile); // -X
  appendDroppedItemQuad(vertices, indices, p010, p011, p111, p110, base * 1.04f, tile); // +Y
  appendDroppedItemQuad(vertices, indices, p000, p100, p101, p001, base * 0.62f, tile); // -Y
  appendDroppedItemQuad(vertices, indices, p101, p111, p011, p001, base * 0.92f, tile); // +Z
  appendDroppedItemQuad(vertices, indices, p000, p010, p110, p100, base * 0.76f, tile); // -Z
}

bool shouldRenderHeldItemAsSprite(uint8_t type) {
  return isUnderwaterPlantBlock(type) ||
         type == kWool ||
         type == kRawMutton ||
         type == kCookedMutton;
}

bool shouldRenderDroppedItemAsSprite(uint8_t type) {
  return !isBlockType(type) || type == kTorch || isUnderwaterPlantBlock(type);
}

void appendDroppedItemSprite(std::vector<Vertex>& vertices,
                             std::vector<uint32_t>& indices,
                             const glm::vec3& center,
                             float spinY,
                             uint8_t type) {
  float halfWidth = 0.22f;
  float halfHeight = 0.22f;
  if (type == kTorch || type == kStick) {
    halfWidth = 0.10f;
    halfHeight = 0.30f;
  } else if (type == kIronIngot) {
    halfWidth = 0.26f;
    halfHeight = 0.14f;
  } else if (type == kDiamond) {
    halfWidth = 0.22f;
    halfHeight = 0.22f;
  } else if (type == kSeagrass) {
    halfWidth = 0.18f;
    halfHeight = 0.28f;
  } else if (type == kCoral) {
    halfWidth = 0.20f;
    halfHeight = 0.24f;
  } else if (type == kWool) {
    halfWidth = 0.24f;
    halfHeight = 0.22f;
  } else if (type == kRawMutton || type == kCookedMutton) {
    halfWidth = 0.24f;
    halfHeight = 0.18f;
  } else if (isToolItem(type)) {
    halfWidth = 0.28f;
    halfHeight = 0.28f;
  }

  const glm::vec3 base = isUnderwaterPlantBlock(type) ? glm::vec3(1.0f) : droppedItemColor(type);
  const int tile = tileForBlock(type);
  const float c = std::cos(spinY);
  const float sRot = std::sin(spinY);

  auto rot = [&](const glm::vec3& local) {
    float x = local.x * c - local.z * sRot;
    float z = local.x * sRot + local.z * c;
    return center + glm::vec3(x, local.y, z);
  };

  glm::vec3 v0 = rot({-halfWidth, -halfHeight, 0.0f});
  glm::vec3 v1 = rot({halfWidth, -halfHeight, 0.0f});
  glm::vec3 v2 = rot({halfWidth, halfHeight, 0.0f});
  glm::vec3 v3 = rot({-halfWidth, halfHeight, 0.0f});
  appendDroppedItemQuad(vertices, indices, v0, v1, v2, v3, base, tile);
}

void appendFirstPersonSprite(std::vector<Vertex>& vertices,
                             std::vector<uint32_t>& indices,
                             const glm::mat4& transform,
                             float halfWidth,
                             float halfHeight,
                             const glm::vec3& color,
                             int tile) {
  auto transformLocal = [&](const glm::vec3& point) {
    glm::vec4 result = transform * glm::vec4(point, 1.0f);
    return glm::vec3(result);
  };
  glm::vec3 v0 = transformLocal({-halfWidth, -halfHeight, 0.0f});
  glm::vec3 v1 = transformLocal({halfWidth, -halfHeight, 0.0f});
  glm::vec3 v2 = transformLocal({halfWidth, halfHeight, 0.0f});
  glm::vec3 v3 = transformLocal({-halfWidth, halfHeight, 0.0f});
  appendDroppedItemQuad(vertices, indices, v0, v1, v2, v3, color, tile);
}

glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point) {
  glm::vec4 result = transform * glm::vec4(point, 1.0f);
  return glm::vec3(result);
}

void appendFirstPersonPrism(std::vector<Vertex>& vertices,
                            std::vector<uint32_t>& indices,
                            const glm::mat4& transform,
                            const glm::vec3& center,
                            const glm::vec3& halfExtents,
                            const glm::vec3& baseColor,
                            int tile) {
  glm::vec3 p000 = transformPoint(transform, center + glm::vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z));
  glm::vec3 p001 = transformPoint(transform, center + glm::vec3(-halfExtents.x, -halfExtents.y, halfExtents.z));
  glm::vec3 p010 = transformPoint(transform, center + glm::vec3(-halfExtents.x, halfExtents.y, -halfExtents.z));
  glm::vec3 p011 = transformPoint(transform, center + glm::vec3(-halfExtents.x, halfExtents.y, halfExtents.z));
  glm::vec3 p100 = transformPoint(transform, center + glm::vec3(halfExtents.x, -halfExtents.y, -halfExtents.z));
  glm::vec3 p101 = transformPoint(transform, center + glm::vec3(halfExtents.x, -halfExtents.y, halfExtents.z));
  glm::vec3 p110 = transformPoint(transform, center + glm::vec3(halfExtents.x, halfExtents.y, -halfExtents.z));
  glm::vec3 p111 = transformPoint(transform, center + glm::vec3(halfExtents.x, halfExtents.y, halfExtents.z));

  appendDroppedItemQuad(vertices, indices, p100, p110, p111, p101, baseColor * 0.84f, tile);
  appendDroppedItemQuad(vertices, indices, p001, p011, p010, p000, baseColor * 0.82f, tile);
  appendDroppedItemQuad(vertices, indices, p010, p011, p111, p110, baseColor * 1.03f, tile);
  appendDroppedItemQuad(vertices, indices, p000, p100, p101, p001, baseColor * 0.60f, tile);
  appendDroppedItemQuad(vertices, indices, p101, p111, p011, p001, baseColor * 0.92f, tile);
  appendDroppedItemQuad(vertices, indices, p000, p010, p110, p100, baseColor * 0.72f, tile);
}

glm::vec3 firstPersonToolHandleColor(uint8_t type) {
  switch (type) {
    case kIronPickaxe:
      return glm::vec3(0.64f, 0.46f, 0.24f);
    case kStonePickaxe:
      return glm::vec3(0.62f, 0.44f, 0.22f);
    case kWoodPickaxe:
    default:
      return glm::vec3(0.70f, 0.50f, 0.24f);
  }
}

glm::vec3 firstPersonToolHeadColor(uint8_t type) {
  switch (type) {
    case kIronPickaxe:
      return glm::vec3(0.86f, 0.88f, 0.92f);
    case kStonePickaxe:
      return glm::vec3(0.56f, 0.58f, 0.62f);
    case kWoodPickaxe:
    default:
      return glm::vec3(0.78f, 0.60f, 0.30f);
  }
}

void appendFirstPersonPickaxe(std::vector<Vertex>& vertices,
                              std::vector<uint32_t>& indices,
                              const glm::mat4& transform,
                              uint8_t type) {
  const glm::vec3 handleColor = firstPersonToolHandleColor(type);
  const glm::vec3 headColor = firstPersonToolHeadColor(type);

  appendFirstPersonPrism(vertices,
                         indices,
                         transform,
                         glm::vec3(0.0f, -0.14f, 0.0f),
                         glm::vec3(0.024f, 0.30f, 0.024f),
                         handleColor,
                         kTileUiWhite);
  appendFirstPersonPrism(vertices,
                         indices,
                         transform,
                         glm::vec3(0.0f, 0.18f, 0.0f),
                         glm::vec3(0.030f, 0.048f, 0.028f),
                         headColor * 0.92f,
                         kTileUiWhite);
  appendFirstPersonPrism(vertices,
                         indices,
                         transform,
                         glm::vec3(0.0f, 0.28f, 0.0f),
                         glm::vec3(0.15f, 0.026f, 0.028f),
                         headColor,
                         kTileUiWhite);
  appendFirstPersonPrism(vertices,
                         indices,
                         transform,
                         glm::vec3(-0.11f, 0.21f, 0.0f),
                         glm::vec3(0.020f, 0.060f, 0.020f),
                         headColor * 0.96f,
                         kTileUiWhite);
  appendFirstPersonPrism(vertices,
                         indices,
                         transform,
                         glm::vec3(0.11f, 0.21f, 0.0f),
                         glm::vec3(0.020f, 0.060f, 0.020f),
                         headColor * 0.96f,
                         kTileUiWhite);
}

float wrapDegrees(float degrees) {
  while (degrees > 180.0f) {
    degrees -= 360.0f;
  }
  while (degrees < -180.0f) {
    degrees += 360.0f;
  }
  return degrees;
}

bool rayIntersectsAabb(const glm::vec3& origin,
                       const glm::vec3& dir,
                       const glm::vec3& minCorner,
                       const glm::vec3& maxCorner,
                       float maxDist,
                       float& outDistance) {
  float tMin = 0.0f;
  float tMax = maxDist;

  for (int axis = 0; axis < 3; ++axis) {
    float axisDir = dir[axis];
    if (std::abs(axisDir) < 0.0001f) {
      if (origin[axis] < minCorner[axis] || origin[axis] > maxCorner[axis]) {
        return false;
      }
      continue;
    }

    float invDir = 1.0f / axisDir;
    float t0 = (minCorner[axis] - origin[axis]) * invDir;
    float t1 = (maxCorner[axis] - origin[axis]) * invDir;
    if (t0 > t1) {
      std::swap(t0, t1);
    }
    tMin = std::max(tMin, t0);
    tMax = std::min(tMax, t1);
    if (tMax < tMin) {
      return false;
    }
  }

  outDistance = tMin;
  return outDistance <= maxDist;
}

void appendSheepEntityMesh(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           const glm::vec3& pos,
                           float yaw,
                           float walkSpeed,
                           float animPhase,
                           float grazeTimer) {
  glm::mat4 bodyTransform(1.0f);
  bodyTransform = glm::translate(bodyTransform, pos);
  bodyTransform = glm::rotate(bodyTransform, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

  float walkBlend = std::clamp(walkSpeed / kSheepMaxSpeed, 0.0f, 1.0f);
  float legSwing = std::sin(animPhase) * 0.14f * walkBlend;
  float bodyBob = std::abs(std::sin(animPhase)) * 0.015f * walkBlend;
  float headNod = walkSpeed > 0.05f
    ? std::sin(animPhase * 0.5f) * 0.02f
    : -std::clamp(0.20f - grazeTimer, 0.0f, 0.20f) * 0.22f;

  const glm::vec3 woolColor(0.94f, 0.94f, 0.97f);
  const glm::vec3 faceColor(0.24f, 0.22f, 0.24f);
  const glm::vec3 muzzleColor(0.88f, 0.72f, 0.76f);
  const glm::vec3 hoofColor(0.16f, 0.14f, 0.14f);

  appendFirstPersonPrism(vertices,
                         indices,
                         bodyTransform,
                         glm::vec3(0.0f, 0.86f + bodyBob, 0.00f),
                         glm::vec3(0.38f, 0.28f, 0.54f),
                         woolColor,
                         kTileUiWhite);

  appendFirstPersonPrism(vertices,
                         indices,
                         bodyTransform,
                         glm::vec3(0.0f, 0.84f + headNod, 0.72f),
                         glm::vec3(0.18f, 0.18f, 0.18f),
                         faceColor,
                         kTileUiWhite);
  appendFirstPersonPrism(vertices,
                         indices,
                         bodyTransform,
                         glm::vec3(0.0f, 0.78f + headNod, 0.92f),
                         glm::vec3(0.12f, 0.10f, 0.08f),
                         muzzleColor,
                         kTileUiWhite);

  const std::array<glm::vec2, 4> legOffsets{{
    {-0.22f, 0.28f},
    {0.22f, 0.28f},
    {-0.22f, -0.26f},
    {0.22f, -0.26f}
  }};
  for (size_t i = 0; i < legOffsets.size(); ++i) {
    float phase = (i % 2 == 0) ? legSwing : -legSwing;
    appendFirstPersonPrism(vertices,
                           indices,
                           bodyTransform,
                           glm::vec3(legOffsets[i].x, 0.34f + std::max(0.0f, phase), legOffsets[i].y),
                           glm::vec3(0.07f, 0.34f, 0.07f),
                           hoofColor,
                           kTileUiWhite);
  }
}

uint32_t mixLootHash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7FEB352Du;
  value ^= value >> 15;
  value *= 0x846CA68Bu;
  value ^= value >> 16;
  return value;
}

uint32_t lootCacheStateForBlock(const glm::ivec3& block, int seed) {
  uint32_t state = mixLootHash(static_cast<uint32_t>(seed) ^ 0x9E3779B9u);
  state ^= mixLootHash(static_cast<uint32_t>(block.x) * 0x85EBCA6Bu);
  state ^= mixLootHash(static_cast<uint32_t>(block.y) * 0xC2B2AE35u);
  state ^= mixLootHash(static_cast<uint32_t>(block.z) * 0x27D4EB2Fu);
  return state == 0u ? 0xA341316Cu : state;
}

bool shouldDropStickFromLeaves(const glm::ivec3& block, int seed) {
  uint32_t state = lootCacheStateForBlock(block, seed) ^ 0x51C1F00Du;
  state = mixLootHash(state);
  return (state % 10u) == 0u;
}

void appendOverlayBox(std::vector<Vertex>& vertices,
                      std::vector<uint32_t>& indices,
                      const glm::vec3& minCorner,
                      const glm::vec3& maxCorner,
                      const glm::vec3& color) {
  glm::vec3 p000{minCorner.x, minCorner.y, minCorner.z};
  glm::vec3 p001{minCorner.x, minCorner.y, maxCorner.z};
  glm::vec3 p010{minCorner.x, maxCorner.y, minCorner.z};
  glm::vec3 p011{minCorner.x, maxCorner.y, maxCorner.z};
  glm::vec3 p100{maxCorner.x, minCorner.y, minCorner.z};
  glm::vec3 p101{maxCorner.x, minCorner.y, maxCorner.z};
  glm::vec3 p110{maxCorner.x, maxCorner.y, minCorner.z};
  glm::vec3 p111{maxCorner.x, maxCorner.y, maxCorner.z};

  appendDroppedItemQuad(vertices, indices, p100, p110, p111, p101, color * 0.82f, kTileUiWhite);
  appendDroppedItemQuad(vertices, indices, p001, p011, p010, p000, color * 0.82f, kTileUiWhite);
  appendDroppedItemQuad(vertices, indices, p010, p011, p111, p110, color * 1.04f, kTileUiWhite);
  appendDroppedItemQuad(vertices, indices, p000, p100, p101, p001, color * 0.62f, kTileUiWhite);
  appendDroppedItemQuad(vertices, indices, p101, p111, p011, p001, color * 0.94f, kTileUiWhite);
  appendDroppedItemQuad(vertices, indices, p000, p010, p110, p100, color * 0.74f, kTileUiWhite);
}

void appendOverlayEdge(std::vector<Vertex>& vertices,
                       std::vector<uint32_t>& indices,
                       const glm::vec3& a,
                       const glm::vec3& b,
                       float halfThickness,
                       const glm::vec3& color) {
  glm::vec3 minCorner = glm::min(a, b);
  glm::vec3 maxCorner = glm::max(a, b);
  if (std::abs(a.x - b.x) < 0.001f) {
    minCorner.x = a.x - halfThickness;
    maxCorner.x = a.x + halfThickness;
  } else {
    minCorner.x -= halfThickness;
    maxCorner.x += halfThickness;
  }
  if (std::abs(a.y - b.y) < 0.001f) {
    minCorner.y = a.y - halfThickness;
    maxCorner.y = a.y + halfThickness;
  } else {
    minCorner.y -= halfThickness;
    maxCorner.y += halfThickness;
  }
  if (std::abs(a.z - b.z) < 0.001f) {
    minCorner.z = a.z - halfThickness;
    maxCorner.z = a.z + halfThickness;
  } else {
    minCorner.z -= halfThickness;
    maxCorner.z += halfThickness;
  }
  appendOverlayBox(vertices, indices, minCorner, maxCorner, color);
}

void appendOverlayWireCube(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           const glm::vec3& minCorner,
                           const glm::vec3& maxCorner,
                           float halfThickness,
                           const glm::vec3& color) {
  glm::vec3 p000{minCorner.x, minCorner.y, minCorner.z};
  glm::vec3 p001{minCorner.x, minCorner.y, maxCorner.z};
  glm::vec3 p010{minCorner.x, maxCorner.y, minCorner.z};
  glm::vec3 p011{minCorner.x, maxCorner.y, maxCorner.z};
  glm::vec3 p100{maxCorner.x, minCorner.y, minCorner.z};
  glm::vec3 p101{maxCorner.x, minCorner.y, maxCorner.z};
  glm::vec3 p110{maxCorner.x, maxCorner.y, minCorner.z};
  glm::vec3 p111{maxCorner.x, maxCorner.y, maxCorner.z};

  appendOverlayEdge(vertices, indices, p000, p100, halfThickness, color);
  appendOverlayEdge(vertices, indices, p001, p101, halfThickness, color);
  appendOverlayEdge(vertices, indices, p010, p110, halfThickness, color);
  appendOverlayEdge(vertices, indices, p011, p111, halfThickness, color);
  appendOverlayEdge(vertices, indices, p000, p001, halfThickness, color);
  appendOverlayEdge(vertices, indices, p010, p011, halfThickness, color);
  appendOverlayEdge(vertices, indices, p100, p101, halfThickness, color);
  appendOverlayEdge(vertices, indices, p110, p111, halfThickness, color);
  appendOverlayEdge(vertices, indices, p000, p010, halfThickness, color);
  appendOverlayEdge(vertices, indices, p001, p011, halfThickness, color);
  appendOverlayEdge(vertices, indices, p100, p110, halfThickness, color);
  appendOverlayEdge(vertices, indices, p101, p111, halfThickness, color);
}

std::string trimAscii(const std::string& value) {
  size_t first = 0;
  while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
    --last;
  }
  return value.substr(first, last - first);
}

std::vector<VulkanContext::WorldChunkMeshUpload> toVkChunkUploads(std::vector<ChunkMeshUpload>&& uploads) {
  std::vector<VulkanContext::WorldChunkMeshUpload> out;
  out.reserve(uploads.size());
  for (ChunkMeshUpload& upload : uploads) {
    VulkanContext::WorldChunkMeshUpload vkUpload;
    vkUpload.key = upload.key;
    vkUpload.vertices = std::move(upload.vertices);
    vkUpload.indices = std::move(upload.indices);
    out.push_back(std::move(vkUpload));
  }
  return out;
}

std::string sanitizeWorldNameForFile(const std::string& value) {
  std::string source = trimAscii(value);
  if (source.empty()) {
    source = "World";
  }

  std::string out;
  out.reserve(source.size());
  for (char c : source) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) != 0 || c == '-' || c == '_' || c == ' ') {
      out.push_back(c == ' ' ? '_' : c);
    }
  }

  if (out.empty()) {
    out = "World";
  }

  if (out.size() > 48) {
    out.resize(48);
  }
  return out;
}

int defaultRenderDistanceForQuality(int quality) {
  static constexpr int kQualityToRadius[3] = {6, 8, 10};
  int clampedQuality = std::clamp(quality, 0, 2);
  int radius = kQualityToRadius[clampedQuality];
  return std::clamp(radius, kMinRenderDistance, kMaxRenderDistance);
}

std::string worldMetaPathForWorld(const std::filesystem::path& worldPath) {
  std::filesystem::path metaPath = worldPath;
  metaPath.replace_extension(".meta");
  return metaPath.string();
}

bool saveWorldDisplayNameMeta(const std::string& worldPath, const std::string& displayName) {
  if (worldPath.empty()) {
    return false;
  }

  std::string trimmed = trimAscii(displayName);
  if (trimmed.empty()) {
    return false;
  }

  std::ofstream out(worldMetaPathForWorld(std::filesystem::path(worldPath)), std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << trimmed << "\n";
  return true;
}

std::string worldDisplayNameFromPath(const std::filesystem::path& worldPath) {
  std::string name = worldPath.stem().string();
  if (name.empty()) {
    name = "World";
  }
  for (char& c : name) {
    if (c == '_') {
      c = ' ';
    }
  }
  return name;
}

std::string playerStatePathForWorld(const std::string& worldPath) {
  if (worldPath.empty()) {
    return {};
  }
  std::filesystem::path p(worldPath);
  p.replace_extension(".player");
  return p.string();
}

std::string uniqueWorldPathForName(const std::string& worldName, std::string* outDisplayName) {
  std::filesystem::path savesDir = cubeosSavesDir();
  std::error_code ec;
  std::filesystem::create_directories(savesDir, ec);

  std::string base = sanitizeWorldNameForFile(worldName);
  std::string candidate = base;
  int suffixIndex = 1;
  int suffix = 2;
  while (std::filesystem::exists(savesDir / (candidate + ".bin"), ec)) {
    candidate = base + "_" + std::to_string(suffix);
    suffixIndex = suffix;
    ++suffix;
  }

  if (outDisplayName) {
    std::string displayName = trimAscii(worldName);
    if (displayName.empty()) {
      displayName = "World";
    }
    if (suffixIndex > 1) {
      displayName += " " + std::to_string(suffixIndex);
    }
    *outDisplayName = displayName;
  }
  return (savesDir / (candidate + ".bin")).string();
}

std::string loadWorldDisplayNameMeta(const std::filesystem::path& worldPath) {
  std::ifstream in(worldMetaPathForWorld(worldPath));
  if (in.is_open()) {
    std::string line;
    if (std::getline(in, line)) {
      line = trimAscii(line);
      if (!line.empty()) {
        return line;
      }
    }
  }
  return worldDisplayNameFromPath(worldPath);
}

bool isAllowedWorldNameCodepoint(unsigned int codepoint) {
  if (codepoint < 32 || codepoint == 127 || codepoint > 0x10FFFFu) {
    return false;
  }
  if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) {
    return false;
  }
  if (codepoint < 128u) {
    char c = static_cast<char>(codepoint);
    switch (c) {
      case '/':
      case '\\':
      case ':':
      case '*':
      case '?':
      case '"':
      case '<':
      case '>':
      case '|':
        return false;
      default:
        break;
    }
  }
  return true;
}

void appendUtf8(std::string& out, unsigned int codepoint) {
  if (codepoint <= 0x7Fu) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FFu) {
    out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6) & 0x1Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else if (codepoint <= 0xFFFFu) {
    out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12) & 0x0Fu)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18) & 0x07u)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  }
}

size_t utf8CodepointCount(const std::string& text) {
  size_t count = 0;
  for (unsigned char c : text) {
    if ((c & 0xC0u) != 0x80u) {
      ++count;
    }
  }
  return count;
}

void popUtf8Back(std::string& text) {
  if (text.empty()) {
    return;
  }
  size_t idx = text.size() - 1;
  while (idx > 0) {
    unsigned char c = static_cast<unsigned char>(text[idx]);
    if ((c & 0xC0u) != 0x80u) {
      break;
    }
    --idx;
  }
  text.erase(idx);
}

struct DiskWorldEntry {
  std::string displayName;
  std::string path;
};

std::vector<DiskWorldEntry> collectWorldSelectEntries() {
  std::vector<DiskWorldEntry> result;
  std::filesystem::path savesDir = cubeosSavesDir();
  std::error_code ec;
  if (!std::filesystem::exists(savesDir, ec)) {
    return result;
  }

  struct WorldFileMeta {
    DiskWorldEntry entry;
    std::filesystem::file_time_type modified{};
  };
  std::vector<WorldFileMeta> files;

  for (const auto& dirEntry : std::filesystem::directory_iterator(savesDir, ec)) {
    if (ec) {
      break;
    }
    if (!dirEntry.is_regular_file()) {
      continue;
    }
    const std::filesystem::path p = dirEntry.path();
    if (p.extension() != ".bin") {
      continue;
    }

    WorldFileMeta meta;
    meta.entry.path = p.string();
    meta.entry.displayName = loadWorldDisplayNameMeta(p);
    meta.modified = std::filesystem::last_write_time(p, ec);
    if (ec) {
      ec.clear();
      meta.modified = std::filesystem::file_time_type::min();
    }
    files.push_back(std::move(meta));
  }

  std::stable_sort(files.begin(),
                   files.end(),
                   [](const WorldFileMeta& a, const WorldFileMeta& b) {
                     if (a.modified != b.modified) {
                       return a.modified > b.modified;
                     }
                     return a.entry.displayName < b.entry.displayName;
                   });

  result.reserve(files.size());
  for (const WorldFileMeta& meta : files) {
    result.push_back(meta.entry);
  }
  return result;
}

bool saveWorldWithWarning(World& world, const std::string& worldPath, const char* reason) {
  if (worldPath.empty()) {
    return false;
  }
  if (world.save(worldPath)) {
    return true;
  }
  std::cerr << "Warning: failed to save world (" << reason << "): " << worldPath << "\n";
  return false;
}

} // namespace

void App::setScreenState(ScreenState state) {
  if (screenState == state) {
    return;
  }

  // Persist current world whenever leaving gameplay.
  bool leavingGameplay =
    (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused) &&
    (state != ScreenState::kPlaying &&
     state != ScreenState::kPaused &&
     state != ScreenState::kDeath);
  bool pauseToInGameSettings =
    screenState == ScreenState::kPaused &&
    state == ScreenState::kSettings &&
    settingsReturnState == ScreenState::kPaused;
  if (leavingGameplay && inventoryOpen) {
    setInventoryOpen(false);
  }
  if (leavingGameplay &&
      !pauseToInGameSettings &&
      !currentWorldPath.empty()) {
    saveCurrentPlayerState();
    saveWorldWithWarning(world, currentWorldPath, "leaving gameplay");
  }

  if (leavingGameplay && !pauseToInGameSettings) {
    droppedItems.clear();
    syncDroppedItemMesh(true);
    sheepEntities.clear();
    syncSheepMesh(true);
  }

  if (state != ScreenState::kPlaying && state != ScreenState::kPaused && inventoryOpen) {
    setInventoryOpen(false);
  }
  if (state != ScreenState::kPlaying && achievementTreeOpen) {
    setAchievementTreeOpen(false);
  }
  if (state != ScreenState::kPlaying) {
    commandInputOpen = false;
    commandInput.clear();
    commandBackspaceDown = false;
    commandEnterDown = false;
    if (state == ScreenState::kDeath) {
      flying = false;
    }
  }

  if (state != ScreenState::kPlaying && breakingActive) {
    breakingActive = false;
    breakingProgress = 0.0f;
    breakingStage = 0;
    world.clearBreakOverlay();
  }

  screenState = state;
  if (screenState == ScreenState::kPlaying ||
      screenState == ScreenState::kPaused ||
      screenState == ScreenState::kLoadingWorld) {
    menuIntro = 1.0f;
  } else {
    menuIntro = 0.0f;
  }
  refreshCursorMode();
  firstMouse = true;
  menuUpDown = false;
  menuDownDown = false;
  menuLeftDown = false;
  menuRightDown = false;
  menuEnterDown = false;
  menuBackspaceDown = false;
  menuEscDown = false;
  uiDirty = true;
  updateWindowTitle();

  if (vkReady) {
    rebuildUiMesh();
    composeMeshData();
    vk.updateMesh(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
    uiDirty = false;
  }
}

void App::updateWindowTitle() {
  if (!window) {
    return;
  }

  const bool ru = appliedSettings.language == 1;
  auto loc = [&](const char* en, const char* ruText) {
    return ru ? std::string(ruText) : std::string(en);
  };

  auto mark = [](int current, int index, const std::string& label) {
    return current == index
      ? std::string("[") + label + "]"
      : std::string(label);
  };

  std::string title = "CubeOS v0.3.0 Snapshot 2";
  switch (screenState) {
    case ScreenState::kMainMenu:
      title += " | " + loc("Menu: ", "Меню: ") + mark(mainMenuSelection, 0, loc("Start", "Начать")) + "  "
            + mark(mainMenuSelection, 1, loc("Settings", "Настройки")) + "  "
            + mark(mainMenuSelection, 2, loc("Quit", "Выход"));
      break;
    case ScreenState::kWorldSelect: {
      int rowCount = static_cast<int>(worldSelectEntries.size()) + 2;
      int clampedSelection = std::clamp(worldSelectSelection, 0, std::max(0, rowCount - 1));
      if (clampedSelection == 0) {
        title += " | " + loc("World Select", "Выбор мира") + " | [" + loc("Create New World", "Создать мир") + "]";
      } else if (clampedSelection == rowCount - 1) {
        title += " | " + loc("World Select", "Выбор мира") + " | [" + loc("Back To Menu", "Назад в меню") + "]";
      } else {
        int worldIndex = clampedSelection - 1;
        std::string worldLabel = worldSelectEntries[static_cast<size_t>(worldIndex)].displayName;
        title += " | " + loc("World Select", "Выбор мира") + " | [" + worldLabel + "]";
      }
      break;
    }
    case ScreenState::kSettings:
      title += ru
        ? " | Настройки | Категории мышкой | Ползунки тянуть | Esc назад"
        : " | Settings | Mouse Categories | Drag Sliders | Esc Back";
      break;
    case ScreenState::kPaused:
      title += " | " + loc("Paused: ", "Пауза: ") + mark(pauseMenuSelection, 0, loc("Continue", "Продолжить")) + "  "
            + mark(pauseMenuSelection, 1, loc("Settings", "Настройки")) + "  "
            + mark(pauseMenuSelection, 2, loc("Main Menu", "Меню"));
      break;
    case ScreenState::kDeath:
      title += " | " + loc("You Died", "Вы погибли") + " | "
            + mark(deathMenuSelection, 0, loc("Respawn", "Респавн")) + "  "
            + mark(deathMenuSelection, 1, loc("Main Menu", "Меню"));
      break;
    case ScreenState::kLoadingWorld:
      title += " | " + loc("Loading World", "Загрузка мира");
      break;
    case ScreenState::kCreateWorld: {
      std::string presetName = worldPresetDisplayName(pendingWorldSettings.preset, ru);
      std::string modeText = pendingWorldSettings.startInventoryMode == 0
        ? loc("Survival", "Выживание")
        : loc("Creative", "Креатив");
      std::string cheatsText = pendingWorldSettings.cheatsEnabled
        ? loc("On", "Вкл")
        : loc("Off", "Выкл");
      std::string seedText = pendingSeedText.empty() ? loc("random", "случайно") : pendingSeedText;
      std::string nameText = pendingWorldName.empty() ? loc("World", "Мир") : pendingWorldName;
      title += " | " + loc("Create World", "Создание мира") +
               " | " + loc("Name", "Имя") + ": " + nameText +
               " | " + loc("Seed", "Сид") + ": " + seedText +
               " | " + loc("Preset", "Пресет") + ": " + presetName +
               " | " + loc("Cave", "Пещеры") + ": " + std::to_string(pendingWorldSettings.caveDensity).substr(0, 4) +
               " | " + loc("Ravine", "Овраги") + ": " + std::to_string(pendingWorldSettings.ravineFrequency).substr(0, 4) +
               " | " + loc("Mode", "Режим") + ": " + modeText +
               " | " + loc("Cheats", "Читы") + ": " + cheatsText;
      break;
    }
    case ScreenState::kPlaying:
      title += " | " + loc("In Game", "В игре") +
               " | " + (isCreativeMode() ? loc("Creative", "Креатив") : loc("Survival", "Выживание"));
      break;
  }

  glfwSetWindowTitle(window, title.c_str());
}

void App::refreshWorldSelectEntries() {
  std::vector<DiskWorldEntry> diskEntries = collectWorldSelectEntries();
  worldSelectEntries.clear();
  worldSelectEntries.reserve(diskEntries.size());
  for (const DiskWorldEntry& entry : diskEntries) {
    worldSelectEntries.push_back({entry.displayName, entry.path});
  }
  int rowCount = static_cast<int>(worldSelectEntries.size()) + 2;
  worldSelectSelection = std::clamp(worldSelectSelection, 0, std::max(0, rowCount - 1));
  worldSelectScroll = std::max(0, std::min(worldSelectScroll, std::max(0, rowCount - 1)));
}

bool App::isCreativeMode() const {
  return isCreativeWorldMode(world.getGenerationSettings());
}

bool App::isCreativeInventoryScreen() const {
  return screenState == ScreenState::kPlaying &&
         inventoryOpen &&
         isCreativeMode() &&
         !workbenchOpen &&
         !furnaceOpen;
}

bool App::worldCheatsEnabled() const {
  return world.getGenerationSettings().cheatsEnabled;
}

bool App::canFly() const {
  return isCreativeMode() || (worldCheatsEnabled() && cheatFlightEnabled);
}

void App::damagePlayer(int amount) {
  if (amount <= 0 || isCreativeMode() || screenState == ScreenState::kDeath) {
    return;
  }

  int previousHealth = playerHealth;
  playerHealth = std::clamp(playerHealth - amount, 0, kMaxPlayerHealth);
  if (playerHealth == previousHealth) {
    return;
  }

  float hurtGain = std::clamp(0.84f + static_cast<float>(amount) * 0.06f, 0.84f, 1.28f);
  audio.playCue(AudioSystem::Cue::kPlayerHurt, hurtGain);
  uiDirty = true;
  if (playerHealth <= 0) {
    handlePlayerDeath();
  }
}

void App::dropPlayerInventoryOnDeath() {
  glm::vec3 basePos = playerPos + glm::vec3(0.0f, 0.9f, 0.0f);
  size_t dropIndex = 0;

  auto dropStack = [&](ItemStack& stack) {
    if (stack.type == kAir || stack.count == 0) {
      return;
    }

    float seedNoise = std::sin(basePos.x * 12.9898f +
                               basePos.y * 78.233f +
                               basePos.z * 37.719f +
                               static_cast<float>(stack.type) * 0.713f +
                               static_cast<float>(stack.count) * 0.193f +
                               static_cast<float>(dropIndex) * 17.371f);
    float seed = fract01(seedNoise * 43758.5453f);
    float angle = seed * 6.2831853f + static_cast<float>(dropIndex) * 0.41f;
    float radialOffset = 0.05f + seed * 0.16f;
    float lateralSpeed = 1.10f + seed * 0.75f;
    glm::vec3 itemPos = basePos + glm::vec3(std::cos(angle) * radialOffset,
                                            0.04f + seed * 0.12f,
                                            std::sin(angle) * radialOffset);
    glm::vec3 itemVelocity(std::cos(angle) * lateralSpeed,
                           2.55f + seed * 0.85f,
                           std::sin(angle) * lateralSpeed);
    spawnDroppedItemWithPhysics(stack.type, itemPos, itemVelocity, 0.90f, stack.count);
    stack.type = kAir;
    stack.count = 0;
    ++dropIndex;
  };

  auto dropSlots = [&](auto& slots) {
    for (ItemStack& stack : slots) {
      dropStack(stack);
    }
  };

  dropSlots(hotbar);
  dropSlots(inventory);
  dropSlots(craftingSlots);
  dropStack(cursorStack);

  refreshSelectedBlock();
  uiDirty = true;
}

void App::handlePlayerDeath() {
  if (screenState == ScreenState::kDeath) {
    return;
  }

  dropPlayerInventoryOnDeath();
  inventoryOpen = false;
  workbenchOpen = false;
  furnaceOpen = false;
  craftResultFlashTimer = 0.0f;
  cursorStack.type = kAir;
  cursorStack.count = 0;
  footstepDistanceAccumulator = 0.0f;
  playerHealth = 0;
  playerVel = glm::vec3(0.0f);
  onGround = false;
  flying = false;
  fallDistance = 0.0f;
  flightToggleTimer = 0.0f;
  jumpDown = false;
  deathMenuSelection = 0;
  commandInputOpen = false;
  commandInput.clear();
  commandBackspaceDown = false;
  commandEnterDown = false;
  setScreenState(ScreenState::kDeath);
  if (!currentWorldPath.empty()) {
    saveCurrentPlayerState();
    saveWorldWithWarning(world, currentWorldPath, "player death");
  }
}

void App::respawnPlayer() {
  glm::vec3 target = respawnPos;
  target.y = std::clamp(target.y, 2.0f, static_cast<float>(world.height() - 3));
  if (!world.inBounds(static_cast<int>(std::floor(target.x)),
                      static_cast<int>(std::floor(target.y)),
                      static_cast<int>(std::floor(target.z)))) {
    target = glm::vec3(8.0f, static_cast<float>(world.height() - 6), 8.0f);
  }

  playerPos = target;
  while (collidesAt(playerPos) && playerPos.y < static_cast<float>(world.height() - 3)) {
    playerPos.y += 0.35f;
  }

  playerVel = glm::vec3(0.0f);
  onGround = false;
  playerHealth = kMaxPlayerHealth;
  flying = false;
  fallDistance = 0.0f;
  footstepDistanceAccumulator = 0.0f;
  flightToggleTimer = 0.0f;
  jumpDown = false;
  commandInputOpen = false;
  commandInput.clear();
  commandBackspaceDown = false;
  commandEnterDown = false;
  setScreenState(ScreenState::kPlaying);
  showToast(appliedSettings.language == 1 ? "РЕСПАВН" : "RESPAWNED", 1.6f);
}

void App::executeCommand(const std::string& rawInput) {
  std::string input = trimAscii(rawInput);
  if (input.empty()) {
    return;
  }
  if (!input.empty() && input.front() == '/') {
    input.erase(input.begin());
  }
  input = trimAscii(input);
  if (input.empty()) {
    return;
  }

  std::istringstream parser(input);
  std::string command;
  parser >> command;
  command = normalizeCommandToken(command);

  const bool ru = appliedSettings.language == 1;
  if (command == "help") {
    showToast(ru ? "КОМАНДЫ /ADD /FLY /HEAL /RESPAWN" : "COMMANDS /ADD /FLY /HEAL /RESPAWN", 3.0f);
    return;
  }

  if (command == "fly") {
    if (isCreativeMode()) {
      flying = !flying;
    } else {
      cheatFlightEnabled = !cheatFlightEnabled;
      flying = cheatFlightEnabled;
    }
    flightToggleTimer = 0.0f;
    jumpDown = false;
    uiDirty = true;
    return;
  }

  if (command == "heal") {
    playerHealth = kMaxPlayerHealth;
    uiDirty = true;
    showToast(ru ? "ЗДОРОВЬЕ ПОЛНОЕ" : "HEALTH RESTORED", 1.8f);
    return;
  }

  if (command == "respawn") {
    respawnPlayer();
    return;
  }

  if (command == "add") {
    std::string itemToken;
    parser >> itemToken;
    if (itemToken.empty()) {
      showToast(ru ? "ИСПОЛЬЗУЙ /ADD STONE 64" : "USE /ADD STONE 64", 2.2f);
      return;
    }

    uint8_t itemType = itemTypeForCommandToken(itemToken);
    if (itemType == kAir) {
      showToast(ru ? "НЕИЗВЕСТНЫЙ ПРЕДМЕТ" : "UNKNOWN ITEM", 2.0f);
      return;
    }

    int count = isToolItem(itemType) ? 1 : 64;
    std::string countToken;
    parser >> countToken;
    if (!countToken.empty()) {
      try {
        count = std::stoi(countToken);
      } catch (...) {
        showToast(ru ? "НЕКОРРЕКТНОЕ КОЛИЧЕСТВО" : "INVALID COUNT", 2.0f);
        return;
      }
    }

    count = std::clamp(count, 1, 999);
    int remainingTotal = count;
    while (remainingTotal > 0) {
      uint16_t batch = static_cast<uint16_t>(std::min(remainingTotal, static_cast<int>(kMaxStack)));
      uint16_t remaining = 0;
      addToInventory(itemType, batch, &remaining);
      remainingTotal -= static_cast<int>(batch - remaining);
      if (remaining > 0) {
        break;
      }
    }

    int added = count - remainingTotal;
    if (added <= 0) {
      showToast(ru ? "ИНВЕНТАРЬ ПОЛОН" : "INVENTORY FULL", 2.0f);
      return;
    }

    showToast((ru ? "ДОБАВЛЕНО " : "ADDED ") +
                std::to_string(added) + " " +
                displayNameForBlock(itemType, ru),
              2.2f);
    return;
  }

  showToast(ru ? "НЕИЗВЕСТНАЯ КОМАНДА" : "UNKNOWN COMMAND", 2.0f);
}

void App::saveCurrentPlayerState() const {
  if (currentWorldPath.empty()) {
    return;
  }
  std::string statePath = playerStatePathForWorld(currentWorldPath);
  if (statePath.empty()) {
    return;
  }

  std::filesystem::path stateFile(statePath);
  std::error_code ec;
  if (stateFile.has_parent_path()) {
    std::filesystem::create_directories(stateFile.parent_path(), ec);
    if (ec) {
      std::cerr << "Warning: failed to prepare player-state directory: " << stateFile.parent_path()
                << " (" << ec.message() << ")\n";
      return;
    }
  }

  std::ofstream out(stateFile, std::ios::trunc);
  if (!out.is_open()) {
    std::cerr << "Warning: failed to open player-state file for write: " << statePath << "\n";
    return;
  }

  auto writeStacks = [&](const auto& slots) {
    for (const ItemStack& slot : slots) {
      out << static_cast<int>(slot.type) << " " << slot.count << " ";
    }
    out << "\n";
  };

  out << "CUBEOS_PLAYER_V6\n";
  out << playerPos.x << " "
      << playerPos.y << " "
      << playerPos.z << " "
      << respawnPos.x << " "
      << respawnPos.y << " "
      << respawnPos.z << " "
      << yaw << " "
      << pitch << " "
      << selectedSlot << " "
      << achievementMask << " "
      << playerHealth << " "
      << (cheatFlightEnabled ? 1 : 0) << " "
      << (flying ? 1 : 0) << "\n";
  writeStacks(hotbar);
  writeStacks(inventory);
  std::vector<SavedFurnace> savedFurnaces;
  savedFurnaces.reserve(furnaceStates.size());
  for (const auto& [key, furnace] : furnaceStates) {
    bool hasContent = (furnace.input.count > 0 && furnace.input.type != kAir) ||
                      (furnace.fuel.count > 0 && furnace.fuel.type != kAir) ||
                      (furnace.output.count > 0 && furnace.output.type != kAir) ||
                      furnace.burnTime > 0.0f ||
                      furnace.smeltProgress > 0.0f;
    if (!hasContent) {
      continue;
    }

    SavedFurnace saved{};
    char sep0 = 0;
    char sep1 = 0;
    std::istringstream parser(key);
    if (!(parser >> saved.pos.x >> sep0 >> saved.pos.y >> sep1 >> saved.pos.z) ||
        sep0 != ':' || sep1 != ':') {
      continue;
    }
    saved.input = furnace.input;
    saved.fuel = furnace.fuel;
    saved.output = furnace.output;
    saved.burnTime = furnace.burnTime;
    saved.burnDuration = furnace.burnDuration;
    saved.smeltProgress = furnace.smeltProgress;
    savedFurnaces.push_back(saved);
  }
  out << savedFurnaces.size() << "\n";
  for (const SavedFurnace& furnace : savedFurnaces) {
    out << furnace.pos.x << " "
        << furnace.pos.y << " "
        << furnace.pos.z << " "
        << static_cast<int>(furnace.input.type) << " " << furnace.input.count << " "
        << static_cast<int>(furnace.fuel.type) << " " << furnace.fuel.count << " "
        << static_cast<int>(furnace.output.type) << " " << furnace.output.count << " "
        << furnace.burnTime << " "
        << furnace.burnDuration << " "
        << furnace.smeltProgress << "\n";
  }

  std::vector<SavedDroppedItem> savedDroppedItems;
  savedDroppedItems.reserve(droppedItems.size());
  for (const DroppedItemEntity& item : droppedItems) {
    if (item.type == kAir || item.count == 0) {
      continue;
    }

    SavedDroppedItem saved{};
    saved.type = item.type;
    saved.count = item.count;
    saved.pos = item.pos;
    saved.vel = item.vel;
    saved.age = item.age;
    saved.pickupDelay = item.pickupDelay;
    saved.spinPhase = item.spinPhase;
    saved.onGround = item.onGround;
    savedDroppedItems.push_back(saved);
  }
  out << savedDroppedItems.size() << "\n";
  for (const SavedDroppedItem& item : savedDroppedItems) {
    out << static_cast<int>(item.type) << " "
        << item.count << " "
        << item.pos.x << " "
        << item.pos.y << " "
        << item.pos.z << " "
        << item.vel.x << " "
        << item.vel.y << " "
        << item.vel.z << " "
        << item.age << " "
        << item.pickupDelay << " "
        << item.spinPhase << " "
        << (item.onGround ? 1 : 0) << "\n";
  }
  if (!out.good()) {
    std::cerr << "Warning: failed to flush player-state data: " << statePath << "\n";
  }
}

bool App::loadPlayerStateForWorld(const std::string& worldPath, PlayerSaveData& outState) const {
  std::string statePath = playerStatePathForWorld(worldPath);
  if (statePath.empty()) {
    return false;
  }

  std::ifstream in(statePath);
  if (!in.is_open()) {
    return false;
  }

  PlayerSaveData loaded{};
  std::string firstToken;
  if (!(in >> firstToken)) {
    return false;
  }

  auto readStacks = [&](auto& slots) -> bool {
    for (ItemStack& slot : slots) {
      int rawType = 0;
      uint16_t rawCount = 0;
      if (!(in >> rawType >> rawCount)) {
        return false;
      }
      slot.type = static_cast<uint8_t>(std::clamp(rawType, 0, 255));
      slot.count = rawCount;
      if (slot.count == 0) {
        slot.type = kAir;
      }
    }
    return true;
  };

  auto loadFurnaceSection = [&](size_t furnaceCount) -> bool {
    loaded.furnaces.clear();
    loaded.furnaces.reserve(furnaceCount);
    for (size_t i = 0; i < furnaceCount; ++i) {
      SavedFurnace furnace{};
      int inputType = 0;
      int fuelType = 0;
      int outputType = 0;
      if (!(in >> furnace.pos.x
              >> furnace.pos.y
              >> furnace.pos.z
              >> inputType >> furnace.input.count
              >> fuelType >> furnace.fuel.count
              >> outputType >> furnace.output.count
              >> furnace.burnTime
              >> furnace.burnDuration
              >> furnace.smeltProgress)) {
        return false;
      }
      furnace.input.type = static_cast<uint8_t>(std::clamp(inputType, 0, 255));
      furnace.fuel.type = static_cast<uint8_t>(std::clamp(fuelType, 0, 255));
      furnace.output.type = static_cast<uint8_t>(std::clamp(outputType, 0, 255));
      if (furnace.input.count == 0) {
        furnace.input.type = kAir;
      }
      if (furnace.fuel.count == 0) {
        furnace.fuel.type = kAir;
      }
      if (furnace.output.count == 0) {
        furnace.output.type = kAir;
      }
      loaded.furnaces.push_back(furnace);
    }
    return true;
  };

  auto loadDroppedItemSection = [&](size_t droppedItemCount) -> bool {
    loaded.droppedItems.clear();
    loaded.droppedItems.reserve(droppedItemCount);
    for (size_t i = 0; i < droppedItemCount; ++i) {
      SavedDroppedItem item{};
      int rawType = 0;
      int onGroundValue = 0;
      if (!(in >> rawType
              >> item.count
              >> item.pos.x
              >> item.pos.y
              >> item.pos.z
              >> item.vel.x
              >> item.vel.y
              >> item.vel.z
              >> item.age
              >> item.pickupDelay
              >> item.spinPhase
              >> onGroundValue)) {
        return false;
      }
      item.type = static_cast<uint8_t>(std::clamp(rawType, 0, 255));
      if (item.count == 0) {
        item.type = kAir;
      }
      item.age = std::max(0.0f, item.age);
      item.pickupDelay = std::max(0.0f, item.pickupDelay);
      item.onGround = onGroundValue != 0;
      loaded.droppedItems.push_back(item);
    }
    return true;
  };

  auto loadPlayerStateModern = [&](bool scaleLegacyHealth,
                                   bool allowZeroHealth,
                                   bool hasDroppedItems) -> bool {
    size_t furnaceCount = 0;
    size_t droppedItemCount = 0;
    int cheatFlightValue = 0;
    int flyingValue = 0;
    if (!(in >> loaded.pos.x
            >> loaded.pos.y
            >> loaded.pos.z
            >> loaded.respawnPos.x
            >> loaded.respawnPos.y
            >> loaded.respawnPos.z
            >> loaded.yaw
            >> loaded.pitch
            >> loaded.selectedSlot
            >> loaded.achievementMask
            >> loaded.health
            >> cheatFlightValue
            >> flyingValue)) {
      return false;
    }
    if (!readStacks(loaded.hotbar) || !readStacks(loaded.inventory)) {
      return false;
    }
    if (!(in >> furnaceCount) || !loadFurnaceSection(furnaceCount)) {
      return false;
    }
    if (hasDroppedItems) {
      if (!(in >> droppedItemCount) || !loadDroppedItemSection(droppedItemCount)) {
        return false;
      }
    }
    loaded.selectedSlot = std::clamp(loaded.selectedSlot, 0, static_cast<int>(hotbar.size()) - 1);
    if (scaleLegacyHealth) {
      loaded.health *= 2;
    }
    loaded.health = std::clamp(loaded.health, allowZeroHealth ? 0 : 1, kMaxPlayerHealth);
    loaded.cheatFlightEnabled = cheatFlightValue != 0;
    loaded.flying = flyingValue != 0;
    outState = loaded;
    return true;
  };

  if (firstToken == "CUBEOS_PLAYER_V6") {
    return loadPlayerStateModern(false, true, true);
  }

  if (firstToken == "CUBEOS_PLAYER_V5") {
    return loadPlayerStateModern(false, false, false);
  }

  if (firstToken == "CUBEOS_PLAYER_V4") {
    return loadPlayerStateModern(true, false, false);
  }

  if (firstToken == "CUBEOS_PLAYER_V3") {
    size_t furnaceCount = 0;
    if (!(in >> loaded.pos.x
            >> loaded.pos.y
            >> loaded.pos.z
            >> loaded.yaw
            >> loaded.pitch
            >> loaded.selectedSlot
            >> loaded.achievementMask)) {
      return false;
    }
    if (!readStacks(loaded.hotbar) || !readStacks(loaded.inventory)) {
      return false;
    }
    if (!(in >> furnaceCount) || !loadFurnaceSection(furnaceCount)) {
      return false;
    }
    loaded.selectedSlot = std::clamp(loaded.selectedSlot, 0, static_cast<int>(hotbar.size()) - 1);
    loaded.respawnPos = loaded.pos;
    loaded.health = kMaxPlayerHealth;
    outState = loaded;
    return true;
  }

  if (firstToken == "CUBEOS_PLAYER_V2") {
    if (!(in >> loaded.pos.x
            >> loaded.pos.y
            >> loaded.pos.z
            >> loaded.yaw
            >> loaded.pitch
            >> loaded.selectedSlot
            >> loaded.achievementMask)) {
      return false;
    }
    if (!readStacks(loaded.hotbar) || !readStacks(loaded.inventory)) {
      return false;
    }
    loaded.selectedSlot = std::clamp(loaded.selectedSlot, 0, static_cast<int>(hotbar.size()) - 1);
    loaded.respawnPos = loaded.pos;
    loaded.health = kMaxPlayerHealth;
    outState = loaded;
    return true;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float loadedYaw = -90.0f;
  float loadedPitch = 0.0f;
  try {
    x = std::stof(firstToken);
  } catch (...) {
    return false;
  }
  if (!(in >> y >> z >> loadedYaw >> loadedPitch)) {
    return false;
  }

  loaded.pos = glm::vec3(x, y, z);
  loaded.respawnPos = loaded.pos;
  loaded.yaw = loadedYaw;
  loaded.pitch = loadedPitch;
  loaded.health = kMaxPlayerHealth;
  outState = loaded;
  return true;
}

void App::beginWorldSelectFlow() {
  refreshWorldSelectEntries();
  worldSelectSelection = 0;
  worldSelectScroll = 0;
  setScreenState(ScreenState::kWorldSelect);
}

void App::loadWorldFromSelection(int entryIndex) {
  if (entryIndex < 0 || entryIndex >= static_cast<int>(worldSelectEntries.size())) {
    return;
  }

  setScreenState(ScreenState::kLoadingWorld);
  const bool ru = appliedSettings.language == 1;
  renderLoadingFrame(0.05f, ru ? "Чтение данных мира" : "Reading world data");

  const WorldSelectEntry& entry = worldSelectEntries[static_cast<size_t>(entryIndex)];
  currentWorldPath = entry.path;
  if (!world.load(currentWorldPath)) {
    currentWorldPath.clear();
    refreshWorldSelectEntries();
    setScreenState(ScreenState::kWorldSelect);
    return;
  }

  renderLoadingFrame(0.18f, ru ? "Подготовка настроек" : "Preparing settings");

  pendingWorldName = entry.displayName;
  pendingWorldSettings = world.getGenerationSettings();
  pendingWorldSettings.generateStructures = true;
  world.setGenerationSettings(pendingWorldSettings);
  pendingSeedText = std::to_string(world.getSeed());
  pendingPlayerState = {};
  hasPendingPlayerResume = loadPlayerStateForWorld(currentWorldPath, pendingPlayerState);

  for (ItemStack& slot : hotbar) {
    slot.type = kAir;
    slot.count = 0;
  }
  for (ItemStack& slot : inventory) {
    slot.type = kAir;
    slot.count = 0;
  }
  for (ItemStack& slot : craftingSlots) {
    slot.type = kAir;
    slot.count = 0;
  }
  cursorStack.type = kAir;
  cursorStack.count = 0;
  selectedSlot = 0;
  achievementMask = 0;
  furnaceStates.clear();
  furnaceOpen = false;
  achievementPopupVisible = false;
  achievementPopupQueue.clear();
  achievementPopupTimer = 0.0f;

  if (hasPendingPlayerResume) {
    hotbar = pendingPlayerState.hotbar;
    inventory = pendingPlayerState.inventory;
    selectedSlot = std::clamp(pendingPlayerState.selectedSlot, 0, static_cast<int>(hotbar.size()) - 1);
    achievementMask = pendingPlayerState.achievementMask;
    for (const SavedFurnace& saved : pendingPlayerState.furnaces) {
      FurnaceState furnace{};
      furnace.input = saved.input;
      furnace.fuel = saved.fuel;
      furnace.output = saved.output;
      furnace.burnTime = saved.burnTime;
      furnace.burnDuration = saved.burnDuration;
      furnace.smeltProgress = saved.smeltProgress;
      furnaceStates[furnaceKeyForBlock(saved.pos)] = furnace;
    }
  }

  bool storageEmpty = true;
  for (const ItemStack& slot : hotbar) {
    storageEmpty = storageEmpty && (slot.count == 0 || slot.type == kAir);
  }
  for (const ItemStack& slot : inventory) {
    storageEmpty = storageEmpty && (slot.count == 0 || slot.type == kAir);
  }

  if ((!hasPendingPlayerResume || storageEmpty) && isCreativeWorldMode(pendingWorldSettings)) {
    populateCreativeHotbar(hotbar);
  }

  renderLoadingFrame(0.24f, ru ? "Подготовка спавна" : "Preparing spawn");
  setupGameplaySession();
}

void App::beginCreateWorldFlow() {
  createWorldSelection = 0;
  pendingWorldName = "World";
  pendingWorldSettings = world.getGenerationSettings();
  pendingWorldSettings.preset = WorldPreset::kMinecraftStyle;
  pendingWorldSettings.generateStructures = true;
  pendingWorldSettings.caveDensity = std::clamp(pendingWorldSettings.caveDensity, 0.25f, 2.5f);
  pendingWorldSettings.ravineFrequency = std::clamp(pendingWorldSettings.ravineFrequency, 0.25f, 2.5f);
  pendingWorldSettings.startInventoryMode = 0;
  pendingWorldSettings.cheatsEnabled = false;
  pendingSeedText.clear();
  hasPendingPlayerResume = false;
  pendingPlayerState = {};
  setScreenState(ScreenState::kCreateWorld);
}

void App::createWorldFromMenu() {
  pendingWorldName = trimAscii(pendingWorldName);
  if (pendingWorldName.empty()) {
    pendingWorldName = "World";
  }

  int seedValue = 0;
  bool parsedSeed = false;
  if (!pendingSeedText.empty()) {
    try {
      seedValue = std::stoi(pendingSeedText);
      parsedSeed = true;
    } catch (...) {
      parsedSeed = false;
    }
  }

  if (!parsedSeed) {
    uint64_t now = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    uint64_t entropy = now ^
                       (static_cast<uint64_t>(rd()) << 32) ^
                       static_cast<uint64_t>(rd());
    entropy ^= entropy >> 33;
    entropy *= 0xff51afd7ed558ccdULL;
    entropy ^= entropy >> 33;
    entropy *= 0xc4ceb9fe1a85ec53ULL;
    entropy ^= entropy >> 33;
    seedValue = static_cast<int>(entropy & 0x7fffffffu);
    if (seedValue == 0) {
      seedValue = 1337;
    }
  }

  WorldGenSettings settings = pendingWorldSettings;
  settings.generateStructures = true;
  settings.caveDensity = std::clamp(settings.caveDensity, 0.25f, 2.5f);
  settings.ravineFrequency = std::clamp(settings.ravineFrequency, 0.25f, 2.5f);

  std::string createdDisplayName;
  currentWorldPath = uniqueWorldPathForName(pendingWorldName, &createdDisplayName);
  pendingWorldName = createdDisplayName;
  saveWorldDisplayNameMeta(currentWorldPath, pendingWorldName);
  hasPendingPlayerResume = false;

  const bool ru = appliedSettings.language == 1;
  setScreenState(ScreenState::kLoadingWorld);
  renderLoadingFrame(0.05f, ru ? "Создание мира" : "Creating world");

  world.setSeed(seedValue);
  world.setGenerationSettings(settings);
  world.generate();
  renderLoadingFrame(0.20f, ru ? "Генерация ландшафта" : "Generating terrain");

  for (ItemStack& slot : hotbar) {
    slot.type = kAir;
    slot.count = 0;
  }
  for (ItemStack& slot : inventory) {
    slot.type = kAir;
    slot.count = 0;
  }
  for (ItemStack& slot : craftingSlots) {
    slot.type = kAir;
    slot.count = 0;
  }
  cursorStack.type = kAir;
  cursorStack.count = 0;
  selectedSlot = 0;
  achievementMask = 0;
  furnaceStates.clear();
  furnaceOpen = false;
  achievementPopupVisible = false;
  achievementPopupQueue.clear();
  achievementPopupTimer = 0.0f;

  if (isCreativeWorldMode(settings)) {
    populateCreativeHotbar(hotbar);
  }

  renderLoadingFrame(0.25f, ru ? "Подготовка спавна" : "Preparing spawn");
  setupGameplaySession();
}

void App::setupGameplaySession() {
  if (screenState != ScreenState::kLoadingWorld) {
    setScreenState(ScreenState::kLoadingWorld);
  }

  glm::vec3 sessionAnchor(8.0f, static_cast<float>(world.height() - 6), 8.0f);
  if (hasPendingPlayerResume) {
    sessionAnchor = pendingPlayerState.pos;
    sessionAnchor.y = std::clamp(sessionAnchor.y,
                                 static_cast<float>(world.getGenerationSettings().minY + 2),
                                 static_cast<float>(world.height() - 3));
  }
  playerPos = sessionAnchor;
  playerVel = glm::vec3(0.0f);
  onGround = false;
  playerHealth = kMaxPlayerHealth;
  cheatFlightEnabled = false;
  flying = false;
  fallDistance = 0.0f;
  footstepDistanceAccumulator = 0.0f;
  lastGrassFootstepVariant = -1;
  lastStoneFootstepVariant = -1;
  flightToggleTimer = 0.0f;
  jumpDown = false;
  inventoryOpen = false;
  workbenchOpen = false;
  furnaceOpen = false;
  breakingActive = false;
  breakingProgress = 0.0f;
  breakingStage = 0;
  droppedItems.clear();
  droppedItemMeshUploaded = false;
  droppedItemMeshTimer = 0.0f;
  sheepEntities.clear();
  sheepMeshUploaded = false;
  sheepMeshTimer = 0.0f;
  sheepSpawnTimer = 0.0f;
  cachedTorchLights.clear();
  torchLightRefreshTimer = 0.0f;
  torchLightsCacheValid = false;
  torchLightSampleSelectedBlock = kAir;
  interactionOverlayUploaded = false;
  interactionOutlineActive = false;
  interactionPreviewActive = false;
  interactionPreviewType = kAir;
  pendingWorldChunkUploads.erase(kInteractionOverlayMeshKey);
  pendingWorldChunkRemovals.erase(kInteractionOverlayMeshKey);
  achievementPopupVisible = false;
  achievementPopupQueue.clear();
  achievementPopupTimer = 0.0f;
  world.clearBreakOverlay();
  resetEnvironmentForSession();

  int initialCx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int initialCz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  int startupViewRadius = startupViewRadiusForQuality(appliedSettings.graphicsQuality, activeChunkViewRadius);
  int spawnChunkRadius = std::max(3, startupViewRadius);
  int preloadRadius = std::max(2, std::min(startupViewRadius, 4));
  const bool ru = appliedSettings.language == 1;
  world.updateActiveChunks(initialCx, initialCz, spawnChunkRadius);
  renderLoadingFrame(0.32f, ru ? "Генерация чанков" : "Generating chunks");

  auto countReadyChunks = [&](int radius) {
    int ready = 0;
    for (int cz = initialCz - radius; cz <= initialCz + radius; ++cz) {
      for (int cx = initialCx - radius; cx <= initialCx + radius; ++cx) {
        if (world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise) {
          ++ready;
        }
      }
    }
    return ready;
  };

  auto preloadRadiusWithAnimation = [&](int radius,
                                        float progressStart,
                                        float progressEnd,
                                        const std::string& message) {
    int diameter = radius * 2 + 1;
    int total = diameter * diameter;
    if (total <= 0) {
      renderLoadingFrame(progressEnd, message);
      return true;
    }

    while (true) {
      world.waitForChunkRegion(initialCx, initialCz, radius, 12);
      int ready = countReadyChunks(radius);
      float ratio = std::clamp(static_cast<float>(ready) / static_cast<float>(total), 0.0f, 1.0f);
      std::string status = message + " " + std::to_string(ready) + "/" + std::to_string(total);
      float progress = progressStart + (progressEnd - progressStart) * ratio;
      renderLoadingFrame(progress, status);

      if (ready >= total) {
        renderLoadingFrame(progressEnd, message);
        return true;
      }
      if (window && glfwWindowShouldClose(window)) {
        return false;
      }
    }
  };

  if (!preloadRadiusWithAnimation(2, 0.32f, 0.56f, ru ? "Подготовка зоны спавна" : "Preparing spawn area")) {
    return;
  }
  if (!preloadRadiusWithAnimation(preloadRadius, 0.56f, 0.84f, ru ? "Предзагрузка мира" : "Preloading world")) {
    return;
  }
  renderLoadingFrame(0.84f, ru ? "Поиск спавна" : "Searching spawn");

  auto isSpawnGround = [](uint8_t block) {
    return block != kAir &&
           !isWaterBlock(block) &&
           !isDecorationBlock(block) &&
           block != kLeaves &&
           block != kGravel &&
           block != kLootCache;
  };

  auto isPreferredSpawnGround = [](uint8_t block) {
    return block == kGrass || block == kDirt || block == kSand;
  };

  auto isSkyPassable = [](uint8_t block) {
    return block == kAir || block == kLeaves || isDecorationBlock(block);
  };

  auto findSurfaceSpawn = [&]() -> glm::vec3 {
    int baseX = static_cast<int>(std::floor(playerPos.x));
    int baseZ = static_cast<int>(std::floor(playerPos.z));
    int spawnSearchRadius = spawnChunkRadius * kChunkSize - 2;
    constexpr int kSeaLevel = 32;

    struct SpawnCandidate {
      int x = 0;
      int z = 0;
      int y = 0;
      uint8_t ground = kAir;
      int radius = 0;
    };

    struct ProbeCacheEntry {
      bool surfaceResolved = false;
      bool surfaceFound = false;
      int surfaceY = 0;
      uint8_t surfaceGround = kAir;
      bool waterResolved = false;
      bool waterFound = false;
      int waterY = 0;
    };

    auto probeKey = [](int x, int z) -> uint64_t {
      return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
             static_cast<uint64_t>(static_cast<uint32_t>(z));
    };

    std::unordered_map<uint64_t, ProbeCacheEntry> probeCache;
    int searchDiameter = spawnSearchRadius * 2 + 1;
    if (searchDiameter > 0) {
      size_t searchArea = static_cast<size_t>(searchDiameter) * static_cast<size_t>(searchDiameter);
      probeCache.reserve(searchArea / 2 + 1024);
    }

    auto probeSurface = [&](int x, int z, int& outY, uint8_t& outGround) -> bool {
      ProbeCacheEntry& cached = probeCache[probeKey(x, z)];
      if (cached.surfaceResolved) {
        if (!cached.surfaceFound) {
          return false;
        }
        outY = cached.surfaceY;
        outGround = cached.surfaceGround;
        return true;
      }

      cached.surfaceResolved = true;
      for (int y = world.height() - 3; y >= 1; --y) {
        uint8_t ground = world.getBlock(x, y, z);
        uint8_t feet = world.getBlock(x, y + 1, z);
        uint8_t head = world.getBlock(x, y + 2, z);
        if (!(isSpawnGround(ground) && feet == kAir && head == kAir)) {
          continue;
        }

        bool openSky = true;
        for (int sy = y + 3; sy < world.height(); ++sy) {
          uint8_t skyBlock = world.getBlock(x, sy, z);
          if (!isSkyPassable(skyBlock)) {
            openSky = false;
            break;
          }
        }
        if (!openSky) {
          continue;
        }

        cached.surfaceFound = true;
        cached.surfaceY = y;
        cached.surfaceGround = ground;
        outY = y;
        outGround = ground;
        return true;
      }
      return false;
    };

    auto probeWaterSurface = [&](int x, int z, int& outWaterY) -> bool {
      ProbeCacheEntry& cached = probeCache[probeKey(x, z)];
      if (cached.waterResolved) {
        if (!cached.waterFound) {
          return false;
        }
        outWaterY = cached.waterY;
        return true;
      }

      cached.waterResolved = true;
      for (int y = world.height() - 3; y >= 1; --y) {
        uint8_t waterBlock = world.getBlock(x, y, z);
        uint8_t above = world.getBlock(x, y + 1, z);
        if (!isWaterBlock(waterBlock) || above != kAir) {
          continue;
        }
        cached.waterFound = true;
        cached.waterY = y;
        outWaterY = y;
        return true;
      }
      return false;
    };

    std::vector<SpawnCandidate> candidates;
    candidates.reserve(static_cast<size_t>((spawnSearchRadius * 2 + 1) * (spawnSearchRadius * 2 + 1)));

    int maxSurfaceY = std::numeric_limits<int>::min();
    for (int radius = 0; radius <= spawnSearchRadius; ++radius) {
      for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
          if (std::max(std::abs(dx), std::abs(dz)) != radius) {
            continue;
          }
          int x = baseX + dx;
          int z = baseZ + dz;
          int y = 0;
          uint8_t ground = kAir;
          if (!probeSurface(x, z, y, ground)) {
            continue;
          }
          maxSurfaceY = std::max(maxSurfaceY, y);
          candidates.push_back({x, z, y, ground, radius});
        }
      }
    }

    auto roughness = [&](const SpawnCandidate& c) -> int {
      static constexpr int kNeighborOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      int total = 0;
      for (const auto& offset : kNeighborOffsets) {
        int ny = 0;
        uint8_t neighborGround = kAir;
        if (!probeSurface(c.x + offset[0], c.z + offset[1], ny, neighborGround)) {
          total += 12;
          continue;
        }
        total += std::abs(c.y - ny);
      }
      return total;
    };

    auto ledgeRisk = [&](const SpawnCandidate& c) -> int {
      static constexpr int kRingOffsets[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
      };
      int risk = 0;
      for (const auto& offset : kRingOffsets) {
        int ny = 0;
        uint8_t nGround = kAir;
        if (!probeSurface(c.x + offset[0], c.z + offset[1], ny, nGround)) {
          risk += 5;
          continue;
        }
        int delta = std::abs(c.y - ny);
        if (delta >= 3) {
          risk += 4;
        } else if (delta == 2) {
          risk += 2;
        }
        if (!isSpawnGround(nGround)) {
          risk += 3;
        }
      }
      return risk;
    };

    auto waterExposure = [&](const SpawnCandidate& c) -> int {
      static constexpr int kWaterOffsets[12][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
        {2, 0}, {-2, 0}, {0, 2}, {0, -2}
      };
      int exposure = 0;
      for (const auto& offset : kWaterOffsets) {
        int waterY = 0;
        if (!probeWaterSurface(c.x + offset[0], c.z + offset[1], waterY)) {
          continue;
        }
        int levelDelta = std::abs(c.y - waterY);
        if (levelDelta <= 2) {
          int ringDist = std::abs(offset[0]) + std::abs(offset[1]);
          exposure += ringDist <= 1 ? 4 : 2;
        }
      }
      return exposure;
    };

    auto nearbyWoodScore = [&](const SpawnCandidate& c) -> int {
      int score = 0;
      for (int dz = -7; dz <= 7; ++dz) {
        for (int dx = -7; dx <= 7; ++dx) {
          int wx = c.x + dx;
          int wz = c.z + dz;
          if (!world.inBounds(wx, std::clamp(c.y, 0, world.height() - 1), wz)) {
            continue;
          }
          for (int y = std::max(1, c.y - 1); y <= std::min(world.height() - 2, c.y + 8); ++y) {
            if (world.getBlock(wx, y, wz) == kWood) {
              score += std::max(1, 8 - std::max(std::abs(dx), std::abs(dz)));
              break;
            }
          }
        }
      }
      return std::min(score, 18);
    };

    auto nearbyStoneScore = [&](const SpawnCandidate& c) -> int {
      auto exposed = [&](int x, int y, int z) {
        static constexpr int kDirs[6][3] = {
          {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
          {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };
        for (const auto& dir : kDirs) {
          int nx = x + dir[0];
          int ny = y + dir[1];
          int nz = z + dir[2];
          if (!world.inBounds(nx, ny, nz)) {
            continue;
          }
          uint8_t neighbor = world.getBlock(nx, ny, nz);
          if (neighbor == kAir || isWaterBlock(neighbor) || isDecorationBlock(neighbor)) {
            return true;
          }
        }
        return false;
      };

      int score = 0;
      for (int dz = -6; dz <= 6; ++dz) {
        for (int dx = -6; dx <= 6; ++dx) {
          int wx = c.x + dx;
          int wz = c.z + dz;
          bool foundColumn = false;
          for (int y = std::max(1, c.y - 4); y <= std::min(world.height() - 2, c.y + 4); ++y) {
            if (!world.inBounds(wx, y, wz)) {
              continue;
            }
            uint8_t block = world.getBlock(wx, y, wz);
            if (block != kStone && block != kCoalOre && block != kIronOre) {
              continue;
            }
            if (!exposed(wx, y, wz)) {
              continue;
            }
            score += (block == kIronOre) ? 4 : (block == kCoalOre ? 3 : 1);
            foundColumn = true;
            break;
          }
          if (foundColumn && score >= 20) {
            return score;
          }
        }
      }
      return score;
    };

    auto nearbyCaveScore = [&](const SpawnCandidate& c) -> int {
      int score = 0;
      for (int dz = -8; dz <= 8; dz += 2) {
        for (int dx = -8; dx <= 8; dx += 2) {
          int wx = c.x + dx;
          int wz = c.z + dz;
          int surfaceY = 0;
          uint8_t ground = kAir;
          if (!probeSurface(wx, wz, surfaceY, ground)) {
            continue;
          }
          for (int y = surfaceY; y >= std::max(surfaceY - 6, 2); --y) {
            if (!world.inBounds(wx, y + 1, wz)) {
              continue;
            }
            if (world.getBlock(wx, y, wz) != kAir || world.getBlock(wx, y + 1, wz) != kAir) {
              continue;
            }
            uint8_t below = world.getBlock(wx, y - 1, wz);
            if (below == kAir || isWaterBlock(below) || isDecorationBlock(below)) {
              continue;
            }
            bool hasRoof = false;
            for (int roofY = y + 2; roofY <= std::min(y + 6, world.height() - 1); ++roofY) {
              uint8_t roof = world.getBlock(wx, roofY, wz);
              if (!isSkyPassable(roof) && !isWaterBlock(roof)) {
                hasRoof = true;
                break;
              }
            }
            if (!hasRoof) {
              continue;
            }
            score += std::max(1, 6 - std::max(std::abs(dx), std::abs(dz)) / 2);
            break;
          }
        }
      }
      return std::min(score, 16);
    };

    glm::vec3 bestPos{};
    auto pickBest = [&](bool highBandOnly,
                        bool preferredOnly,
                        int maxRoughness,
                        int maxLedgeRisk,
                        int maxWaterExposure) -> bool {
      int bestScore = std::numeric_limits<int>::min();
      bool found = false;
      int highBandFloor = maxSurfaceY - 18;

      for (const SpawnCandidate& c : candidates) {
        if (highBandOnly && c.y < highBandFloor) {
          continue;
        }
        if (preferredOnly && !isPreferredSpawnGround(c.ground)) {
          continue;
        }

        int surfaceRoughness = roughness(c);
        if (surfaceRoughness > maxRoughness) {
          continue;
        }
        int localLedgeRisk = ledgeRisk(c);
        if (localLedgeRisk > maxLedgeRisk) {
          continue;
        }
        int localWaterExposure = waterExposure(c);
        if (localWaterExposure > maxWaterExposure) {
          continue;
        }
        int woodScore = nearbyWoodScore(c);
        int stoneScore = nearbyStoneScore(c);
        int caveScore = nearbyCaveScore(c);
        int scenicRelief = std::clamp(surfaceRoughness, 2, 10);
        int scenicBonus = scenicRelief * 12;
        if (surfaceRoughness <= 1) {
          scenicBonus -= 20;
        }
        if (surfaceRoughness >= 7) {
          scenicBonus -= (surfaceRoughness - 6) * 8;
        }

        int score = c.y * 54
                    - c.radius * 5
                    - surfaceRoughness * 6
                    - localLedgeRisk * 10
                    - localWaterExposure * 7
                    + scenicBonus
                    + woodScore * 18
                    + stoneScore * 12
                    + caveScore * 22;
        if (isPreferredSpawnGround(c.ground)) {
          score += 260;
        }
        if (woodScore > 0) {
          score += 120;
        }
        if (stoneScore > 0) {
          score += 90;
        }
        if (caveScore > 0) {
          score += 110;
        }

        if (score > bestScore) {
          bestScore = score;
          found = true;
          bestPos = {static_cast<float>(c.x) + 0.5f,
                     static_cast<float>(c.y) + 2.35f,
                     static_cast<float>(c.z) + 0.5f};
        }
      }
      return found;
    };

    if (candidates.empty()) {
      std::vector<SpawnCandidate> waterCandidates;
      waterCandidates.reserve(static_cast<size_t>((spawnSearchRadius * 2 + 1) * (spawnSearchRadius * 2 + 1)));

      for (int radius = 0; radius <= spawnSearchRadius; ++radius) {
        for (int dz = -radius; dz <= radius; ++dz) {
          for (int dx = -radius; dx <= radius; ++dx) {
            if (std::max(std::abs(dx), std::abs(dz)) != radius) {
              continue;
            }
            int x = baseX + dx;
            int z = baseZ + dz;
            int waterY = 0;
            if (!probeWaterSurface(x, z, waterY)) {
              continue;
            }
            waterCandidates.push_back({x, z, waterY, kWater, radius});
          }
        }
      }

      if (!waterCandidates.empty()) {
        const SpawnCandidate* bestWater = &waterCandidates.front();
        int bestScore = std::numeric_limits<int>::min();
        for (const SpawnCandidate& c : waterCandidates) {
          int score = c.y * 2 - c.radius * 14;
          if (score > bestScore) {
            bestScore = score;
            bestWater = &c;
          }
        }

        int platformTopY = std::clamp(bestWater->y + 1, 3, world.height() - 4);
        for (int oz = -2; oz <= 2; ++oz) {
          for (int ox = -2; ox <= 2; ++ox) {
            int x = bestWater->x + ox;
            int z = bestWater->z + oz;
            uint8_t below = world.getBlock(x, platformTopY - 1, z);
            if (below == kAir || isWaterBlock(below) || isDecorationBlock(below) || below == kLeaves) {
              world.setBlock(x, platformTopY - 1, z, kStone);
            }

            uint8_t floorBlock = world.getBlock(x, platformTopY, z);
            if (floorBlock == kAir || isWaterBlock(floorBlock) || isDecorationBlock(floorBlock) || floorBlock == kLeaves) {
              world.setBlock(x, platformTopY, z, kDirt);
            }

            for (int y = platformTopY + 1; y <= platformTopY + 3; ++y) {
              uint8_t headBlock = world.getBlock(x, y, z);
              if (isWaterBlock(headBlock) || isDecorationBlock(headBlock) || headBlock == kLeaves) {
                world.setBlock(x, y, z, kAir);
              }
            }
          }
        }

        return {static_cast<float>(bestWater->x) + 0.5f,
                static_cast<float>(platformTopY) + 2.35f,
                static_cast<float>(bestWater->z) + 0.5f};
      }

      int highestSolid = 0;
      for (int y = world.height() - 3; y >= 1; --y) {
        uint8_t block = world.getBlock(baseX, y, baseZ);
        if (block == kAir || isWaterBlock(block) || isDecorationBlock(block) || block == kLeaves) {
          continue;
        }
        highestSolid = y;
        break;
      }

      int safeY = std::clamp(std::max(highestSolid + 2, kSeaLevel + 2), 6, world.height() - 6);
      return {static_cast<float>(baseX) + 0.5f,
              static_cast<float>(safeY),
              static_cast<float>(baseZ) + 0.5f};
    }

    if (!pickBest(true, true, 8, 6, 8) &&
        !pickBest(true, false, 10, 8, 10) &&
        !pickBest(false, true, 14, 11, 14) &&
        !pickBest(false, false, 20, 18, 20)) {
      const SpawnCandidate* fallback = &candidates.front();
      for (const SpawnCandidate& c : candidates) {
        if (c.y > fallback->y || (c.y == fallback->y && c.radius < fallback->radius)) {
          fallback = &c;
        }
      }
      bestPos = {static_cast<float>(fallback->x) + 0.5f,
                 static_cast<float>(fallback->y) + 2.35f,
                 static_cast<float>(fallback->z) + 0.5f};
    }

    return bestPos;
  };

  playerPos = findSurfaceSpawn();
  respawnPos = playerPos;
  if (hasPendingPlayerResume) {
    bool resumeAfterDeath = pendingPlayerState.health <= 0;
    glm::vec3 resumePos = pendingPlayerState.pos;
    int rx = static_cast<int>(std::floor(resumePos.x));
    int ry = static_cast<int>(std::floor(resumePos.y));
    int rz = static_cast<int>(std::floor(resumePos.z));

    auto hasNearbySupport = [&](int x, int y, int z) {
      if (y <= 1) {
        return false;
      }
      constexpr int kMaxDropForResume = 48;
      int minY = std::max(1, y - kMaxDropForResume);
      for (int sy = y - 1; sy >= minY; --sy) {
        uint8_t block = world.getBlock(x, sy, z);
        if (block == kAir || block == kLeaves || isDecorationBlock(block)) {
          continue;
        }
        return true;
      }
      return false;
    };

    bool resumeInBounds = world.inBounds(rx, ry, rz) &&
                          world.inBounds(rx, ry + 1, rz);
    bool resumeSafe = hasNearbySupport(rx, ry, rz);
    if (!resumeAfterDeath && resumeInBounds && !collidesAt(resumePos) && resumeSafe) {
      playerPos = resumePos;
      yaw = pendingPlayerState.yaw;
      pitch = std::clamp(pendingPlayerState.pitch, -89.0f, 89.0f);
    }
    if (world.inBounds(static_cast<int>(std::floor(pendingPlayerState.respawnPos.x)),
                       static_cast<int>(std::floor(pendingPlayerState.respawnPos.y)),
                       static_cast<int>(std::floor(pendingPlayerState.respawnPos.z)))) {
      respawnPos = pendingPlayerState.respawnPos;
    }
    if (resumeAfterDeath) {
      playerPos = respawnPos;
      yaw = pendingPlayerState.yaw;
      pitch = std::clamp(pendingPlayerState.pitch, -89.0f, 89.0f);
      playerHealth = kMaxPlayerHealth;
    } else {
      playerHealth = std::clamp(pendingPlayerState.health, 1, kMaxPlayerHealth);
    }
    cheatFlightEnabled = pendingPlayerState.cheatFlightEnabled && pendingWorldSettings.cheatsEnabled;
    flying = !resumeAfterDeath &&
             pendingPlayerState.flying &&
             (isCreativeWorldMode(pendingWorldSettings) || cheatFlightEnabled);
    droppedItems.clear();
    droppedItems.reserve(std::min<size_t>(pendingPlayerState.droppedItems.size(), 384));
    for (const SavedDroppedItem& saved : pendingPlayerState.droppedItems) {
      if (saved.type == kAir || saved.count == 0 || isWaterBlock(saved.type)) {
        continue;
      }
      int itemX = static_cast<int>(std::floor(saved.pos.x));
      int itemY = static_cast<int>(std::floor(saved.pos.y));
      int itemZ = static_cast<int>(std::floor(saved.pos.z));
      if (!world.inBounds(itemX, itemY, itemZ)) {
        continue;
      }

      DroppedItemEntity item;
      item.type = saved.type;
      item.count = saved.count;
      item.pos = saved.pos;
      item.vel = saved.vel;
      item.age = std::clamp(saved.age, 0.0f, 180.0f);
      item.pickupDelay = std::max(0.0f, saved.pickupDelay);
      item.spinPhase = saved.spinPhase;
      item.onGround = saved.onGround;
      droppedItems.push_back(item);
      if (droppedItems.size() >= 384) {
        break;
      }
    }
    hasPendingPlayerResume = false;
    pendingPlayerState = {};
  }
  while (collidesAt(playerPos) && playerPos.y < static_cast<float>(world.height() - 3)) {
    playerPos.y += 0.35f;
  }
  playerVel = glm::vec3(0.0f);
  onGround = false;
  fallDistance = 0.0f;
  flightToggleTimer = 0.0f;
  jumpDown = false;

  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  int initialLoadedViewRadius = std::min(startupViewRadius, kMinRenderDistance);
  world.updateActiveChunks(cx, cz, initialLoadedViewRadius);
  renderLoadingFrame(0.90f,
                     ru
                       ? "Подготовка ближайших чанков"
                       : "Preparing nearby chunks");
  (void)world.waitForChunkRegion(cx, cz, 1, 1200, ChunkGenStatus::kFull);
  loadedChunkViewRadius = initialLoadedViewRadius;
  nextStreamingRadiusExpandTime = glfwGetTime() + streamingExpansionIntervalForQuality(appliedSettings.graphicsQuality);
  currentChunkX = cx;
  currentChunkZ = cz;
  chunkCenterValid = true;

  const bool ruUi = appliedSettings.language == 1;
  renderLoadingFrame(0.95f, ruUi ? "Сборка рендера чанков" : "Building chunk render data");
  std::vector<ChunkMeshUpload> worldSnapshot;
  world.snapshotChunkMeshes(worldSnapshot);
  rebuildUiMesh();
  composeMeshData();
  std::vector<VulkanContext::WorldChunkMeshUpload> vkWorldSnapshot =
    toVkChunkUploads(std::move(worldSnapshot));
  if (vkReady) {
    vk.updateMesh(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
    vk.setWorldChunkMeshes(vkWorldSnapshot);
  } else {
    vk.setMeshData(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
  }
  pendingWorldChunkUploads.clear();
  pendingWorldChunkRemovals.clear();
  lastWorldChunkUploadTime = glfwGetTime();
  uiDirty = false;
  refreshSelectedBlock();
  refreshAchievementsProgress();
  syncDroppedItemMesh(true);
  populateSheepForSession();
  syncSheepMesh(true);

  renderLoadingFrame(1.0f, ruUi ? "Готово" : "Done");
  setScreenState(ScreenState::kPlaying);

  saveWorldWithWarning(world, currentWorldPath, "world session setup");
}

void App::processMenuInput(float deltaTime) {
  (void)deltaTime;

  bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool leftPressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool rightPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
  bool backspacePressed = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
  bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

  double mouseX = 0.0;
  double mouseY = 0.0;
  glfwGetCursorPos(window, &mouseX, &mouseY);
  glm::vec2 menuMouseFb = cursorToFramebuffer(mouseX, mouseY);
  if (std::abs(cursorFbX - menuMouseFb.x) > 0.01f ||
      std::abs(cursorFbY - menuMouseFb.y) > 0.01f) {
    cursorFbX = menuMouseFb.x;
    cursorFbY = menuMouseFb.y;
    uiDirty = true;
  }
  const float uiW = static_cast<float>(uiLayoutWidth());
  const float uiH = static_cast<float>(uiLayoutHeight());
  bool mouseLeftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool mouseRightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  bool mouseLeftClicked = mouseLeftPressed && !mouseLeftDown;
  bool mouseRightClicked = mouseRightPressed && !mouseRightDown;

  GLFWgamepadstate gamepadState{};
  bool hasMenuGamepad = false;
  for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
    if (glfwJoystickIsGamepad(jid) == GLFW_TRUE &&
        glfwGetGamepadState(jid, &gamepadState) == GLFW_TRUE) {
      hasMenuGamepad = true;
      break;
    }
  }

  if (hasMenuGamepad) {
    constexpr float kStickThreshold = 0.55f;
    upPressed = upPressed ||
                gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -kStickThreshold;
    downPressed = downPressed ||
                  gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                  gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > kStickThreshold;
    leftPressed = leftPressed ||
                  gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS ||
                  gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -kStickThreshold;
    rightPressed = rightPressed ||
                   gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS ||
                   gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] > kStickThreshold;
    enterPressed = enterPressed ||
                   gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS ||
                   gamepadState.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
    backspacePressed = backspacePressed ||
                       gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
    escPressed = escPressed ||
                 gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS ||
                 gamepadState.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS;
  }

  auto togglePreset = [&]() {
    pendingWorldSettings.preset = cycleCreateWorldPreset(pendingWorldSettings.preset, true);
    updateWindowTitle();
    uiDirty = true;
  };

  auto adjustCreateSetting = [&](bool increase) {
    constexpr float kStep = 0.25f;
    bool changed = false;
    switch (createWorldSelection) {
      case 2:
        pendingWorldSettings.preset = cycleCreateWorldPreset(pendingWorldSettings.preset, increase);
        changed = true;
        break;
      case 3: {
        float oldValue = pendingWorldSettings.caveDensity;
        float delta = increase ? kStep : -kStep;
        pendingWorldSettings.caveDensity = std::clamp(oldValue + delta, 0.25f, 2.5f);
        changed = (pendingWorldSettings.caveDensity != oldValue);
        break;
      }
      case 4: {
        float oldValue = pendingWorldSettings.ravineFrequency;
        float delta = increase ? kStep : -kStep;
        pendingWorldSettings.ravineFrequency = std::clamp(oldValue + delta, 0.25f, 2.5f);
        changed = (pendingWorldSettings.ravineFrequency != oldValue);
        break;
      }
      case 5: {
        pendingWorldSettings.startInventoryMode =
          pendingWorldSettings.startInventoryMode == 0 ? 1 : 0;
        changed = true;
        break;
      }
      case 6:
        pendingWorldSettings.cheatsEnabled = !pendingWorldSettings.cheatsEnabled;
        changed = true;
        break;
      default:
        break;
    }

    if (changed) {
      updateWindowTitle();
      uiDirty = true;
    }
  };

  auto updateSettingsDirtyFlag = [&]() {
    settingsDirty =
      pendingSettings.graphicsQuality != appliedSettings.graphicsQuality ||
      pendingSettings.renderDistance != appliedSettings.renderDistance ||
      std::abs(pendingSettings.sensitivity - appliedSettings.sensitivity) > 0.0001f ||
      pendingSettings.audioVolume != appliedSettings.audioVolume ||
      std::abs(pendingSettings.uiScale - appliedSettings.uiScale) > 0.001f ||
      pendingSettings.language != appliedSettings.language ||
      pendingSettings.blockGuides != appliedSettings.blockGuides;
  };

  auto syncSettingsState = [&]() {
    settingsCategory = std::clamp(settingsCategory, 0, kSettingsCategoryCount - 1);
    settingsOptionSelection =
      std::clamp(settingsOptionSelection, 0, std::max(0, settingsEntryCountForCategory(settingsCategory) - 1));
    settingsActionSelection = std::clamp(settingsActionSelection, 0, kSettingsActionCount - 1);
    settingsFocusArea = std::clamp(settingsFocusArea,
                                   static_cast<int>(SettingsFocusArea::kCategories),
                                   static_cast<int>(SettingsFocusArea::kActions));
  };

  auto openSettingsScreen = [&](ScreenState returnState) {
    settingsSelection = 0;
    settingsScroll = 0;
    settingsReturnState = returnState;
    settingsCategory = static_cast<int>(SettingsCategoryTab::kGraphics);
    settingsOptionSelection = 0;
    settingsActionSelection = 0;
    settingsFocusArea = static_cast<int>(SettingsFocusArea::kOptions);
    pendingSettings = appliedSettings;
    settingsDirty = false;
    setScreenState(ScreenState::kSettings);
  };

  auto setSettingsEntryFromNormalized = [&](SettingsEntryId entry, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (entry) {
      case SettingsEntryId::kRenderDistance:
        pendingSettings.renderDistance = std::clamp(
          kMinRenderDistance + static_cast<int>(std::lround(t * static_cast<float>(kMaxRenderDistance - kMinRenderDistance))),
          kMinRenderDistance,
          kMaxRenderDistance);
        break;
      case SettingsEntryId::kAudioVolume:
        pendingSettings.audioVolume = std::clamp(static_cast<int>(std::lround(t * 100.0f)), 0, 100);
        break;
      case SettingsEntryId::kSensitivity:
        pendingSettings.sensitivity = std::clamp(0.03f + t * (0.40f - 0.03f), 0.03f, 0.40f);
        break;
      case SettingsEntryId::kUiScale:
        pendingSettings.uiScale = clampUiScaleValue(kMinUiScale + t * (kMaxUiScale - kMinUiScale));
        break;
      default:
        return;
    }
    updateSettingsDirtyFlag();
    uiDirty = true;
  };

  auto adjustSettingsEntry = [&](SettingsEntryId entry, bool increase) {
    switch (entry) {
      case SettingsEntryId::kGraphicsQuality: {
        int delta = increase ? 1 : -1;
        pendingSettings.graphicsQuality = std::clamp(pendingSettings.graphicsQuality + delta, 0, 2);
        break;
      }
      case SettingsEntryId::kRenderDistance: {
        int delta = increase ? 1 : -1;
        pendingSettings.renderDistance =
          std::clamp(pendingSettings.renderDistance + delta, kMinRenderDistance, kMaxRenderDistance);
        break;
      }
      case SettingsEntryId::kAudioVolume: {
        int delta = increase ? 5 : -5;
        pendingSettings.audioVolume = std::clamp(pendingSettings.audioVolume + delta, 0, 100);
        break;
      }
      case SettingsEntryId::kSensitivity: {
        float delta = increase ? 0.01f : -0.01f;
        pendingSettings.sensitivity = std::clamp(pendingSettings.sensitivity + delta, 0.03f, 0.40f);
        break;
      }
      case SettingsEntryId::kUiScale: {
        float delta = increase ? kUiScaleStep : -kUiScaleStep;
        pendingSettings.uiScale = clampUiScaleValue(pendingSettings.uiScale + delta);
        break;
      }
      case SettingsEntryId::kLanguage:
        pendingSettings.language = pendingSettings.language == 0 ? 1 : 0;
        break;
      case SettingsEntryId::kBlockGuides:
        pendingSettings.blockGuides = !pendingSettings.blockGuides;
        break;
      default:
        return;
    }

    updateSettingsDirtyFlag();
    uiDirty = true;
  };

  auto activateSettingsAction = [&](SettingsActionId action) {
    switch (action) {
      case SettingsActionId::kApply: {
        bool inGameContext =
          settingsReturnState == ScreenState::kPlaying ||
          settingsReturnState == ScreenState::kPaused;
        applySettings(inGameContext);
        saveSettingsWithWarning("settings menu apply");
        break;
      }
      case SettingsActionId::kReset:
        pendingSettings = UserSettings{};
        updateSettingsDirtyFlag();
        uiDirty = true;
        break;
      case SettingsActionId::kBack:
      default:
        pendingSettings = appliedSettings;
        settingsDirty = false;
        setScreenState(settingsReturnState);
        break;
    }
  };

  auto fixedMenuRowAtMouse = [&](int rowCount) -> int {
    float t = static_cast<float>(glfwGetTime());
    float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
    float panelWidth = std::min(uiW * 0.72f, 640.0f);
    float panelHeight = std::min(uiH * 0.78f, 540.0f);
    float panelX = (uiW - panelWidth) * 0.5f;
    float panelY = (uiH - panelHeight) * 0.5f;
    panelY += (1.0f - intro) * 28.0f;
    panelY += std::sin(t * 1.4f) * 3.0f;

    float totalH = kMenuButtonHeight * static_cast<float>(rowCount) +
                   kMenuButtonGap * static_cast<float>(std::max(0, rowCount - 1));
    float bx = panelX + (panelWidth - kMenuButtonWidth) * 0.5f;
    float by = panelY + 90.0f + (panelHeight - 130.0f - totalH) * 0.5f;
    if (menuMouseFb.x < bx || menuMouseFb.x > bx + kMenuButtonWidth) {
      return -1;
    }

    for (int i = 0; i < rowCount; ++i) {
      float y = by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap);
      if (menuMouseFb.y >= y && menuMouseFb.y <= y + kMenuButtonHeight) {
        return i;
      }
    }
    return -1;
  };

  if (screenState == ScreenState::kMainMenu) {
    int hoveredRow = fixedMenuRowAtMouse(3);
    if (hoveredRow >= 0 && hoveredRow != mainMenuSelection) {
      mainMenuSelection = hoveredRow;
      updateWindowTitle();
      uiDirty = true;
    }
    if (upPressed && !menuUpDown) {
      mainMenuSelection = std::max(0, mainMenuSelection - 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      mainMenuSelection = std::min(2, mainMenuSelection + 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (enterPressed && !menuEnterDown) {
      if (mainMenuSelection == 0) {
        beginWorldSelectFlow();
      } else if (mainMenuSelection == 1) {
        openSettingsScreen(ScreenState::kMainMenu);
      } else {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
    }
    if (mouseLeftClicked && hoveredRow >= 0) {
      if (hoveredRow == 0) {
        beginWorldSelectFlow();
      } else if (hoveredRow == 1) {
        openSettingsScreen(ScreenState::kMainMenu);
      } else {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
    }
    if (escPressed && !menuEscDown) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  } else if (screenState == ScreenState::kPaused) {
    int hoveredRow = fixedMenuRowAtMouse(3);
    if (hoveredRow >= 0 && hoveredRow != pauseMenuSelection) {
      pauseMenuSelection = hoveredRow;
      updateWindowTitle();
      uiDirty = true;
    }
    if (upPressed && !menuUpDown) {
      pauseMenuSelection = std::max(0, pauseMenuSelection - 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      pauseMenuSelection = std::min(2, pauseMenuSelection + 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (enterPressed && !menuEnterDown) {
      if (pauseMenuSelection == 0) {
        setScreenState(ScreenState::kPlaying);
      } else if (pauseMenuSelection == 1) {
        openSettingsScreen(ScreenState::kPaused);
      } else {
        setScreenState(ScreenState::kMainMenu);
      }
    }
    if (mouseLeftClicked && hoveredRow >= 0) {
      if (hoveredRow == 0) {
        setScreenState(ScreenState::kPlaying);
      } else if (hoveredRow == 1) {
        openSettingsScreen(ScreenState::kPaused);
      } else {
        setScreenState(ScreenState::kMainMenu);
      }
    }
    if (escPressed && !menuEscDown) {
      setScreenState(ScreenState::kPlaying);
    }
  } else if (screenState == ScreenState::kDeath) {
    int hoveredRow = fixedMenuRowAtMouse(2);
    if (hoveredRow >= 0 && hoveredRow != deathMenuSelection) {
      deathMenuSelection = hoveredRow;
      updateWindowTitle();
      uiDirty = true;
    }
    if (upPressed && !menuUpDown) {
      deathMenuSelection = std::max(0, deathMenuSelection - 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      deathMenuSelection = std::min(1, deathMenuSelection + 1);
      updateWindowTitle();
      uiDirty = true;
    }
    auto activateDeathRow = [&]() {
      if (deathMenuSelection == 0) {
        respawnPlayer();
      } else {
        respawnPlayer();
        setScreenState(ScreenState::kMainMenu);
      }
    };
    if (enterPressed && !menuEnterDown) {
      activateDeathRow();
    }
    if (mouseLeftClicked && hoveredRow >= 0) {
      deathMenuSelection = hoveredRow;
      activateDeathRow();
    }
  } else if (screenState == ScreenState::kWorldSelect) {
    int rowCount = static_cast<int>(worldSelectEntries.size()) + 2;
    int visibleRows = computeWorldSelectVisibleRows(static_cast<int>(uiH), rowCount);

    auto syncWorldSelectScroll = [&]() {
      if (rowCount <= visibleRows) {
        worldSelectScroll = 0;
        return;
      }
      int maxScroll = rowCount - visibleRows;
      if (worldSelectSelection < worldSelectScroll) {
        worldSelectScroll = worldSelectSelection;
      } else if (worldSelectSelection >= worldSelectScroll + visibleRows) {
        worldSelectScroll = worldSelectSelection - visibleRows + 1;
      }
      worldSelectScroll = std::clamp(worldSelectScroll, 0, maxScroll);
    };
    syncWorldSelectScroll();

    auto activateWorldSelectRow = [&](int row) {
      if (row < 0 || row >= rowCount) {
        return;
      }
      if (row == 0) {
        beginCreateWorldFlow();
      } else if (row == rowCount - 1) {
        setScreenState(ScreenState::kMainMenu);
      } else {
        loadWorldFromSelection(row - 1);
      }
    };

    auto deleteWorldSelectRow = [&](int row) {
      if (row <= 0 || row >= rowCount - 1) {
        return;
      }

      const std::string worldPath = worldSelectEntries[static_cast<size_t>(row - 1)].path;
      std::error_code ec;
      std::filesystem::remove(std::filesystem::path(worldPath), ec);

      std::string metaPath = worldMetaPathForWorld(std::filesystem::path(worldPath));
      if (!metaPath.empty()) {
        ec.clear();
        std::filesystem::remove(std::filesystem::path(metaPath), ec);
      }

      std::string statePath = playerStatePathForWorld(worldPath);
      if (!statePath.empty()) {
        ec.clear();
        std::filesystem::remove(std::filesystem::path(statePath), ec);
      }

      if (currentWorldPath == worldPath) {
        currentWorldPath.clear();
      }

      refreshWorldSelectEntries();
      rowCount = static_cast<int>(worldSelectEntries.size()) + 2;
      worldSelectSelection = std::clamp(worldSelectSelection, 0, std::max(0, rowCount - 1));
      syncWorldSelectScroll();
      updateWindowTitle();
      uiDirty = true;
    };

    auto worldSelectRowAtMouse = [&]() -> int {
      float t = static_cast<float>(glfwGetTime());
      float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
      float panelWidth = std::min(uiW * 0.72f, 640.0f);
      float panelHeight = std::min(uiH * 0.78f, 540.0f);
      float panelX = (uiW - panelWidth) * 0.5f;
      float panelY = (uiH - panelHeight) * 0.5f;
      panelY += (1.0f - intro) * 28.0f;
      panelY += std::sin(t * 1.4f) * 3.0f;

      int startRow = 0;
      if (rowCount > visibleRows) {
        startRow = std::clamp(worldSelectScroll, 0, rowCount - visibleRows);
      }
      int endRow = std::min(rowCount, startRow + visibleRows);
      int drawRows = endRow - startRow;

      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 40.0f;
      float rowGap = 10.0f;
      float rowsH = static_cast<float>(drawRows) * rowH +
                    static_cast<float>(std::max(0, drawRows - 1)) * rowGap;
      float rowY = panelY + 84.0f + std::max(0.0f, (panelHeight - 116.0f - rowsH) * 0.5f);

      if (menuMouseFb.x < rowX || menuMouseFb.x > rowX + rowW) {
        return -1;
      }

      for (int row = startRow; row < endRow; ++row) {
        int drawIndex = row - startRow;
        float y = rowY + static_cast<float>(drawIndex) * (rowH + rowGap);
        if (menuMouseFb.y >= y && menuMouseFb.y <= y + rowH) {
          return row;
        }
      }
      return -1;
    };

    int hoveredRow = worldSelectRowAtMouse();
    if (hoveredRow >= 0 && hoveredRow != worldSelectSelection) {
      worldSelectSelection = hoveredRow;
      syncWorldSelectScroll();
      updateWindowTitle();
      uiDirty = true;
    }

    if (upPressed && !menuUpDown) {
      worldSelectSelection = std::max(0, worldSelectSelection - 1);
      syncWorldSelectScroll();
      updateWindowTitle();
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      worldSelectSelection = std::min(rowCount - 1, worldSelectSelection + 1);
      syncWorldSelectScroll();
      updateWindowTitle();
      uiDirty = true;
    }
    if (enterPressed && !menuEnterDown) {
      activateWorldSelectRow(worldSelectSelection);
    }
    if (mouseLeftClicked || mouseRightClicked) {
      if (hoveredRow >= 0) {
        worldSelectSelection = hoveredRow;
        syncWorldSelectScroll();
        updateWindowTitle();
        uiDirty = true;

        if (mouseRightClicked) {
          deleteWorldSelectRow(hoveredRow);
        } else {
          activateWorldSelectRow(hoveredRow);
        }
      }
    }
    if (escPressed && !menuEscDown) {
      setScreenState(ScreenState::kMainMenu);
    }
  } else if (screenState == ScreenState::kLoadingWorld) {
    // Loading is driven by synchronous world setup functions.
  } else if (screenState == ScreenState::kSettings) {
    syncSettingsState();
    float t = static_cast<float>(glfwGetTime());
    float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
    float settingsPanelW = std::min(uiW * 0.82f, 860.0f);
    float settingsPanelH = std::min(uiH * 0.80f, 560.0f);
    float settingsPanelX = (uiW - settingsPanelW) * 0.5f;
    float settingsPanelY = (uiH - settingsPanelH) * 0.5f;
    settingsPanelY += (1.0f - intro) * 28.0f;
    settingsPanelY += std::sin(t * 1.4f) * 3.0f;
    SettingsUiLayout settingsLayout = makeSettingsUiLayout(settingsPanelX,
                                                           settingsPanelY,
                                                           settingsPanelW,
                                                           settingsPanelH);
    int optionCount = settingsEntryCountForCategory(settingsCategory);

    int hoveredCategory = -1;
    for (int i = 0; i < kSettingsCategoryCount; ++i) {
      if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsCategoryRect(settingsLayout, i))) {
        hoveredCategory = i;
        break;
      }
    }

    int hoveredOption = -1;
    for (int i = 0; i < optionCount; ++i) {
      if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsOptionRect(settingsLayout, i))) {
        hoveredOption = i;
        break;
      }
    }

    int hoveredAction = -1;
    for (int i = 0; i < kSettingsActionCount; ++i) {
      if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsActionRect(settingsLayout, i))) {
        hoveredAction = i;
        break;
      }
    }

    auto setSettingsFocus = [&](SettingsFocusArea focus) {
      int nextFocus = static_cast<int>(focus);
      if (settingsFocusArea != nextFocus) {
        settingsFocusArea = nextFocus;
        uiDirty = true;
      }
    };

    auto handleSettingsOptionPointer = [&](int optionIndex, bool primaryClick, bool secondaryClick, bool primaryHold) {
      settingsOptionSelection = std::clamp(optionIndex, 0, std::max(0, optionCount - 1));
      setSettingsFocus(SettingsFocusArea::kOptions);

      SettingsEntryId entry = settingsEntryForCategory(settingsCategory, optionIndex);
      glm::vec4 optionRect = settingsOptionRect(settingsLayout, optionIndex);
      glm::vec4 controlRect = settingsControlRect(optionRect);

      switch (entry) {
        case SettingsEntryId::kGraphicsQuality: {
          if (!primaryClick && !secondaryClick) {
            return;
          }
          for (int i = 0; i < 3; ++i) {
            if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsSegmentRect(controlRect, 3, i))) {
              pendingSettings.graphicsQuality = i;
              updateSettingsDirtyFlag();
              uiDirty = true;
              return;
            }
          }
          adjustSettingsEntry(entry, primaryClick);
          return;
        }
        case SettingsEntryId::kRenderDistance:
        case SettingsEntryId::kAudioVolume:
        case SettingsEntryId::kSensitivity:
        case SettingsEntryId::kUiScale: {
          glm::vec4 sliderRect = settingsSliderTrackRect(optionRect);
          glm::vec4 sliderHitRect{sliderRect.x, optionRect.y + 12.0f, sliderRect.z, optionRect.w - 24.0f};
          if (primaryHold && pointInRect(menuMouseFb.x, menuMouseFb.y, sliderHitRect)) {
            setSettingsEntryFromNormalized(entry, normalizedValueFromRect(menuMouseFb.x, sliderRect));
            return;
          }
          if (secondaryClick) {
            adjustSettingsEntry(entry, false);
            return;
          }
          if (primaryClick) {
            if (pointInRect(menuMouseFb.x, menuMouseFb.y, sliderHitRect)) {
              setSettingsEntryFromNormalized(entry, normalizedValueFromRect(menuMouseFb.x, sliderRect));
            } else {
              adjustSettingsEntry(entry, menuMouseFb.x >= optionRect.x + optionRect.z * 0.5f);
            }
          }
          return;
        }
        case SettingsEntryId::kLanguage: {
          if (!primaryClick && !secondaryClick) {
            return;
          }
          if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsSegmentRect(controlRect, 2, 0))) {
            pendingSettings.language = 0;
          } else if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsSegmentRect(controlRect, 2, 1))) {
            pendingSettings.language = 1;
          } else if (primaryClick || secondaryClick) {
            pendingSettings.language = pendingSettings.language == 0 ? 1 : 0;
          }
          updateSettingsDirtyFlag();
          uiDirty = true;
          return;
        }
        case SettingsEntryId::kBlockGuides: {
          if (!primaryClick && !secondaryClick) {
            return;
          }
          if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsSegmentRect(controlRect, 2, 0))) {
            pendingSettings.blockGuides = false;
          } else if (pointInRect(menuMouseFb.x, menuMouseFb.y, settingsSegmentRect(controlRect, 2, 1))) {
            pendingSettings.blockGuides = true;
          } else {
            pendingSettings.blockGuides = !pendingSettings.blockGuides;
          }
          updateSettingsDirtyFlag();
          uiDirty = true;
          return;
        }
        default:
          return;
      }
    };

    if (mouseLeftClicked && hoveredCategory >= 0) {
      settingsCategory = hoveredCategory;
      settingsOptionSelection = 0;
      setSettingsFocus(SettingsFocusArea::kOptions);
      uiDirty = true;
    } else if (mouseLeftClicked && hoveredAction >= 0) {
      settingsActionSelection = hoveredAction;
      setSettingsFocus(SettingsFocusArea::kActions);
      activateSettingsAction(static_cast<SettingsActionId>(hoveredAction));
    } else if (hoveredOption >= 0 &&
               (mouseLeftClicked || mouseRightClicked || mouseLeftPressed)) {
      handleSettingsOptionPointer(hoveredOption, mouseLeftClicked, mouseRightClicked, mouseLeftPressed);
    }

    optionCount = settingsEntryCountForCategory(settingsCategory);
    settingsOptionSelection = std::clamp(settingsOptionSelection, 0, std::max(0, optionCount - 1));

    if (upPressed && !menuUpDown) {
      if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kCategories)) {
        settingsCategory = std::max(0, settingsCategory - 1);
        settingsOptionSelection = 0;
      } else if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kOptions)) {
        if (settingsOptionSelection > 0) {
          --settingsOptionSelection;
        } else {
          settingsFocusArea = static_cast<int>(SettingsFocusArea::kCategories);
        }
      } else {
        settingsFocusArea = static_cast<int>(SettingsFocusArea::kOptions);
      }
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kCategories)) {
        settingsCategory = std::min(kSettingsCategoryCount - 1, settingsCategory + 1);
        settingsOptionSelection = 0;
      } else if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kOptions)) {
        if (settingsOptionSelection + 1 < optionCount) {
          ++settingsOptionSelection;
        } else {
          settingsFocusArea = static_cast<int>(SettingsFocusArea::kActions);
        }
      } else {
        settingsFocusArea = static_cast<int>(SettingsFocusArea::kOptions);
      }
      uiDirty = true;
    }
    if (leftPressed && !menuLeftDown) {
      if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kOptions)) {
        adjustSettingsEntry(settingsEntryForCategory(settingsCategory, settingsOptionSelection), false);
      } else if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kActions)) {
        settingsActionSelection = std::max(0, settingsActionSelection - 1);
        uiDirty = true;
      }
    }
    if (rightPressed && !menuRightDown) {
      if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kCategories)) {
        settingsFocusArea = static_cast<int>(SettingsFocusArea::kOptions);
        uiDirty = true;
      } else if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kOptions)) {
        adjustSettingsEntry(settingsEntryForCategory(settingsCategory, settingsOptionSelection), true);
      } else {
        settingsActionSelection = std::min(kSettingsActionCount - 1, settingsActionSelection + 1);
        uiDirty = true;
      }
    }
    if (enterPressed && !menuEnterDown) {
      if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kCategories)) {
        settingsFocusArea = static_cast<int>(SettingsFocusArea::kOptions);
        uiDirty = true;
      } else if (settingsFocusArea == static_cast<int>(SettingsFocusArea::kOptions)) {
        adjustSettingsEntry(settingsEntryForCategory(settingsCategory, settingsOptionSelection), true);
      } else {
        activateSettingsAction(static_cast<SettingsActionId>(settingsActionSelection));
      }
    }
    if (escPressed && !menuEscDown) {
      pendingSettings = appliedSettings;
      settingsDirty = false;
      setScreenState(settingsReturnState);
    }
  } else if (screenState == ScreenState::kCreateWorld) {
    constexpr int kCreateFieldCount = 9;
    float t = static_cast<float>(glfwGetTime());
    float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
    float panelWidth = std::min(uiW * 0.72f, 640.0f);
    float panelHeight = std::min(uiH * 0.78f, 540.0f);
    float panelX = (uiW - panelWidth) * 0.5f;
    float panelY = (uiH - panelHeight) * 0.5f;
    panelY += (1.0f - intro) * 28.0f;
    panelY += std::sin(t * 1.4f) * 3.0f;

    float rowW = panelWidth - 120.0f;
    float rowX = panelX + (panelWidth - rowW) * 0.5f;
    float rowH = 36.0f;
    float rowGap = 12.0f;
    float rowsH = static_cast<float>(kCreateFieldCount) * rowH +
                  static_cast<float>(kCreateFieldCount - 1) * rowGap;
    float rowY = panelY + 72.0f + std::max(0.0f, (panelHeight - 92.0f - rowsH) * 0.5f);

    auto createRowRect = [&](int rowIndex) -> glm::vec4 {
      return {
        rowX,
        rowY + static_cast<float>(rowIndex) * (rowH + rowGap),
        rowW,
        rowH
      };
    };

    auto setCreateSelection = [&](int nextSelection) {
      nextSelection = std::clamp(nextSelection, 0, kCreateFieldCount - 1);
      if (createWorldSelection != nextSelection) {
        createWorldSelection = nextSelection;
        updateWindowTitle();
        uiDirty = true;
      }
    };

    auto setCreateSliderValue = [&](int rowIndex, float normalized) {
      normalized = std::clamp(normalized, 0.0f, 1.0f);
      float value = 0.25f + normalized * (2.5f - 0.25f);
      value = std::round(value * 4.0f) * 0.25f;
      value = std::clamp(value, 0.25f, 2.5f);
      bool changed = false;
      if (rowIndex == 3) {
        changed = std::abs(pendingWorldSettings.caveDensity - value) > 0.001f;
        pendingWorldSettings.caveDensity = value;
      } else if (rowIndex == 4) {
        changed = std::abs(pendingWorldSettings.ravineFrequency - value) > 0.001f;
        pendingWorldSettings.ravineFrequency = value;
      }
      if (changed) {
        updateWindowTitle();
        uiDirty = true;
      }
    };

    auto setCreatePreset = [&](WorldPreset preset) {
      if (pendingWorldSettings.preset != preset) {
        pendingWorldSettings.preset = preset;
        if (preset == WorldPreset::kAprilFools) {
          pendingWorldSettings.caveDensity = 1.75f;
          pendingWorldSettings.ravineFrequency = 1.60f;
        } else {
          pendingWorldSettings.caveDensity = 1.0f;
          pendingWorldSettings.ravineFrequency = 1.0f;
        }
        updateWindowTitle();
        uiDirty = true;
      }
    };

    auto setCreateGameMode = [&](uint8_t mode) {
      mode = static_cast<uint8_t>(std::clamp<int>(mode, 0, 1));
      if (pendingWorldSettings.startInventoryMode != mode) {
        pendingWorldSettings.startInventoryMode = mode;
        updateWindowTitle();
        uiDirty = true;
      }
    };

    auto setCreateCheatsEnabled = [&](bool enabled) {
      if (pendingWorldSettings.cheatsEnabled != enabled) {
        pendingWorldSettings.cheatsEnabled = enabled;
        updateWindowTitle();
        uiDirty = true;
      }
    };

    auto createRowAtMouse = [&]() -> int {
      if (menuMouseFb.x < rowX || menuMouseFb.x > rowX + rowW) {
        return -1;
      }
      for (int i = 0; i < kCreateFieldCount; ++i) {
        glm::vec4 rect = createRowRect(i);
        if (menuMouseFb.y >= rect.y && menuMouseFb.y <= rect.y + rect.w) {
          return i;
        }
      }
      return -1;
    };

    auto presetButtonRect = [&](size_t optionIndex) -> glm::vec4 {
      constexpr float kPresetOptionGap = 8.0f;
      float optionW = 82.0f;
      float optionH = rowH - 12.0f;
      float optionY = createRowRect(2).y + 6.0f;
      float totalPresetWidth =
        optionW * static_cast<float>(kCreateWorldPresetOptions.size()) +
        kPresetOptionGap * static_cast<float>(kCreateWorldPresetOptions.size() - 1);
      float firstOptionX = rowX + rowW - totalPresetWidth - 10.0f;
      float optionX = firstOptionX + static_cast<float>(optionIndex) * (optionW + kPresetOptionGap);
      return {optionX, optionY, optionW, optionH};
    };

    auto createSliderRect = [&](int rowIndex) -> glm::vec4 {
      float trackW = 240.0f;
      float trackH = 12.0f;
      float tx = rowX + rowW - trackW - 16.0f;
      float ty = createRowRect(rowIndex).y + (rowH - trackH) * 0.5f;
      return {tx, ty, trackW, trackH};
    };

    auto createSliderHitRect = [&](int rowIndex) -> glm::vec4 {
      glm::vec4 sliderRect = createSliderRect(rowIndex);
      return {sliderRect.x, createRowRect(rowIndex).y + 8.0f, sliderRect.z, rowH - 16.0f};
    };

    auto createModeToggleRect = [&]() -> glm::vec4 {
      float toggleW = 108.0f;
      float toggleH = rowH - 12.0f;
      float toggleX = rowX + rowW - toggleW - 16.0f;
      float toggleY = createRowRect(5).y + 6.0f;
      return {toggleX, toggleY, toggleW, toggleH};
    };

    auto createCheatsToggleRect = [&]() -> glm::vec4 {
      float toggleW = 108.0f;
      float toggleH = rowH - 12.0f;
      float toggleX = rowX + rowW - toggleW - 16.0f;
      float toggleY = createRowRect(6).y + 6.0f;
      return {toggleX, toggleY, toggleW, toggleH};
    };

    int hoveredCreateRow = createRowAtMouse();
    if (hoveredCreateRow >= 0) {
      setCreateSelection(hoveredCreateRow);
    }

    if (mouseLeftClicked || mouseRightClicked || mouseLeftPressed) {
      if (hoveredCreateRow >= 0) {
        setCreateSelection(hoveredCreateRow);

        if (hoveredCreateRow == 2 && mouseLeftClicked) {
          bool clickedPresetButton = false;
          for (size_t i = 0; i < kCreateWorldPresetOptions.size(); ++i) {
            if (!pointInRect(menuMouseFb.x, menuMouseFb.y, presetButtonRect(i))) {
              continue;
            }
            setCreatePreset(kCreateWorldPresetOptions[i]);
            clickedPresetButton = true;
            break;
          }
          if (!clickedPresetButton) {
            togglePreset();
          }
        } else if ((hoveredCreateRow == 3 || hoveredCreateRow == 4) &&
                   (mouseLeftClicked || mouseRightClicked || mouseLeftPressed)) {
          glm::vec4 sliderRect = createSliderRect(hoveredCreateRow);
          glm::vec4 sliderHitRect = createSliderHitRect(hoveredCreateRow);
          if (mouseLeftPressed && pointInRect(menuMouseFb.x, menuMouseFb.y, sliderHitRect)) {
            setCreateSliderValue(hoveredCreateRow, normalizedValueFromRect(menuMouseFb.x, sliderRect));
          } else if (mouseRightClicked) {
            adjustCreateSetting(false);
          } else if (mouseLeftClicked) {
            adjustCreateSetting(menuMouseFb.x >= sliderRect.x + sliderRect.z * 0.5f);
          }
        } else if (hoveredCreateRow == 5 && mouseLeftClicked) {
          if (pointInRect(menuMouseFb.x, menuMouseFb.y, createModeToggleRect())) {
            setCreateGameMode(pendingWorldSettings.startInventoryMode == 0 ? 1 : 0);
          } else {
            adjustCreateSetting(true);
          }
        } else if (hoveredCreateRow == 6 && mouseLeftClicked) {
          if (pointInRect(menuMouseFb.x, menuMouseFb.y, createCheatsToggleRect())) {
            setCreateCheatsEnabled(!pendingWorldSettings.cheatsEnabled);
          } else {
            adjustCreateSetting(true);
          }
        } else if (hoveredCreateRow == 7 && mouseLeftClicked) {
          createWorldFromMenu();
        } else if (hoveredCreateRow == 8 && mouseLeftClicked) {
          refreshWorldSelectEntries();
          setScreenState(ScreenState::kWorldSelect);
        }
      }
    }

    if (upPressed && !menuUpDown) {
      createWorldSelection = std::max(0, createWorldSelection - 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      createWorldSelection = std::min(kCreateFieldCount - 1, createWorldSelection + 1);
      updateWindowTitle();
      uiDirty = true;
    }
    if ((leftPressed && !menuLeftDown) || (rightPressed && !menuRightDown)) {
      adjustCreateSetting(rightPressed && !menuRightDown);
    }
    if (backspacePressed && !menuBackspaceDown) {
      if (createWorldSelection == 0 && !pendingWorldName.empty()) {
        popUtf8Back(pendingWorldName);
        updateWindowTitle();
      } else if (createWorldSelection == 1 && !pendingSeedText.empty()) {
        pendingSeedText.pop_back();
        updateWindowTitle();
      }
      uiDirty = true;
    }
    if (enterPressed && !menuEnterDown) {
      if (createWorldSelection >= 2 && createWorldSelection <= 6) {
        adjustCreateSetting(true);
      } else if (createWorldSelection == 7) {
        createWorldFromMenu();
      } else if (createWorldSelection == 8) {
        refreshWorldSelectEntries();
        setScreenState(ScreenState::kWorldSelect);
      }
    }
    if (escPressed && !menuEscDown) {
      refreshWorldSelectEntries();
      setScreenState(ScreenState::kWorldSelect);
    }
  }

  menuUpDown = upPressed;
  menuDownDown = downPressed;
  menuLeftDown = leftPressed;
  menuRightDown = rightPressed;
  menuEnterDown = enterPressed;
  menuBackspaceDown = backspacePressed;
  menuEscDown = escPressed;
}

void App::onCharInput(unsigned int codepoint) {
  if (screenState == ScreenState::kPlaying) {
    if (inventoryOpen || achievementTreeOpen) {
      return;
    }

    if (!commandInputOpen) {
      if (codepoint == '/') {
        if (!worldCheatsEnabled()) {
          showToast(appliedSettings.language == 1 ? "ЧИТЫ ВЫКЛЮЧЕНЫ" : "CHEATS DISABLED", 1.8f);
          return;
        }
        commandInputOpen = true;
        commandInput = "/";
        commandBackspaceDown = false;
        commandEnterDown = false;
        refreshCursorMode();
        uiDirty = true;
      }
      return;
    }

    if (codepoint >= 32 && codepoint <= 126 && commandInput.size() < kCommandBoxMaxChars) {
      commandInput.push_back(static_cast<char>(codepoint));
      uiDirty = true;
    }
    return;
  }

  if (screenState != ScreenState::kCreateWorld) {
    return;
  }

  if (createWorldSelection == 0) {
    if (isAllowedWorldNameCodepoint(codepoint) && utf8CodepointCount(pendingWorldName) < 24) {
      if (pendingWorldName == "World") {
        pendingWorldName.clear();
      }
      appendUtf8(pendingWorldName, codepoint);
      uiDirty = true;
      updateWindowTitle();
    }
    return;
  }

  if (createWorldSelection == 1) {
    if ((codepoint >= '0' && codepoint <= '9') && pendingSeedText.size() < 11) {
      pendingSeedText.push_back(static_cast<char>(codepoint));
      uiDirty = true;
      updateWindowTitle();
      return;
    }
    if (codepoint == '-' && pendingSeedText.empty()) {
      pendingSeedText.push_back('-');
      uiDirty = true;
      updateWindowTitle();
    }
  }
}

void App::loadSettings() {
  appliedSettings = UserSettings{};
  bool hasRenderDistance = false;
  std::filesystem::path settingsPath = cubeosSettingsPath();
  std::ifstream in(settingsPath);
  if (in.is_open()) {
    std::string line;
    while (std::getline(in, line)) {
      line = trimAscii(line);
      if (line.empty() || line[0] == '#') {
        continue;
      }

      size_t sep = line.find('=');
      if (sep == std::string::npos) {
        continue;
      }

      std::string key = trimAscii(line.substr(0, sep));
      std::string value = trimAscii(line.substr(sep + 1));
      if (key == "graphics_quality") {
        try {
          appliedSettings.graphicsQuality = std::stoi(value);
        } catch (...) {
        }
      } else if (key == "render_distance") {
        try {
          appliedSettings.renderDistance = std::stoi(value);
          hasRenderDistance = true;
        } catch (...) {
        }
      } else if (key == "sensitivity") {
        try {
          appliedSettings.sensitivity = std::stof(value);
        } catch (...) {
        }
      } else if (key == "audio_volume") {
        try {
          appliedSettings.audioVolume = std::stoi(value);
        } catch (...) {
        }
      } else if (key == "ui_scale") {
        try {
          appliedSettings.uiScale = clampUiScaleValue(std::stof(value));
        } catch (...) {
        }
      } else if (key == "language") {
        std::string lowered = value;
        for (char& c : lowered) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        appliedSettings.language = (lowered == "ru" || lowered == "russian" || lowered == "1") ? 1 : 0;
      } else if (key == "block_guides") {
        std::string lowered = value;
        for (char& c : lowered) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        appliedSettings.blockGuides =
          !(lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no");
      }
    }
  }

  if (!hasRenderDistance) {
    appliedSettings.renderDistance = defaultRenderDistanceForQuality(appliedSettings.graphicsQuality);
  }

  pendingSettings = appliedSettings;
  applySettings(false);
  settingsDirty = false;
}

bool App::saveSettings() const {
  std::filesystem::path settingsPath = cubeosSettingsPath();
  std::error_code ec;
  std::filesystem::create_directories(settingsPath.parent_path(), ec);
  std::ofstream out(settingsPath, std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }

  out << "graphics_quality=" << appliedSettings.graphicsQuality << "\n";
  out << "render_distance=" << appliedSettings.renderDistance << "\n";
  out << "sensitivity=" << appliedSettings.sensitivity << "\n";
  out << "audio_volume=" << appliedSettings.audioVolume << "\n";
  out << "ui_scale=" << appliedSettings.uiScale << "\n";
  out << "language=" << (appliedSettings.language == 1 ? "ru" : "en") << "\n";
  out << "block_guides=" << (appliedSettings.blockGuides ? "on" : "off") << "\n";
  return true;
}

void App::saveSettingsWithWarning(const char* reason) const {
  if (saveSettings()) {
    return;
  }
  std::cerr << "Warning: failed to save settings (" << reason << "): "
            << cubeosSettingsPath().string() << "\n";
}

void App::applySettings(bool refreshWorldStreaming) {
  appliedSettings.graphicsQuality = std::clamp(pendingSettings.graphicsQuality, 0, 2);
  appliedSettings.renderDistance = std::clamp(pendingSettings.renderDistance,
                                              kMinRenderDistance,
                                              kMaxRenderDistance);
  appliedSettings.sensitivity = std::clamp(pendingSettings.sensitivity, 0.03f, 0.40f);
  appliedSettings.audioVolume = std::clamp(pendingSettings.audioVolume, 0, 100);
  appliedSettings.uiScale = clampUiScaleValue(pendingSettings.uiScale);
  appliedSettings.language = std::clamp(pendingSettings.language, 0, 1);
  appliedSettings.blockGuides = pendingSettings.blockGuides;
  audio.setMasterVolume(appliedSettings.audioVolume);
  pendingSettings = appliedSettings;
  settingsDirty = false;

  activeChunkViewRadius = appliedSettings.renderDistance;
  loadedChunkViewRadius = std::min(loadedChunkViewRadius, activeChunkViewRadius);

  if (refreshWorldStreaming && screenState == ScreenState::kPlaying) {
    int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
    int radius = std::max(loadedChunkViewRadius, startupViewRadiusForQuality(appliedSettings.graphicsQuality,
                                                                             activeChunkViewRadius));
    loadedChunkViewRadius = std::min(radius, activeChunkViewRadius);
    world.updateActiveChunks(cx, cz, loadedChunkViewRadius);
    nextStreamingRadiusExpandTime = glfwGetTime() + streamingExpansionIntervalForQuality(appliedSettings.graphicsQuality);
    currentChunkX = cx;
    currentChunkZ = cz;
    chunkCenterValid = true;
  }

  if (window) {
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 uiPos = cursorToFramebuffer(xpos, ypos);
    cursorFbX = uiPos.x;
    cursorFbY = uiPos.y;
  }
  if (achievementTreeOpen) {
    AchievementTreeUiLayout layout = makeAchievementTreeLayout(uiLayoutWidth(), uiLayoutHeight());
    glm::vec2 clamped = clampAchievementTreeScroll(layout, {achievementTreeScrollX, achievementTreeScrollY});
    achievementTreeScrollX = clamped.x;
    achievementTreeScrollY = clamped.y;
  }

  updateWindowTitle();
  uiDirty = true;
}

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

  glfwGetWindowSize(window, &width, &height);
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
  glfwSetWindowUserPointer(window, this);
  glfwSetWindowSizeCallback(window, App::windowSizeCallback);
  glfwSetFramebufferSizeCallback(window, App::framebufferResizeCallback);
  glfwSetScrollCallback(window, App::scrollCallback);
  glfwSetCursorPosCallback(window, App::mouseCallback);
  glfwSetCharCallback(window, App::charCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  preparePersistentStorage();
  loadSettings();
  if (!audio.init()) {
    std::cerr << "Warning: failed to initialize audio.\n";
  }
  audio.setMasterVolume(appliedSettings.audioVolume);
  menuIntro = 0.0f;
  updateWindowTitle();
}

void App::initVulkan() {
  WorldGenSettings startupSettings{};
  startupSettings.preset = WorldPreset::kMinecraftStyle;
  startupSettings.generateStructures = true;
  startupSettings.caveDensity = 1.0f;
  startupSettings.ravineFrequency = 1.0f;
  startupSettings.startInventoryMode = 0;
  startupSettings.cheatsEnabled = false;

  world.setGenerationSettings(startupSettings);
  world.setSeed(1337);
  world.generate();
  constexpr int kMenuPreviewChunkRadius = 1;
  world.updateActiveChunks(0, 0, kMenuPreviewChunkRadius);
  world.waitForChunkRegion(0, 0, kMenuPreviewChunkRadius, 1800);

  pendingWorldSettings = startupSettings;
  pendingWorldName = "World";
  pendingSeedText.clear();
  currentWorldPath.clear();

  rebuildUiMesh();
  composeMeshData();
  uiDirty = false;
  refreshSelectedBlock();

  vk.setMeshData(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
  vk.init(window, &framebufferResized);
  vkReady = true;

  std::vector<ChunkMeshUpload> worldSnapshot;
  world.snapshotChunkMeshes(worldSnapshot);
  vk.setWorldChunkMeshes(toVkChunkUploads(std::move(worldSnapshot)));
  pendingWorldChunkUploads.clear();
  pendingWorldChunkRemovals.clear();
  lastWorldChunkUploadTime = glfwGetTime();
  dayLightFactor = computeDaylightFactor();
  vk.setEnvironmentState(dayLightFactor,
                         weatherIntensity,
                         dayCycleTime,
                         isAprilFoolsPreset(world.getGenerationSettings()));
}

void App::mainLoop() {
  constexpr double kWorldChunkUploadMinIntervalSec = 1.0 / 60.0; // Keep world pop-in lower without uploading every frame.
  constexpr int kMeshRebuildTaskBudgetPerTick = 18;
  constexpr int kMeshUploadBudgetPerTick = 28;
  constexpr float kWaterSimTickFocusedSec = 0.14f;
  constexpr float kWaterSimTickUnfocusedSec = 0.24f;
  constexpr double kMenuUiTickFocusedSec = 1.0 / 24.0;
  constexpr double kMenuUiTickUnfocusedSec = 1.0 / 12.0;
  double lastTime = glfwGetTime();
  double menuUiTickAccumulator = 0.0;
  while (!glfwWindowShouldClose(window)) {
    double frameStart = glfwGetTime();
    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - lastTime);
    deltaTime = std::clamp(deltaTime, 0.0f, 0.12f);
    lastTime = currentTime;
    profiler.frameCount += 1;
    size_t chunkUpdatesThisFrame = 0;
    size_t chunkRemovalsThisFrame = 0;
    float positiveDelta = std::max(0.0f, deltaTime);
    fpsSampleAccum += positiveDelta;
    fpsSampleFrames += 1;
    if (fpsSampleAccum >= 0.25f && fpsSampleFrames > 0) {
      int newFps = static_cast<int>(std::lround(static_cast<double>(fpsSampleFrames) /
                                                std::max(0.001, static_cast<double>(fpsSampleAccum))));
      if (newFps != fpsDisplayValue) {
        fpsDisplayValue = newFps;
        uiDirty = true;
      }
      fpsSampleAccum = 0.0f;
      fpsSampleFrames = 0;
    }

    double eventsInputStart = glfwGetTime();
    glfwPollEvents();
    bool windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    bool windowMinimized = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE ||
                           width <= 0 || height <= 0;
    if (windowMinimized) {
      glfwWaitEventsTimeout(0.08);
      lastTime = glfwGetTime();
      continue;
    }

    processInput(deltaTime);
    recordProfilerMetric(profiler.eventsInput,
                         (glfwGetTime() - eventsInputStart) * 1000.0,
                         profiler.frameCount);

    double environmentStart = glfwGetTime();
    updateEnvironment(deltaTime);
    recordProfilerMetric(profiler.environment,
                         (glfwGetTime() - environmentStart) * 1000.0,
                         profiler.frameCount);

    if (selectedItemToastTimer > 0.0f) {
      selectedItemToastTimer = std::max(0.0f, selectedItemToastTimer - deltaTime);
      uiDirty = true;
    }
    if (craftResultFlashTimer > 0.0f) {
      craftResultFlashTimer = std::max(0.0f, craftResultFlashTimer - deltaTime);
      uiDirty = true;
    }
    if (achievementPopupVisible) {
      achievementPopupTimer = std::max(0.0f, achievementPopupTimer - deltaTime);
      uiDirty = true;
      if (achievementPopupTimer <= 0.0f) {
        achievementPopupVisible = false;
      }
    }
    if (!achievementPopupVisible && !achievementPopupQueue.empty()) {
      activeAchievementPopupId = achievementPopupQueue.front();
      achievementPopupQueue.pop_front();
      achievementPopupVisible = true;
      achievementPopupTimer = kAchievementPopupDuration;
      uiDirty = true;
    }
    if (debugWorldgenOverlay && (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused)) {
      uiDirty = true;
    }
    if (debugProfilerOverlay && (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused)) {
      uiDirty = true;
    }
    if (commandInputOpen && screenState == ScreenState::kPlaying) {
      uiDirty = true;
    }

    if (screenState != ScreenState::kPlaying) {
      menuUiTickAccumulator += static_cast<double>(deltaTime);
      double menuUiTick = windowFocused ? kMenuUiTickFocusedSec : kMenuUiTickUnfocusedSec;
      bool shouldTickMenuUi = menuUiTickAccumulator >= menuUiTick || menuIntro < 1.0f;
      if (shouldTickMenuUi) {
        if (menuIntro < 1.0f) {
          menuIntro = std::min(1.0f, menuIntro + deltaTime * 3.2f);
        }
        menuUiTickAccumulator = 0.0;
        // Menus have ambient animation, but throttled to reduce CPU load.
        uiDirty = true;
      }
    } else {
      menuUiTickAccumulator = 0.0;
    }
    double playerStart = glfwGetTime();
    if (screenState == ScreenState::kPlaying && !inventoryOpen && !achievementTreeOpen && !commandInputOpen) {
      updatePlayer(deltaTime);
    }
    recordProfilerMetric(profiler.player,
                         (glfwGetTime() - playerStart) * 1000.0,
                         profiler.frameCount);

    double streamingStart = glfwGetTime();
    if (screenState == ScreenState::kPlaying) {
      updateStreaming();
    }
    recordProfilerMetric(profiler.streaming,
                         (glfwGetTime() - streamingStart) * 1000.0,
                         profiler.frameCount);

    double simulationStart = glfwGetTime();
    if (screenState == ScreenState::kPlaying) {
      waterSimBoostTimer = std::max(0.0f, waterSimBoostTimer - deltaTime);
      waterSimAccumulator += deltaTime;
      float waterTickSec = windowFocused ? kWaterSimTickFocusedSec : kWaterSimTickUnfocusedSec;
      if (waterSimAccumulator > waterTickSec * 3.0f) {
        waterSimAccumulator = waterTickSec * 3.0f;
      }
      int px = static_cast<int>(std::floor(playerPos.x));
      int pz = static_cast<int>(std::floor(playerPos.z));
      bool nearWater = intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.65f, 0.0f));
      bool highActivity = waterSimBoostTimer > 0.0f;
      while (waterSimAccumulator >= waterTickSec) {
        // Keep background water work very small to avoid frame drops in ocean areas.
        if (highActivity || nearWater) {
          int simRadius = highActivity ? 24 : 16;
          int waterUpdates = highActivity ? 96 : 28;
          int fallingUpdates = highActivity ? 34 : 10;
          if (!windowFocused) {
            simRadius = std::max(8, simRadius - 4);
            waterUpdates = std::max(4, static_cast<int>(std::lround(waterUpdates * 0.65f)));
            fallingUpdates = std::max(4, static_cast<int>(std::lround(fallingUpdates * 0.65f)));
          }
          world.simulateWater(px, pz, simRadius, waterUpdates);
          world.simulateFallingBlocks(px, pz, simRadius, fallingUpdates);
        }

        waterSimAccumulator -= waterTickSec;
      }
    } else {
      waterSimBoostTimer = 0.0f;
      waterSimAccumulator = 0.0f;
    }
    recordProfilerMetric(profiler.simulation,
                         (glfwGetTime() - simulationStart) * 1000.0,
                         profiler.frameCount);

    double gameplayStart = glfwGetTime();
    if (screenState == ScreenState::kPlaying) {
      updateFurnaces(deltaTime);
      updateDroppedItems(deltaTime);
      updateSheep(deltaTime);
    }
    syncAudioState();
    syncDroppedItemMesh(false);
    syncSheepMesh(false);
    updateFirstPersonState(deltaTime);
    syncFirstPersonMesh();
    updateInteractionOverlayMesh();
    recordProfilerMetric(profiler.gameplay,
                         (glfwGetTime() - gameplayStart) * 1000.0,
                         profiler.frameCount);

    double meshStart = glfwGetTime();
    bool worldScreenActive = screenState == ScreenState::kPlaying ||
                             screenState == ScreenState::kPaused ||
                             screenState == ScreenState::kLoadingWorld;
    if (worldScreenActive) {
      bool hasWorldMeshWork = world.consumeMeshDirty();
      if (hasWorldMeshWork) {
        int meshBuildBudget = kMeshRebuildTaskBudgetPerTick;
        int meshUploadBudget = kMeshUploadBudgetPerTick;
        if (!windowFocused) {
          meshBuildBudget = std::max(2, meshBuildBudget / 2);
          meshUploadBudget = std::max(4, meshUploadBudget / 2);
        }

        std::vector<ChunkMeshUpload> chunkUpdates;
        std::vector<uint64_t> removedChunkKeys;
        bool hasChunkUpdateBatch = world.consumeChunkMeshUpdates(
          chunkUpdates,
          removedChunkKeys,
          meshBuildBudget,
          meshUploadBudget);
        if (hasChunkUpdateBatch) {
          chunkUpdatesThisFrame += chunkUpdates.size();
          chunkRemovalsThisFrame += removedChunkKeys.size();
          for (uint64_t key : removedChunkKeys) {
            pendingWorldChunkUploads.erase(key);
            pendingWorldChunkRemovals.insert(key);
          }

          std::vector<VulkanContext::WorldChunkMeshUpload> vkUpdates =
            toVkChunkUploads(std::move(chunkUpdates));
          for (VulkanContext::WorldChunkMeshUpload& update : vkUpdates) {
            pendingWorldChunkRemovals.erase(update.key);
            pendingWorldChunkUploads[update.key] = std::move(update);
          }
        }
      }
    }

    bool shouldFlushWorldChunks = !pendingWorldChunkUploads.empty() || !pendingWorldChunkRemovals.empty();
    if (shouldFlushWorldChunks &&
        (currentTime - lastWorldChunkUploadTime) >= kWorldChunkUploadMinIntervalSec) {
      std::vector<uint64_t> removals;
      removals.reserve(pendingWorldChunkRemovals.size());
      for (uint64_t key : pendingWorldChunkRemovals) {
        removals.push_back(key);
      }
      pendingWorldChunkRemovals.clear();

      std::vector<VulkanContext::WorldChunkMeshUpload> uploads;
      uploads.reserve(pendingWorldChunkUploads.size());
      for (auto& [key, upload] : pendingWorldChunkUploads) {
        (void)key;
        uploads.push_back(std::move(upload));
      }
      pendingWorldChunkUploads.clear();

      vk.updateWorldChunkMeshes(uploads, removals);
      lastWorldChunkUploadTime = currentTime;
    }
    recordProfilerMetric(profiler.mesh,
                         (glfwGetTime() - meshStart) * 1000.0,
                         profiler.frameCount);

    double uiStart = glfwGetTime();
    if (uiDirty) {
      rebuildUiMesh();
      composeMeshData();
      vk.updateMesh(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
      uiDirty = false;
    }
    recordProfilerMetric(profiler.ui,
                         (glfwGetTime() - uiStart) * 1000.0,
                         profiler.frameCount);

    glm::vec3 eye;
    glm::vec3 front;
    bool cameraInWater = false;
    if (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused) {
      eye = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
      front = cameraFront();
      cameraInWater = isWaterVolumeBlock(world.getBlock(static_cast<int>(std::floor(eye.x)),
                                                        static_cast<int>(std::floor(eye.y)),
                                                        static_cast<int>(std::floor(eye.z))));
    } else {
      eye = glm::vec3(6.0f, 62.0f, -18.0f);
      front = glm::normalize(glm::vec3(0.2f, -0.16f, 1.0f));
    }

    glm::mat4 view = glm::lookAt(eye, eye + front, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(70.0f),
                                      width / static_cast<float>(height),
                                      0.1f,
                                      200.0f);
    proj[1][1] *= -1.0f;
    glm::mat4 firstPersonView(1.0f);
    glm::mat4 firstPersonProj = glm::perspective(glm::radians(kFirstPersonProjectionFovDeg),
                                                 width / static_cast<float>(height),
                                                 0.01f,
                                                 12.0f);
    firstPersonProj[1][1] *= -1.0f;

    double torchStart = glfwGetTime();
    vk.setEnvironmentState(dayLightFactor,
                           weatherIntensity,
                           dayCycleTime,
                           isAprilFoolsPreset(world.getGenerationSettings()));
    if (worldScreenActive) {
      int quality = std::clamp(appliedSettings.graphicsQuality, 0, 2);
      size_t maxLights = static_cast<size_t>(maxTorchLightsForQuality(quality));
      float refreshInterval = torchLightRefreshIntervalForQuality(quality);
      glm::vec3 eyeDelta = eye - torchLightSampleEye;
      float movedSq = glm::dot(eyeDelta, eyeDelta);
      bool movedEnough = movedSq >= (kTorchLightRefreshMoveThreshold * kTorchLightRefreshMoveThreshold);
      bool turnedEnough = glm::dot(front, torchLightSampleForward) <= kTorchLightRefreshForwardDot;
      bool selectedChanged = torchLightSampleSelectedBlock != selectedBlock;
      torchLightRefreshTimer = std::max(0.0f, torchLightRefreshTimer - deltaTime);
      if (!torchLightsCacheValid || movedEnough || turnedEnough || selectedChanged || torchLightRefreshTimer <= 0.0f) {
        cachedTorchLights = collectTorchLights(maxLights);
        torchLightSampleEye = eye;
        torchLightSampleForward = front;
        torchLightSampleSelectedBlock = selectedBlock;
        torchLightRefreshTimer = refreshInterval;
        torchLightsCacheValid = true;
      } else if (cachedTorchLights.size() > maxLights) {
        cachedTorchLights.resize(maxLights);
      }
      vk.setTorchLights(cachedTorchLights);
    } else {
      cachedTorchLights.clear();
      torchLightRefreshTimer = 0.0f;
      torchLightsCacheValid = false;
      vk.setTorchLights({});
    }
    recordProfilerMetric(profiler.torch,
                         (glfwGetTime() - torchStart) * 1000.0,
                         profiler.frameCount);

    double drawStart = glfwGetTime();
    vk.setCameraWorldState(eye, front, cameraInWater);
    vk.setCameraMatrices(view, proj);
    vk.setFirstPersonMatrices(firstPersonView, firstPersonProj);
    vk.drawFrame();
    recordProfilerMetric(profiler.draw,
                         (glfwGetTime() - drawStart) * 1000.0,
                         profiler.frameCount);

    double frameDuration = glfwGetTime() - frameStart;
    bool playingState = screenState == ScreenState::kPlaying;
    int quality = std::clamp(appliedSettings.graphicsQuality, 0, 2);
    double targetFps = playingState
      ? (windowFocused ? focusedPlayingFpsForQuality(quality) : unfocusedPlayingFpsForQuality(quality))
      : (windowFocused ? focusedMenuFpsForQuality(quality) : unfocusedMenuFpsForQuality(quality));
    double frameBudget = 1.0 / targetFps;
    double sleepMs = 0.0;
    if (frameDuration < frameBudget) {
      double sleepStart = glfwGetTime();
      auto sleepTime = std::chrono::duration<double>(frameBudget - frameDuration);
      std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(sleepTime));
      sleepMs = (glfwGetTime() - sleepStart) * 1000.0;
    }
    recordProfilerMetric(profiler.sleep, sleepMs, profiler.frameCount);
    profiler.targetFps = targetFps;
    profiler.targetFrameMs = frameBudget * 1000.0;
    profiler.torchLights = cachedTorchLights.size();
    profiler.chunkUpdates = chunkUpdatesThisFrame;
    profiler.chunkRemovals = chunkRemovalsThisFrame;
    profiler.pendingGpuUploads = pendingWorldChunkUploads.size();
    profiler.pendingGpuRemovals = pendingWorldChunkRemovals.size();
    recordProfilerMetric(profiler.frame,
                         (glfwGetTime() - frameStart) * 1000.0,
                         profiler.frameCount);
  }

  vk.waitIdle();
}

float App::computeDaylightFactor() const {
  float cycle = dayCycleTime - std::floor(dayCycleTime);
  float sun = std::sin((cycle - 0.25f) * kTau);
  float daylight = std::clamp((sun + 0.30f) / 0.56f, 0.0f, 1.0f);
  daylight = daylight * daylight * (3.0f - 2.0f * daylight);
  return glm::mix(0.05f, 1.0f, daylight);
}

void App::resetEnvironmentForSession() {
  weatherRngState = static_cast<uint32_t>(world.getSeed()) ^ 0x9E3779B9u;
  if (weatherRngState == 0u) {
    weatherRngState = 0xA341316Cu;
  }

  float seedOffset = nextUnitRandom(weatherRngState);
  dayCycleTime = std::fmod(0.19f + seedOffset * 0.55f, 1.0f);
  weatherIntensity = 0.0f;
  weatherTargetIntensity = 0.0f;
  weatherDecisionTimer = 16.0f + 30.0f * nextUnitRandom(weatherRngState);
  dayLightFactor = computeDaylightFactor();
}

void App::updateEnvironment(float deltaTime) {
  float dt = std::max(0.0f, deltaTime);

  if (screenState == ScreenState::kPlaying) {
    dayCycleTime += dt / kDayNightCycleDurationSec;
    dayCycleTime -= std::floor(dayCycleTime);

    weatherDecisionTimer -= dt;
    if (weatherDecisionTimer <= 0.0f) {
      uint8_t biomeId = 0;
      BiomeClimateSample climate{};
      int wx = static_cast<int>(std::floor(playerPos.x));
      int wz = static_cast<int>(std::floor(playerPos.z));
      world.sampleBiomeClimateAt(wx, wz, biomeId, climate);
      (void)biomeId;

      float humidityFactor = std::clamp((climate.humidity - 0.15f) * 1.25f, 0.0f, 1.0f);
      float rainChance = 0.16f + humidityFactor * 0.58f;
      float decision = nextUnitRandom(weatherRngState);
      if (decision < rainChance) {
        float strength = 0.28f + 0.72f * nextUnitRandom(weatherRngState);
        weatherTargetIntensity = std::clamp(strength * (0.55f + humidityFactor * 0.60f), 0.20f, 1.0f);
      } else {
        weatherTargetIntensity = 0.0f;
      }

      float intervalRand = nextUnitRandom(weatherRngState);
      weatherDecisionTimer += kWeatherMinDecisionSec +
                              (kWeatherMaxDecisionSec - kWeatherMinDecisionSec) * intervalRand;
    }

    float settleSpeed = (weatherTargetIntensity > weatherIntensity) ? 0.11f : 0.05f;
    weatherIntensity = moveToward(weatherIntensity, weatherTargetIntensity, dt * settleSpeed);
  }

  dayLightFactor = computeDaylightFactor();
}

void App::updateFurnaces(float deltaTime) {
  if (deltaTime <= 0.0f || furnaceStates.empty()) {
    return;
  }

  bool activeChanged = false;
  std::string activeKey = furnaceOpen ? furnaceKeyForBlock(activeFurnaceBlock) : std::string();

  for (auto& [key, furnace] : furnaceStates) {
    float prevBurnTime = furnace.burnTime;
    float prevBurnDuration = furnace.burnDuration;
    float prevSmelt = furnace.smeltProgress;
    ItemStack prevInput = furnace.input;
    ItemStack prevFuel = furnace.fuel;
    ItemStack prevOutput = furnace.output;

    if (furnace.burnTime > 0.0f) {
      furnace.burnTime = std::max(0.0f, furnace.burnTime - deltaTime);
    }

    uint8_t resultType = kAir;
    bool hasRecipe = furnace.input.count > 0 &&
                     furnaceResultForInput(furnace.input.type, resultType);
    bool outputCompatible = hasRecipe &&
                            (furnace.output.count == 0 ||
                             (furnace.output.type == resultType && furnace.output.count < kMaxStack));
    bool canSmelt = hasRecipe && outputCompatible;

    if (canSmelt && furnace.burnTime <= 0.0f && furnace.fuel.count > 0) {
      float fuelTime = furnaceFuelDuration(furnace.fuel.type);
      if (fuelTime > 0.0f) {
        furnace.burnTime = fuelTime;
        furnace.burnDuration = fuelTime;
        furnace.fuel.count = static_cast<uint16_t>(furnace.fuel.count - 1);
        if (furnace.fuel.count == 0) {
          furnace.fuel.type = kAir;
        }
      }
    }

    if (canSmelt && furnace.burnTime > 0.0f) {
      furnace.smeltProgress += deltaTime;
      if (furnace.smeltProgress >= kFurnaceSmeltDuration) {
        furnace.smeltProgress -= kFurnaceSmeltDuration;
        furnace.input.count = static_cast<uint16_t>(furnace.input.count - 1);
        if (furnace.input.count == 0) {
          furnace.input.type = kAir;
        }
        if (furnace.output.count == 0 || furnace.output.type == kAir) {
          furnace.output.type = resultType;
          furnace.output.count = 1;
        } else if (furnace.output.type == resultType && furnace.output.count < kMaxStack) {
          furnace.output.count = static_cast<uint16_t>(furnace.output.count + 1);
        }
      }
    } else if (!canSmelt) {
      furnace.smeltProgress = 0.0f;
    }

    if (!activeKey.empty() && key == activeKey &&
        (prevBurnTime != furnace.burnTime ||
         prevBurnDuration != furnace.burnDuration ||
         prevSmelt != furnace.smeltProgress ||
         prevInput.type != furnace.input.type ||
         prevInput.count != furnace.input.count ||
         prevFuel.type != furnace.fuel.type ||
         prevFuel.count != furnace.fuel.count ||
         prevOutput.type != furnace.output.type ||
         prevOutput.count != furnace.output.count)) {
      activeChanged = true;
    }
  }

  if (furnaceOpen && activeChanged) {
    uiDirty = true;
  }
}

void App::syncAudioState() {
  bool playFurnaceLoop = false;
  if (screenState == ScreenState::kPlaying && inventoryOpen && furnaceOpen && hasFurnaceAccess()) {
    auto it = furnaceStates.find(furnaceKeyForBlock(activeFurnaceBlock));
    playFurnaceLoop = it != furnaceStates.end() && it->second.burnTime > 0.0f;
  }

  audio.setLoopCueActive(AudioSystem::Cue::kFurnaceLoop, playFurnaceLoop);
}

void App::processInput(float deltaTime) {
  if (screenState != ScreenState::kPlaying) {
    processMenuInput(deltaTime);
    crouching = false;
    sprinting = false;
    escDown = false;
    tabDown = false;
    jumpDown = false;
    achievementToggleDown = false;
    commandInputOpen = false;
    commandInput.clear();
    commandBackspaceDown = false;
    commandEnterDown = false;

    bool allowDebugOverlayKeys = screenState == ScreenState::kPaused;
    if (allowDebugOverlayKeys) {
      bool debugTogglePressed = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
      if (debugTogglePressed && !debugToggleDown) {
        debugWorldgenOverlay = !debugWorldgenOverlay;
        uiDirty = true;
      }
      debugToggleDown = debugTogglePressed;

      bool debugModePressed = glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
      if (debugModePressed && !debugModeDown) {
        debugWorldgenOverlayMode = (debugWorldgenOverlayMode + 1) % 3;
        debugWorldgenOverlay = true;
        uiDirty = true;
      }
      debugModeDown = debugModePressed;

      bool sliceUpPressed = glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
      if (sliceUpPressed && !debugSliceUpDown) {
        debugDensitySliceOffset = std::clamp(debugDensitySliceOffset + 4, -48, 48);
        debugWorldgenOverlay = true;
        uiDirty = true;
      }
      debugSliceUpDown = sliceUpPressed;

      bool sliceDownPressed = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS ||
                              glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
      if (sliceDownPressed && !debugSliceDownDown) {
        debugDensitySliceOffset = std::clamp(debugDensitySliceOffset - 4, -48, 48);
        debugWorldgenOverlay = true;
        uiDirty = true;
      }
      debugSliceDownDown = sliceDownPressed;

      bool profilerTogglePressed = glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS;
      if (profilerTogglePressed && !debugProfilerToggleDown) {
        debugProfilerOverlay = !debugProfilerOverlay;
        uiDirty = true;
      }
      debugProfilerToggleDown = profilerTogglePressed;
    } else {
      debugToggleDown = false;
      debugModeDown = false;
      debugSliceUpDown = false;
      debugSliceDownDown = false;
      debugProfilerToggleDown = false;
    }
    mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouseRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    return;
  }

  flightToggleTimer = std::max(0.0f, flightToggleTimer - deltaTime);

  bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  bool dropPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
  bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

  if (commandInputOpen) {
    bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escPressed && !escDown) {
      commandInputOpen = false;
      commandInput.clear();
      refreshCursorMode();
      uiDirty = true;
      escDown = true;
    } else if (!escPressed) {
      escDown = false;
    }

    bool backspacePressed = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
    if (backspacePressed && !commandBackspaceDown && !commandInput.empty()) {
      if (commandInput.size() <= 1) {
        commandInput = "/";
      } else {
        commandInput.pop_back();
      }
      uiDirty = true;
    }
    commandBackspaceDown = backspacePressed;

    bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    if (enterPressed && !commandEnterDown) {
      std::string submitted = commandInput;
      commandInputOpen = false;
      commandInput.clear();
      refreshCursorMode();
      executeCommand(submitted);
      uiDirty = true;
    }
    commandEnterDown = enterPressed;

    mouseLeftDown = leftPressed;
    mouseRightDown = rightPressed;
    dropOneDown = dropPressed;
    jumpDown = spacePressed;
    jumpDown = spacePressed;
    crouching = false;
    sprinting = false;
    if (breakingActive) {
      breakingActive = false;
      breakingProgress = 0.0f;
      breakingStage = 0;
      world.clearBreakOverlay();
    }
    return;
  }

  bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (escPressed && !escDown) {
    if (inventoryOpen) {
      setInventoryOpen(false);
    } else if (achievementTreeOpen) {
      setAchievementTreeOpen(false);
    } else {
      pauseMenuSelection = 0;
      setScreenState(ScreenState::kPaused);
      // Consume held Escape so pause menu does not instantly unpause.
      menuEscDown = true;
    }
    escDown = true;
  } else if (!escPressed) {
    escDown = false;
  }

  bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
  if (tabPressed && !tabDown) {
    if (!inventoryOpen) {
      workbenchOpen = false;
      furnaceOpen = false;
    }
    setInventoryOpen(!inventoryOpen);
    tabDown = true;
  } else if (!tabPressed) {
    tabDown = false;
  }

  if (inventoryOpen && workbenchOpen && !hasWorkbenchAccess()) {
    setInventoryOpen(false);
    showToast(appliedSettings.language == 1 ? "ВЕРСТАК СЛИШКОМ ДАЛЕКО" : "WORKBENCH OUT OF RANGE", 1.8f);
    mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouseRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    return;
  }
  if (inventoryOpen && furnaceOpen && !hasFurnaceAccess()) {
    setInventoryOpen(false);
    showToast(appliedSettings.language == 1 ? "ПЕЧКА СЛИШКОМ ДАЛЕКО" : "FURNACE OUT OF RANGE", 1.8f);
    mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouseRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    return;
  }

  bool achievementPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
  if (achievementPressed && !achievementToggleDown) {
    setAchievementTreeOpen(!achievementTreeOpen);
  }
  achievementToggleDown = achievementPressed;

  bool debugTogglePressed = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
  if (debugTogglePressed && !debugToggleDown) {
    debugWorldgenOverlay = !debugWorldgenOverlay;
    uiDirty = true;
  }
  debugToggleDown = debugTogglePressed;

  bool debugModePressed = glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
  if (debugModePressed && !debugModeDown) {
    debugWorldgenOverlayMode = (debugWorldgenOverlayMode + 1) % 3;
    debugWorldgenOverlay = true;
    uiDirty = true;
  }
  debugModeDown = debugModePressed;

  bool sliceUpPressed = glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
  if (sliceUpPressed && !debugSliceUpDown) {
    debugDensitySliceOffset = std::clamp(debugDensitySliceOffset + 4, -48, 48);
    debugWorldgenOverlay = true;
    uiDirty = true;
  }
  debugSliceUpDown = sliceUpPressed;

  bool sliceDownPressed = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
  if (sliceDownPressed && !debugSliceDownDown) {
    debugDensitySliceOffset = std::clamp(debugDensitySliceOffset - 4, -48, 48);
    debugWorldgenOverlay = true;
    uiDirty = true;
  }
  debugSliceDownDown = sliceDownPressed;

  bool profilerTogglePressed = glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS;
  if (profilerTogglePressed && !debugProfilerToggleDown) {
    debugProfilerOverlay = !debugProfilerOverlay;
    uiDirty = true;
  }
  debugProfilerToggleDown = profilerTogglePressed;

  int prevSlot = selectedSlot;
  for (int i = 0; i < static_cast<int>(hotbar.size()); ++i) {
    if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
      selectedSlot = i;
    }
  }
  if (selectedSlot != prevSlot) {
    refreshSelectedBlock();
    showSelectedItemToast();
  }

  if (inventoryOpen) {
    crouching = false;
    sprinting = false;
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
    dropOneDown = dropPressed;
    jumpDown = spacePressed;

    if (breakingActive) {
      breakingActive = false;
      breakingProgress = 0.0f;
      breakingStage = 0;
      world.clearBreakOverlay();
    }

    (void)deltaTime;
    return;
  }

  if (achievementTreeOpen) {
    crouching = false;
    sprinting = false;
    AchievementTreeUiLayout layout = makeAchievementTreeLayout(uiLayoutWidth(), uiLayoutHeight());
    glm::vec2 scroll =
      clampAchievementTreeScroll(layout, {achievementTreeScrollX, achievementTreeScrollY});

    bool panLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool panRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool panUp = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    bool panDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;

    float panStep = 420.0f * deltaTime;
    if (panLeft) {
      scroll.x -= panStep;
    }
    if (panRight) {
      scroll.x += panStep;
    }
    if (panUp) {
      scroll.y -= panStep;
    }
    if (panDown) {
      scroll.y += panStep;
    }

    auto pointInRect = [](float px, float py, const glm::vec4& rect) {
      return px >= rect.x && px <= rect.x + rect.z &&
             py >= rect.y && py <= rect.y + rect.w;
    };

    bool insideViewport =
      cursorFbX >= layout.viewportX && cursorFbX <= layout.viewportX + layout.viewportW &&
      cursorFbY >= layout.viewportY && cursorFbY <= layout.viewportY + layout.viewportH;

    if (leftPressed && !mouseLeftDown) {
      achievementTreeDragging = false;
      for (int tab = 0; tab < 3; ++tab) {
        glm::vec4 rect = achievementTreeTabRect(layout, tab);
        if (pointInRect(cursorFbX, cursorFbY, rect)) {
          glm::vec2 target = achievementTreeTabTargetScroll(layout, tab);
          scroll = clampAchievementTreeScroll(layout, target);
          uiDirty = true;
          break;
        }
      }

      if (!achievementTreeDragging && insideViewport) {
        achievementTreeDragging = true;
        achievementTreeDragLastX = cursorFbX;
        achievementTreeDragLastY = cursorFbY;
      }
    } else if (!leftPressed) {
      achievementTreeDragging = false;
    }

    if (achievementTreeDragging && leftPressed) {
      float dx = cursorFbX - achievementTreeDragLastX;
      float dy = cursorFbY - achievementTreeDragLastY;
      achievementTreeDragLastX = cursorFbX;
      achievementTreeDragLastY = cursorFbY;
      scroll.x -= dx;
      scroll.y -= dy;
    }

    scroll = clampAchievementTreeScroll(layout, scroll);
    if (std::abs(scroll.x - achievementTreeScrollX) > 0.01f ||
        std::abs(scroll.y - achievementTreeScrollY) > 0.01f) {
      achievementTreeScrollX = scroll.x;
      achievementTreeScrollY = scroll.y;
      uiDirty = true;
    }

    mouseLeftDown = leftPressed;
    mouseRightDown = rightPressed;
    dropOneDown = dropPressed;
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

  if (canFly() && spacePressed && !jumpDown) {
    if (flightToggleTimer > 0.0f) {
      flying = !flying;
      flightToggleTimer = 0.0f;
      playerVel.y = 0.0f;
      onGround = false;
    } else {
      flightToggleTimer = kFlightToggleWindowSec;
    }
  }
  jumpDown = spacePressed;

  bool inWater = !flying && intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f));
  bool shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  bool sprintPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
  crouching = !flying && !inWater && shiftPressed;
  sprinting = !inWater && !crouching && sprintPressed && glm::length(wishDir) > 0.001f;
  const float moveSpeed = flying
    ? (sprinting ? kCreativeFlyBoostSpeed : kCreativeFlySpeed)
    : (inWater ? 3.2f : (crouching ? 2.6f : (sprinting ? 8.2f : 6.0f)));
  playerVel.x = wishDir.x * moveSpeed;
  playerVel.z = wishDir.z * moveSpeed;

  if (dropPressed && !dropOneDown) {
    ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
    if (stack.count > 0 && stack.type != kAir) {
      glm::vec3 viewDir = cameraFront();
      glm::vec3 horizontalDir(viewDir.x, 0.0f, viewDir.z);
      if (glm::dot(horizontalDir, horizontalDir) < 0.0001f) {
        float yawRadians = glm::radians(yaw);
        horizontalDir = glm::vec3(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
      } else {
        horizontalDir = glm::normalize(horizontalDir);
      }

      glm::vec3 dropSource =
        playerPos + glm::vec3(0.0f, 1.15f, 0.0f) + horizontalDir * kThrownItemSpawnAhead;
      glm::vec3 dropVelocity =
        horizontalDir * kThrownItemForwardSpeed +
        glm::vec3(playerVel.x, 0.0f, playerVel.z) * 0.45f +
        glm::vec3(0.0f, kThrownItemUpwardSpeed, 0.0f);
      spawnDroppedItemWithPhysics(stack.type, dropSource, dropVelocity, kThrownItemPickupDelay);
      triggerFirstPersonUseAnimation(0.55f);
      if (!isCreativeMode()) {
        stack.count = static_cast<uint16_t>(stack.count - 1);
        if (stack.count == 0) {
          stack.type = kAir;
        }
        refreshSelectedBlock();
      }
      uiDirty = true;
    }
  }
  dropOneDown = dropPressed;

  if (flying) {
    float verticalIntent = 0.0f;
    if (spacePressed) {
      verticalIntent += 1.0f;
    }
    if (shiftPressed) {
      verticalIntent -= 1.0f;
    }
    playerVel.y = verticalIntent * (sprinting ? kCreativeVerticalFlyBoostSpeed : kCreativeVerticalFlySpeed);
    onGround = false;
  } else if (inWater) {
    float verticalIntent = 0.0f;
    if (spacePressed) {
      verticalIntent += 1.0f;
    }
    if (shiftPressed) {
      verticalIntent -= 1.0f;
    }

    if (std::abs(verticalIntent) > 0.001f) {
      playerVel.y = verticalIntent * 4.4f;
    } else if (playerVel.y > -0.9f) {
      playerVel.y = -0.9f;
    }
    onGround = false;
  } else if (onGround && spacePressed) {
    playerVel.y = 6.5f;
    onGround = false;
  }

  ToolTier equippedTool = toolTierForItem(selectedBlock);

  bool sheepAttackHandled = false;
  if (leftPressed && !mouseLeftDown) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
    glm::vec3 breakDir = cameraFront();
    SheepHit sheepHit = raycastSheep(origin, breakDir, kBreakMaxDistance);
    if (sheepHit.hit) {
      RaycastHit blockHit = raycast(origin, breakDir, kBreakMaxDistance);
      float blockDist = std::numeric_limits<float>::infinity();
      if (blockHit.hit) {
        glm::vec3 blockCenter = glm::vec3(blockHit.block) + glm::vec3(0.5f);
        blockDist = glm::length(blockCenter - origin);
      }
      if (!blockHit.hit || sheepHit.distance <= blockDist + 0.08f) {
        if (!firstPersonSwingActive || firstPersonSwingProgress >= 0.72f) {
          triggerFirstPersonSwing(1.0f);
        }
        damageSheep(sheepHit.index, isCreativeMode() ? 99 : 3, breakDir);
        sheepAttackHandled = true;
        breakingActive = false;
        breakingProgress = 0.0f;
        breakingStage = 0;
        world.clearBreakOverlay();
      }
    }
  }

  if (leftPressed && !sheepAttackHandled) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
    glm::vec3 breakDir = cameraFront();
    RaycastHit hit = raycast(origin, breakDir, kBreakMaxDistance);
    if (hit.hit) {
      glm::ivec3 targetBlock = hit.block;
      uint8_t hitType = world.getBlock(targetBlock.x, targetBlock.y, targetBlock.z);

      auto replacementBlockAfterBreak = [&](const glm::ivec3& blockPos, uint8_t removedType) -> uint8_t {
        if (!isUnderwaterPlantBlock(removedType)) {
          return kAir;
        }

        constexpr std::array<glm::ivec3, 6> kNeighbors = {{
          {0, 1, 0},
          {0, -1, 0},
          {1, 0, 0},
          {-1, 0, 0},
          {0, 0, 1},
          {0, 0, -1}
        }};
        for (const glm::ivec3& offset : kNeighbors) {
          if (isWaterVolumeBlock(world.getBlock(blockPos.x + offset.x,
                                                blockPos.y + offset.y,
                                                blockPos.z + offset.z))) {
            return kWater;
          }
        }
        return kAir;
      };

      auto completeBreakTarget = [&](const glm::ivec3& blockPos, bool spawnDrops) {
        uint8_t removed = world.getBlock(blockPos.x, blockPos.y, blockPos.z);
        if (removed != kAir && !isWaterBlock(removed)) {
          if (isBedBlock(removed)) {
            glm::ivec3 otherPos = blockPos + bedOtherPartOffset(removed);
            uint8_t otherType = world.getBlock(otherPos.x, otherPos.y, otherPos.z);
            if (isBedBlock(otherType)) {
              world.setBlock(otherPos.x, otherPos.y, otherPos.z, kAir);
            }
          }
          uint8_t replacement = replacementBlockAfterBreak(blockPos, removed);
          world.setBlock(blockPos.x, blockPos.y, blockPos.z, replacement);
          audio.playCue(breakCueForBlock(removed), breakGainForBlock(removed));
          if (isFurnaceBlock(removed)) {
            auto furnaceIt = furnaceStates.find(furnaceKeyForBlock(blockPos));
            if (furnaceIt != furnaceStates.end()) {
              spawnDroppedStack(furnaceIt->second.input, blockPos);
              spawnDroppedStack(furnaceIt->second.fuel, blockPos);
              spawnDroppedStack(furnaceIt->second.output, blockPos);
              furnaceStates.erase(furnaceIt);
            }
            if (furnaceOpen && activeFurnaceBlock == blockPos) {
              furnaceOpen = false;
            }
          }
          waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);
          if (spawnDrops && shouldDropBrokenBlock(removed, equippedTool)) {
            spawnDroppedItem(droppedItemForBlock(removed), blockPos);
          }
          if (spawnDrops && removed == kLeaves && shouldDropStickFromLeaves(blockPos, world.getSeed())) {
            spawnDroppedItem(kStick, blockPos);
          }
        }
        breakingActive = false;
        breakingProgress = 0.0f;
        breakingStage = 0;
        world.clearBreakOverlay();
      };

      auto breakTarget = [&](const glm::ivec3& blockPos, uint8_t blockType) {
        if (!firstPersonSwingActive || firstPersonSwingProgress >= 0.72f) {
          triggerFirstPersonSwing(1.0f);
        }
        if (isCreativeMode()) {
          completeBreakTarget(blockPos, false);
          return;
        }
        if (breaksInstantly(blockType)) {
          completeBreakTarget(blockPos, true);
          return;
        }
        float breakDuration = breakDurationForBlock(blockType, equippedTool, inWater);
        if (breakDuration <= 0.0f) {
          return;
        }

        if (!breakingActive || blockPos != breakingBlock) {
          breakingActive = true;
          breakingBlock = blockPos;
          breakingProgress = 0.0f;
          breakingStage = 0;
        }

        breakingProgress += deltaTime;
        int newStage = static_cast<int>((breakingProgress / breakDuration) * kBreakStages) + 1;
        newStage = std::clamp(newStage, 1, kBreakStages);
        if (newStage != breakingStage) {
          breakingStage = newStage;
          world.setBreakOverlay(breakingBlock, breakingStage);
        }

        if (breakingProgress >= breakDuration) {
          completeBreakTarget(blockPos, true);
        }
      };

      if (isWaterBlock(hitType)) {
        bool foundTarget = false;

        // Prefer the block directly behind water along the crosshair ray.
        glm::ivec3 lastVoxel = hit.block;
        for (float d = 0.06f; d <= kBreakMaxDistance; d += 0.06f) {
          glm::vec3 sample = origin + breakDir * d;
          glm::ivec3 voxel(static_cast<int>(std::floor(sample.x)),
                           static_cast<int>(std::floor(sample.y)),
                           static_cast<int>(std::floor(sample.z)));
          if (voxel == lastVoxel) {
            continue;
          }
          lastVoxel = voxel;
          if (!world.inBounds(voxel.x, voxel.y, voxel.z)) {
            break;
          }

          uint8_t candidate = world.getBlock(voxel.x, voxel.y, voxel.z);
          if (candidate == kAir || isWaterBlock(candidate)) {
            continue;
          }
          targetBlock = voxel;
          hitType = candidate;
          foundTarget = true;
          break;
        }

        // Fallback: if ray did not find a solid, mine in the water column below.
        if (!foundTarget) {
          constexpr int kMaxWaterProbeDepth = 12;
          int minY = std::max(0, hit.block.y - kMaxWaterProbeDepth);
          for (int y = hit.block.y - 1; y >= minY; --y) {
            uint8_t belowType = world.getBlock(hit.block.x, y, hit.block.z);
            if (belowType == kAir || isWaterBlock(belowType)) {
              continue;
            }
            targetBlock = {hit.block.x, y, hit.block.z};
            hitType = belowType;
            foundTarget = true;
            break;
          }
        }

        if (!foundTarget) {
          if (breakingActive) {
            breakingActive = false;
            breakingProgress = 0.0f;
            breakingStage = 0;
            world.clearBreakOverlay();
          }
        } else {
          breakTarget(targetBlock, hitType);
        }
      } else {
        breakTarget(targetBlock, hitType);
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
    ItemStack& heldStack = hotbar[static_cast<size_t>(selectedSlot)];
    if (heldStack.count > 0 &&
        heldStack.type != kAir &&
        isEdibleItem(heldStack.type) &&
        playerHealth < kMaxPlayerHealth) {
      playerHealth = std::min(kMaxPlayerHealth, playerHealth + foodHealAmount(heldStack.type));
      triggerFirstPersonUseAnimation(0.80f);
      if (!isCreativeMode()) {
        heldStack.count = static_cast<uint16_t>(heldStack.count - 1);
        if (heldStack.count == 0) {
          heldStack.type = kAir;
        }
        refreshSelectedBlock();
      }
      uiDirty = true;
      mouseLeftDown = leftPressed;
      mouseRightDown = rightPressed;
      return;
    }

    glm::vec3 origin = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
    glm::vec3 placeDir = cameraFront();
    RaycastHit hit = raycast(origin, placeDir, kBreakMaxDistance);
    if (hit.hit) {
      glm::ivec3 target = hit.block + hit.normal;
      glm::ivec3 placementNormal = hit.normal;
      uint8_t hitType = world.getBlock(hit.block.x, hit.block.y, hit.block.z);

      if (hitType == kLootCache && !crouching) {
        if (claimLootCache(hit.block)) {
          triggerFirstPersonUseAnimation(0.75f);
          mouseLeftDown = leftPressed;
          mouseRightDown = rightPressed;
          return;
        }
      }
      if (isWorkbenchBlock(hitType) && !crouching) {
        activeWorkbenchBlock = hit.block;
        furnaceOpen = false;
        workbenchOpen = true;
        triggerFirstPersonUseAnimation(0.70f);
        setInventoryOpen(true);
        mouseLeftDown = leftPressed;
        mouseRightDown = rightPressed;
        return;
      }
      if (isFurnaceBlock(hitType) && !crouching) {
        activeFurnaceBlock = hit.block;
        workbenchOpen = false;
        furnaceOpen = true;
        returnCraftingItemsToInventory();
        triggerFirstPersonUseAnimation(0.70f);
        setInventoryOpen(true);
        mouseLeftDown = leftPressed;
        mouseRightDown = rightPressed;
        return;
      }
      if ((hitType == kBed || isBedBlock(hitType)) && !crouching) {
        glm::ivec3 respawnBlock = hit.block;
        if (isBedHeadBlock(hitType)) {
          respawnBlock += bedOtherPartOffset(hitType);
        }
        respawnPos = glm::vec3(static_cast<float>(respawnBlock.x) + 0.5f,
                               static_cast<float>(respawnBlock.y) + 0.62f,
                               static_cast<float>(respawnBlock.z) + 0.5f);
        triggerFirstPersonUseAnimation(0.55f);
        mouseLeftDown = leftPressed;
        mouseRightDown = rightPressed;
        return;
      }

      if (isWaterBlock(hitType)) {
        bool foundPlace = false;
        glm::ivec3 lastReplaceable = hit.block;
        glm::ivec3 lastVoxel = hit.block;

        for (float d = 0.06f; d <= kBreakMaxDistance; d += 0.06f) {
          glm::vec3 sample = origin + placeDir * d;
          glm::ivec3 voxel(static_cast<int>(std::floor(sample.x)),
                           static_cast<int>(std::floor(sample.y)),
                           static_cast<int>(std::floor(sample.z)));
          if (voxel == lastVoxel) {
            continue;
          }
          lastVoxel = voxel;
          if (!world.inBounds(voxel.x, voxel.y, voxel.z)) {
            break;
          }

          uint8_t candidate = world.getBlock(voxel.x, voxel.y, voxel.z);
          if (candidate == kAir || isWaterBlock(candidate) || isDecorationBlock(candidate)) {
            lastReplaceable = voxel;
            continue;
          }

          uint8_t replaceType = world.getBlock(lastReplaceable.x, lastReplaceable.y, lastReplaceable.z);
          if (replaceType == kAir || isWaterBlock(replaceType) || isDecorationBlock(replaceType)) {
            target = lastReplaceable;
            foundPlace = true;
          }
          break;
        }

        if (!foundPlace) {
          target = hit.block;
        }
      }

      uint8_t targetType = world.getBlock(target.x, target.y, target.z);
      bool targetReplaceable = targetType == kAir ||
                               isWaterBlock(targetType) ||
                               isDecorationBlock(targetType);
      ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
      glm::vec3 adjustedPlayerPos = playerPos;
      uint8_t placedType = placedBlockTypeForItem(stack.type, placeDir, placementNormal);
      if (stack.count > 0 && stack.type == kBed) {
        glm::ivec3 headPos = target + bedFacingVector(placedType);
        uint8_t headPlacedType = bedBlockForFacing(bedFacingIndex(placedType), true);
        bool headInBounds = world.inBounds(headPos.x, headPos.y, headPos.z);
        uint8_t headTargetType = headInBounds ? world.getBlock(headPos.x, headPos.y, headPos.z) : kAir;
        bool headReplaceable = headTargetType == kAir ||
                               isWaterBlock(headTargetType) ||
                               isDecorationBlock(headTargetType);
        glm::vec3 headAdjustedPlayerPos = playerPos;
        auto hasSolidSupport = [&](const glm::ivec3& block) {
          uint8_t ground = world.getBlock(block.x, block.y - 1, block.z);
          return ground != kAir && !isWaterBlock(ground) && !isDecorationBlock(ground);
        };

        if (world.inBounds(target.x, target.y, target.z) &&
            headInBounds &&
            targetReplaceable &&
            headReplaceable &&
            canPlaceBlockAt(target.x, target.y, target.z, placedType, &adjustedPlayerPos) &&
            canPlaceBlockAt(headPos.x, headPos.y, headPos.z, headPlacedType, &headAdjustedPlayerPos) &&
            hasSolidSupport(target) &&
            hasSolidSupport(headPos)) {
          world.setBlock(target.x, target.y, target.z, placedType);
          world.setBlock(headPos.x, headPos.y, headPos.z, headPlacedType);
          audio.playCue(placeCueForBlock(kBed), placeGainForBlock(kBed));
          triggerFirstPersonUseAnimation(0.82f);
          waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);
          float liftY = std::max(adjustedPlayerPos.y, headAdjustedPlayerPos.y);
          if (liftY > playerPos.y + 0.0001f) {
            playerPos.y = liftY;
            playerVel.y = std::max(0.0f, playerVel.y);
            onGround = true;
          }
          if (!isCreativeMode()) {
            stack.count -= 1;
            if (stack.count == 0) {
              stack.type = kAir;
            }
            refreshSelectedBlock();
          }
          uiDirty = true;
        }
        mouseLeftDown = leftPressed;
        mouseRightDown = rightPressed;
        return;
      }
      if (world.inBounds(target.x, target.y, target.z) &&
          placedType != kAir &&
          targetReplaceable &&
          canPlaceBlockAt(target.x, target.y, target.z, placedType, &adjustedPlayerPos)) {
        if (stack.count > 0 && stack.type != kAir && isPlaceableItem(stack.type)) {
          if (!hasPlacementSupportAt(world, target.x, target.y, target.z, placedType, targetType)) {
            mouseLeftDown = leftPressed;
            mouseRightDown = rightPressed;
            return;
          }
          world.setBlock(target.x, target.y, target.z, placedType);
          audio.playCue(placeCueForBlock(placedType), placeGainForBlock(placedType));
          triggerFirstPersonUseAnimation(0.82f);
          waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);
          if (adjustedPlayerPos.y > playerPos.y + 0.0001f) {
            playerPos = adjustedPlayerPos;
            playerVel.y = std::max(0.0f, playerVel.y);
            onGround = true;
          }
          if (!isCreativeMode()) {
            stack.count -= 1;
            if (stack.count == 0) {
              stack.type = kAir;
            }
            refreshSelectedBlock();
          }
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
  waterSwimSoundTimer = std::max(0.0f, waterSwimSoundTimer - deltaTime);

  glm::vec3 startPos = playerPos;
  bool wasOnGround = onGround;
  bool inWater = !flying && intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f));
  if (flying) {
    playerVel.y = std::clamp(playerVel.y, -kCreativeVerticalFlyBoostSpeed, kCreativeVerticalFlyBoostSpeed);
  } else if (inWater) {
    const float waterGravity = -5.0f;
    playerVel.y += waterGravity * deltaTime;
    playerVel.y = std::clamp(playerVel.y, -2.7f, 4.0f);
  } else {
    const float gravity = -18.0f;
    playerVel.y += gravity * deltaTime;
  }

  glm::vec2 horizontalVel(playerVel.x, playerVel.z);
  float horizontalSpeed = glm::length(horizontalVel);
  if (horizontalSpeed > 0.18f) {
    glm::vec2 moveDir = horizontalVel / horizontalSpeed;
    glm::vec3 probePos = playerPos + glm::vec3(moveDir.x, 0.0f, moveDir.y) * kStreamingProbeBlocks;
    int probeCx = static_cast<int>(std::floor(probePos.x / static_cast<float>(kChunkSize)));
    int probeCz = static_cast<int>(std::floor(probePos.z / static_cast<float>(kChunkSize)));
    if (world.getChunkGenerationStatus(probeCx, probeCz) < ChunkGenStatus::kSurface) {
      for (int dz = -kStreamingWarmRadius; dz <= kStreamingWarmRadius; ++dz) {
        for (int dx = -kStreamingWarmRadius; dx <= kStreamingWarmRadius; ++dx) {
          int dist = std::max(std::abs(dx), std::abs(dz));
          ChunkGenStatus target = dist == 0 ? ChunkGenStatus::kFeatures : ChunkGenStatus::kSurface;
          world.requestChunkToStatus(probeCx + dx, probeCz + dz, target);
        }
      }
    }
  }

  glm::vec3 pos = playerPos;

  // X axis
  pos.x += playerVel.x * deltaTime;
  if (collidesAt(pos)) {
    if (inWater) {
      glm::vec3 stepPos{playerPos.x + playerVel.x * deltaTime,
                        playerPos.y + 1.05f,
                        playerPos.z};
      if (!collidesAt(stepPos)) {
        pos = stepPos;
      } else {
        pos.x = playerPos.x;
        playerVel.x = 0.0f;
      }
    } else {
      pos.x = playerPos.x;
      playerVel.x = 0.0f;
    }
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
    if (inWater) {
      glm::vec3 stepPos{pos.x,
                        pos.y + 1.05f,
                        playerPos.z + playerVel.z * deltaTime};
      if (!collidesAt(stepPos)) {
        pos = stepPos;
      } else {
        pos.z = playerPos.z;
        playerVel.z = 0.0f;
      }
    } else {
      pos.z = playerPos.z;
      playerVel.z = 0.0f;
    }
  }

  playerPos = pos;
  bool inWaterAfterMove = !flying && intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f));
  glm::vec3 frameDelta = playerPos - startPos;
  float horizontalMove = glm::length(glm::vec2(frameDelta.x, frameDelta.z));
  if (flying || inWaterAfterMove) {
    fallDistance = 0.0f;
  } else if (playerPos.y < startPos.y) {
    fallDistance += startPos.y - playerPos.y;
  }

  if (inWaterAfterMove) {
    onGround = false;

    float verticalMove = std::abs(frameDelta.y);
    bool enteredWater = !wasPlayerInWater;
    bool activeSwim = horizontalMove > 0.05f || verticalMove > 0.04f;
    if ((enteredWater || activeSwim) && waterSwimSoundTimer <= 0.0f) {
      float swimGain = enteredWater
                         ? 1.02f
                         : std::clamp(0.74f + horizontalMove * 3.8f + verticalMove * 2.4f, 0.78f, 1.12f);
      audio.playCue(AudioSystem::Cue::kWaterSwim, swimGain);
      waterSwimSoundTimer = enteredWater ? 0.52f : (horizontalMove > 0.12f ? 0.36f : 0.46f);
    }
  } else {
    waterSwimSoundTimer = 0.0f;
  }
  wasPlayerInWater = inWaterAfterMove;

  if (!inWaterAfterMove && onGround && !flying && horizontalMove > 0.015f) {
    FootstepSurface footstepSurface = footstepSurfaceUnderPlayer(world, playerPos);
    if (footstepSurface != FootstepSurface::kNone) {
      footstepDistanceAccumulator += horizontalMove;
      float stepDistance = sprinting ? 1.28f : (crouching ? 2.05f : 1.62f);
      while (footstepDistanceAccumulator >= stepDistance) {
        footstepDistanceAccumulator -= stepDistance;
        if (footstepSurface == FootstepSurface::kGrass) {
          int variant = chooseFootstepVariant(footstepRngState,
                                              kGrassFootstepVariantCount,
                                              lastGrassFootstepVariant);
          lastGrassFootstepVariant = variant;
          float gain = std::clamp((sprinting ? 1.02f : (crouching ? 0.78f : 0.92f)) +
                                  horizontalMove * 0.85f,
                                  0.72f,
                                  1.16f);
          audio.playCue(footstepCueForVariant(footstepSurface, variant), gain);
        } else {
          int variant = chooseFootstepVariant(footstepRngState,
                                              kStoneFootstepVariantCount,
                                              lastStoneFootstepVariant);
          lastStoneFootstepVariant = variant;
          float gain = std::clamp((sprinting ? 0.98f : (crouching ? 0.76f : 0.88f)) +
                                  horizontalMove * 0.82f,
                                  0.70f,
                                  1.10f);
          audio.playCue(footstepCueForVariant(footstepSurface, variant), gain);
        }
      }
    } else {
      footstepDistanceAccumulator = 0.0f;
    }
  } else {
    footstepDistanceAccumulator = 0.0f;
  }

  bool landedThisFrame = !wasOnGround && onGround && !inWaterAfterMove && !flying;
  if (landedThisFrame && !isCreativeMode()) {
    int fallDamage = std::max(0, static_cast<int>(std::floor(fallDistance - 3.0f)));
    if (fallDamage > 0) {
      damagePlayer(fallDamage);
    }
    fallDistance = 0.0f;
  } else if (onGround) {
    fallDistance = 0.0f;
  }

  refreshAchievementsProgress();
}

void App::updateStreaming() {
  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  int streamingRadius = std::max(kMinRenderDistance, std::min(loadedChunkViewRadius, activeChunkViewRadius));
  if (!chunkCenterValid || cx != currentChunkX || cz != currentChunkZ) {
    currentChunkX = cx;
    currentChunkZ = cz;
    chunkCenterValid = true;
    world.updateActiveChunks(cx, cz, streamingRadius);
  }

  if (loadedChunkViewRadius < activeChunkViewRadius) {
    double now = glfwGetTime();
    World::RuntimeStats stats = world.collectRuntimeStats();
    bool busy =
      stats.generatingChunks > 12 ||
      stats.generationQueue > 18 ||
      stats.pendingSectionRebuilds > 24 ||
      stats.dirtySections > 96;
    if (!busy && now >= nextStreamingRadiusExpandTime) {
      loadedChunkViewRadius = std::min(activeChunkViewRadius, loadedChunkViewRadius + 1);
      world.updateActiveChunks(cx, cz, loadedChunkViewRadius);
      nextStreamingRadiusExpandTime = now + streamingExpansionIntervalForQuality(appliedSettings.graphicsQuality);
    }
  }

  glm::vec2 streamDir{0.0f};
  glm::vec3 forward3 = cameraFront();
  glm::vec2 forward2(forward3.x, forward3.z);
  float forwardLen = glm::length(forward2);
  if (forwardLen > 0.001f) {
    streamDir = forward2 / forwardLen;
  }

  if (glm::length(streamDir) > 0.001f) {
    glm::vec3 streamAnchor = playerPos;
    streamAnchor.x += streamDir.x * kStreamingLookaheadBlocks;
    streamAnchor.z += streamDir.y * kStreamingLookaheadBlocks;
    int streamCx = static_cast<int>(std::floor(streamAnchor.x / static_cast<float>(kChunkSize)));
    int streamCz = static_cast<int>(std::floor(streamAnchor.z / static_cast<float>(kChunkSize)));
    if (streamCx != cx || streamCz != cz) {
      for (int dz = -kStreamingWarmRadius; dz <= kStreamingWarmRadius; ++dz) {
        for (int dx = -kStreamingWarmRadius; dx <= kStreamingWarmRadius; ++dx) {
          int dist = std::max(std::abs(dx), std::abs(dz));
          ChunkGenStatus target = dist == 0 ? ChunkGenStatus::kFeatures : ChunkGenStatus::kSurface;
          if (world.getChunkGenerationStatus(streamCx + dx, streamCz + dz) < target) {
            world.requestChunkToStatus(streamCx + dx, streamCz + dz, target);
          }
        }
      }
    }
  }
}

std::vector<glm::vec4> App::collectTorchLights(size_t maxLights) const {
  size_t clampedMaxLights = std::min(maxLights, static_cast<size_t>(kMaxTorchLights));
  if (clampedMaxLights == 0) {
    return {};
  }

  std::vector<std::pair<float, glm::vec4>> rankedLights;
  std::vector<glm::ivec3> torchBlocks = world.collectTorchBlocks();
  rankedLights.reserve(torchBlocks.size() + 4);

  glm::vec3 eye = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
  float candidateRangeSq = kTorchLightCandidateRange * kTorchLightCandidateRange;

  auto considerLight = [&](const glm::vec3& lightPos, float range) {
    glm::vec3 delta = lightPos - eye;
    float distSq = glm::dot(delta, delta);
    if (distSq > candidateRangeSq) {
      return;
    }
    rankedLights.push_back({distSq, glm::vec4(lightPos, range)});
  };

  for (const glm::ivec3& torchBlock : torchBlocks) {
    uint8_t torchType = world.getBlock(torchBlock.x, torchBlock.y, torchBlock.z);
    if (!isTorchBlock(torchType)) {
      continue;
    }
    considerLight(torchLightWorldPosition(torchBlock.x, torchBlock.y, torchBlock.z, torchType),
                  kTorchLightRange);
  }

  if (selectedBlock == kTorch) {
    glm::vec3 heldLight = eye + cameraFront() * 0.46f + glm::vec3(0.0f, -0.18f, 0.0f);
    considerLight(heldLight, 6.4f);
  }

  std::sort(rankedLights.begin(),
            rankedLights.end(),
            [](const auto& a, const auto& b) {
              return a.first < b.first;
            });
  if (rankedLights.size() > clampedMaxLights) {
    rankedLights.resize(clampedMaxLights);
  }

  std::vector<glm::vec4> lights;
  lights.reserve(rankedLights.size());
  for (const auto& entry : rankedLights) {
    lights.push_back(entry.second);
  }
  return lights;
}

void App::rebuildWorldMesh() {
  std::vector<ChunkMeshUpload> snapshot;
  world.snapshotChunkMeshes(snapshot);
  if (vkReady) {
    vk.setWorldChunkMeshes(toVkChunkUploads(std::move(snapshot)));
    pendingWorldChunkUploads.clear();
    pendingWorldChunkRemovals.clear();
    lastWorldChunkUploadTime = glfwGetTime();
  }
}

void App::rebuildUiMesh() {
  skyVertices.clear();
  skyIndices.clear();
  uiVertices.clear();
  uiIndices.clear();

  if (width <= 0 || height <= 0) {
    return;
  }

  const float uiScale = uiScaleFactor();
  const float uiW = static_cast<float>(uiLayoutWidth());
  const float uiH = static_cast<float>(uiLayoutHeight());

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (uiW - totalWidth) * 0.5f;
  const float startY = uiH - kMarginBottom - kSlotSize;

  auto toNdc = [&](float px, float py) -> glm::vec2 {
    float scaledX = px * uiScale;
    float scaledY = py * uiScale;
    float x = (scaledX / static_cast<float>(width)) * 2.0f - 1.0f;
    // Vulkan NDC maps +Y toward the bottom with a positive viewport height.
    float y = (scaledY / static_cast<float>(height)) * 2.0f - 1.0f;
    return {x, y};
  };

  auto addSkyQuad = [&](float x, float y, float w, float h) {
    glm::vec2 p0 = toNdc(x, y);
    glm::vec2 p1 = toNdc(x + w, y);
    glm::vec2 p2 = toNdc(x + w, y + h);
    glm::vec2 p3 = toNdc(x, y + h);

    // Negative red channel marks a procedural cloud pass in fragment shader.
    glm::vec3 skyMarkerColor{-1.0f, 0.0f, 0.0f};
    uint32_t start = static_cast<uint32_t>(skyVertices.size());
    skyVertices.push_back({{p0.x, p0.y, 0.0f}, skyMarkerColor, {0.0f, 0.0f}});
    skyVertices.push_back({{p1.x, p1.y, 0.0f}, skyMarkerColor, {1.0f, 0.0f}});
    skyVertices.push_back({{p2.x, p2.y, 0.0f}, skyMarkerColor, {1.0f, 1.0f}});
    skyVertices.push_back({{p3.x, p3.y, 0.0f}, skyMarkerColor, {0.0f, 1.0f}});

    skyIndices.push_back(start + 0);
    skyIndices.push_back(start + 1);
    skyIndices.push_back(start + 2);
    skyIndices.push_back(start + 0);
    skyIndices.push_back(start + 2);
    skyIndices.push_back(start + 3);
  };

  addSkyQuad(0.0f, 0.0f, uiW, uiH);

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

  auto addUvQuad = [&](float x,
                       float y,
                       float w,
                       float h,
                       const glm::vec3& color,
                       const glm::vec2& uv0,
                       const glm::vec2& uv1,
                       const glm::vec2& uv2,
                       const glm::vec2& uv3) {
    glm::vec2 p0 = toNdc(x, y);
    glm::vec2 p1 = toNdc(x + w, y);
    glm::vec2 p2 = toNdc(x + w, y + h);
    glm::vec2 p3 = toNdc(x, y + h);

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

  auto addSolidQuad = [&](float x, float y, float w, float h, const glm::vec3& color) {
    glm::vec2 p0 = toNdc(x, y);
    glm::vec2 p1 = toNdc(x + w, y);
    glm::vec2 p2 = toNdc(x + w, y + h);
    glm::vec2 p3 = toNdc(x, y + h);

    // Negative green channel flags a texture-free UI primitive in fragment shader.
    glm::vec3 markerColor{
      std::clamp(color.r, 0.0f, 1.0f),
      -(1.0f + std::clamp(color.g, 0.0f, 1.0f)),
      std::clamp(color.b, 0.0f, 1.0f)
    };
    uint32_t start = static_cast<uint32_t>(uiVertices.size());
    uiVertices.push_back({{p0.x, p0.y, 0.0f}, markerColor, {0.0f, 0.0f}});
    uiVertices.push_back({{p1.x, p1.y, 0.0f}, markerColor, {0.0f, 0.0f}});
    uiVertices.push_back({{p2.x, p2.y, 0.0f}, markerColor, {0.0f, 0.0f}});
    uiVertices.push_back({{p3.x, p3.y, 0.0f}, markerColor, {0.0f, 0.0f}});

    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 1);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 3);
  };

  auto addPointUvQuad = [&](const glm::vec2& p0,
                            const glm::vec2& p1,
                            const glm::vec2& p2,
                            const glm::vec2& p3,
                            const glm::vec3& color,
                            const glm::vec2& uv0,
                            const glm::vec2& uv1,
                            const glm::vec2& uv2,
                            const glm::vec2& uv3) {
    glm::vec2 ndc0 = toNdc(p0.x, p0.y);
    glm::vec2 ndc1 = toNdc(p1.x, p1.y);
    glm::vec2 ndc2 = toNdc(p2.x, p2.y);
    glm::vec2 ndc3 = toNdc(p3.x, p3.y);

    uint32_t start = static_cast<uint32_t>(uiVertices.size());
    uiVertices.push_back({{ndc0.x, ndc0.y, 0.0f}, color, uv0});
    uiVertices.push_back({{ndc1.x, ndc1.y, 0.0f}, color, uv1});
    uiVertices.push_back({{ndc2.x, ndc2.y, 0.0f}, color, uv2});
    uiVertices.push_back({{ndc3.x, ndc3.y, 0.0f}, color, uv3});

    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 1);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 0);
    uiIndices.push_back(start + 2);
    uiIndices.push_back(start + 3);
  };

  auto addPointTileQuad = [&](const glm::vec2& p0,
                              const glm::vec2& p1,
                              const glm::vec2& p2,
                              const glm::vec2& p3,
                              const glm::vec3& color,
                              int tile) {
    addPointUvQuad(p0,
                   p1,
                   p2,
                   p3,
                   color,
                   uvForTile(tile, 0.0f, 0.0f),
                   uvForTile(tile, 1.0f, 0.0f),
                   uvForTile(tile, 1.0f, 1.0f),
                   uvForTile(tile, 0.0f, 1.0f));
  };

  const int backgroundTile = tileForBlock(kStone);

  auto findTextGlyph = [&](uint32_t codepoint) -> const VulkanContext::UiGlyphInfo* {
    if (!vk.hasUiFont()) {
      return nullptr;
    }
    if (const auto* glyph = vk.findUiGlyph(codepoint)) {
      return glyph;
    }
    uint32_t upper = toUpperCodepoint(codepoint);
    if (upper != codepoint) {
      return vk.findUiGlyph(upper);
    }
    return nullptr;
  };

  auto bitmapAdvanceForCodepoint = [&](uint32_t codepoint, float pixel) -> float {
    if (pixel <= 0.0f) {
      return 0.0f;
    }
    codepoint = toUpperCodepoint(codepoint);
    if (codepoint == static_cast<uint32_t>(' ')) {
      return pixel * 2.0f;
    }
    return pixel * static_cast<float>(kGlyphWidth + 1);
  };

  auto drawBitmapGlyph = [&](uint32_t codepoint,
                             float x,
                             float y,
                             float pixel,
                             const glm::vec3& color) {
    if (pixel <= 0.0f) {
      return;
    }

    codepoint = toUpperCodepoint(codepoint);
    if (codepoint == static_cast<uint32_t>(' ')) {
      return;
    }

    uint8_t fallback[kGlyphHeight] = {0, 0, 0, 0, 0};
    const uint8_t* glyph = glyphForCodepoint(codepoint);
    if (!glyph) {
      switch (codepoint) {
        case static_cast<uint32_t>('-'):
          fallback[2] = 0b111;
          glyph = fallback;
          break;
        case static_cast<uint32_t>('.'):
          fallback[4] = 0b010;
          glyph = fallback;
          break;
        case static_cast<uint32_t>(':'):
          fallback[1] = 0b010;
          fallback[3] = 0b010;
          glyph = fallback;
          break;
        case static_cast<uint32_t>('/'):
          fallback[0] = 0b001;
          fallback[1] = 0b001;
          fallback[2] = 0b010;
          fallback[3] = 0b100;
          fallback[4] = 0b100;
          glyph = fallback;
          break;
        default:
          fallback[0] = 0b111;
          fallback[1] = 0b001;
          fallback[2] = 0b010;
          fallback[3] = 0b000;
          fallback[4] = 0b010;
          glyph = fallback;
          break;
      }
    }

    for (int row = 0; row < kGlyphHeight; ++row) {
      uint8_t bits = glyph[row];
      for (int col = 0; col < kGlyphWidth; ++col) {
        int bit = kGlyphWidth - 1 - col;
        if ((bits & (1u << bit)) == 0) {
          continue;
        }
        addSolidQuad(x + static_cast<float>(col) * pixel,
                     y + static_cast<float>(row) * pixel,
                     pixel,
                     pixel,
                     color);
      }
    }
  };

  auto measureTextWidth = [&](const std::string& text, float pixel) -> float {
    float widthPx = 0.0f;
    float fontScale = 0.0f;
    if (vk.hasUiFont() && vk.uiFontLineHeight() > 0.0f) {
      fontScale = (pixel * kUiTextLineHeightPerPixel) / vk.uiFontLineHeight();
    }
    size_t index = 0;
    while (index < text.size()) {
      uint32_t cp = 0;
      if (!nextUtf8Codepoint(text, index, cp)) {
        break;
      }
      if (cp == static_cast<uint32_t>('\n')) {
        break;
      }
      if (const auto* glyph = findTextGlyph(cp)) {
        widthPx += std::max(glyph->advance, glyph->width + 1.0f) * fontScale;
        continue;
      }
      widthPx += bitmapAdvanceForCodepoint(cp, pixel);
    }
    return widthPx;
  };

  auto drawText = [&](const std::string& text,
                      float x,
                      float y,
                      float pixel,
                      const glm::vec3& color,
                      bool centered) {
    if (pixel <= 0.0f) {
      return;
    }

    float fontScale = 0.0f;
    if (vk.hasUiFont() && vk.uiFontLineHeight() > 0.0f) {
      fontScale = (pixel * kUiTextLineHeightPerPixel) / vk.uiFontLineHeight();
    }

    if (centered) {
      x -= measureTextWidth(text, pixel) * 0.5f;
    }

    size_t index = 0;
    while (index < text.size()) {
      uint32_t cp = 0;
      if (!nextUtf8Codepoint(text, index, cp)) {
        break;
      }
      if (cp == static_cast<uint32_t>('\n')) {
        break;
      }

      if (const auto* glyph = findTextGlyph(cp)) {
        if (glyph->width > 0.0f && glyph->height > 0.0f) {
          float gx = x + glyph->bearingX * fontScale;
          float gy = y + glyph->bearingTop * fontScale;
          addUvQuad(gx,
                    gy,
                    glyph->width * fontScale,
                    glyph->height * fontScale,
                    color,
                    {glyph->uMin, glyph->vMin},
                    {glyph->uMax, glyph->vMin},
                    {glyph->uMax, glyph->vMax},
                    {glyph->uMin, glyph->vMax});
        }
        x += std::max(glyph->advance, glyph->width + 1.0f) * fontScale;
        continue;
      }

      drawBitmapGlyph(cp, x, y, pixel, color);
      x += bitmapAdvanceForCodepoint(cp, pixel);
    }
  };

  auto drawTopLeftInfo = [&]() {
    const float versionPixel = 2.2f;
    const float textX = 4.0f;
    const float textY = 4.0f;
    drawText(kSnapshotVersionLabel,
             textX,
             textY,
             versionPixel,
             glm::vec3(0.82f, 0.88f, 0.96f),
             false);
  };

  auto drawHealthBar = [&]() {
    if (screenState != ScreenState::kPlaying || isCreativeMode()) {
      return;
    }

    constexpr uint8_t kHeartRows[5] = {
      0b01010,
      0b11111,
      0b11111,
      0b01110,
      0b00100
    };
    constexpr int kHeartsPerRow = 10;
    constexpr int kHeartCount = kMaxPlayerHealth / 2;

    auto drawHeart = [&](float x, float y, float pixel, int fillUnits) {
      for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
          int bit = 4 - col;
          if ((kHeartRows[row] & (1u << bit)) == 0) {
            continue;
          }

          addSolidQuad(x + static_cast<float>(col) * pixel,
                       y + static_cast<float>(row) * pixel,
                       pixel,
                       pixel,
                       glm::vec3(0.22f, 0.08f, 0.10f));

          bool filled = fillUnits >= 2 || (fillUnits == 1 && col <= 2);
          if (filled) {
            addSolidQuad(x + static_cast<float>(col) * pixel,
                         y + static_cast<float>(row) * pixel,
                         pixel,
                         pixel,
                         glm::vec3(0.92f, 0.18f, 0.24f));
          }
        }
      }
    };

    const float pixel = 2.0f;
    const float heartWidth = pixel * 5.0f;
    const float heartHeight = pixel * 5.0f;
    const float gap = 2.0f;
    const float rowGap = 4.0f;
    const float baseX = startX;
    const float bottomRowY = startY - heartHeight - 10.0f;
    for (int heart = 0; heart < kHeartCount; ++heart) {
      int row = heart / kHeartsPerRow;
      int col = heart % kHeartsPerRow;
      int fillUnits = std::clamp(playerHealth - heart * 2, 0, 2);
      drawHeart(baseX + static_cast<float>(col) * (heartWidth + gap),
                bottomRowY - static_cast<float>(row) * (heartHeight + rowGap),
                pixel,
                fillUnits);
    }
  };

  drawTopLeftInfo();
  drawHealthBar();

  if (screenState != ScreenState::kPlaying) {
    const bool ruUi = appliedSettings.language == 1;
    float t = static_cast<float>(glfwGetTime());
    float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
    float pulse = 0.5f + 0.5f * std::sin(t * 6.5f);
    bool settingsScreen = screenState == ScreenState::kSettings;
    float panelWidth = settingsScreen
      ? std::min(uiW * 0.82f, 860.0f)
      : std::min(uiW * 0.72f, 640.0f);
    float panelHeight = settingsScreen
      ? std::min(uiH * 0.80f, 560.0f)
      : std::min(uiH * 0.78f, 540.0f);
    float panelX = (uiW - panelWidth) * 0.5f;
    float panelY = (uiH - panelHeight) * 0.5f;
    panelY += (1.0f - intro) * 28.0f;
    panelY += std::sin(t * 1.4f) * 3.0f;

    addQuad(panelX, panelY, panelWidth, panelHeight, glm::vec3(0.10f, 0.10f, 0.13f), backgroundTile);
    addQuad(panelX + 8.0f,
            panelY + 8.0f,
            panelWidth - 16.0f,
            40.0f,
            glm::vec3(0.16f, 0.19f, 0.24f),
            backgroundTile);

    auto drawMenuRow = [&](int index,
                           int selectedIndex,
                           float x,
                           float y,
                           float w,
                           float h,
                           const glm::vec3& baseColor) {
      glm::vec3 rowColor = baseColor;
      if (index == selectedIndex) {
        float focus = 0.35f + pulse * 0.25f;
        rowColor = baseColor + glm::vec3(0.10f, 0.13f, 0.21f) + glm::vec3(focus * 0.2f);
        addQuad(x - 5.0f,
                y - 5.0f,
                w + 10.0f,
                h + 10.0f,
                glm::vec3(0.78f, 0.85f, 0.95f),
                backgroundTile);
        addQuad(x - 16.0f,
                y + h * 0.5f - 3.0f,
                8.0f + pulse * 6.0f,
                6.0f,
                glm::vec3(0.84f, 0.90f, 0.98f),
                backgroundTile);
      }
      addQuad(x, y, w, h, rowColor, backgroundTile);
    };

    if (screenState == ScreenState::kMainMenu) {
      drawText(ruUi ? "ГЛАВНОЕ МЕНЮ" : "MAIN MENU",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);
      float totalH = kMenuButtonHeight * 3.0f + kMenuButtonGap * 2.0f;
      float bx = panelX + (panelWidth - kMenuButtonWidth) * 0.5f;
      float by = panelY + 90.0f + (panelHeight - 130.0f - totalH) * 0.5f;
      const char* menuLabels[3] = {
        ruUi ? "НАЧАТЬ" : "START",
        ruUi ? "НАСТРОЙКИ" : "SETTINGS",
        ruUi ? "ВЫХОД" : "QUIT"
      };
      for (int i = 0; i < 3; ++i) {
        drawMenuRow(i,
                    mainMenuSelection,
                    bx,
                    by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap),
                    kMenuButtonWidth,
                    kMenuButtonHeight,
                    glm::vec3(0.22f, 0.24f, 0.29f));
        drawText(menuLabels[i],
                 bx + kMenuButtonWidth * 0.5f,
                 by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap) + 12.0f,
                 3.1f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 true);
      }
    } else if (screenState == ScreenState::kPaused) {
      drawText(ruUi ? "ПАУЗА" : "GAME PAUSED",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);
      float totalH = kMenuButtonHeight * 3.0f + kMenuButtonGap * 2.0f;
      float bx = panelX + (panelWidth - kMenuButtonWidth) * 0.5f;
      float by = panelY + 90.0f + (panelHeight - 130.0f - totalH) * 0.5f;
      const char* pauseLabels[3] = {
        ruUi ? "ПРОДОЛЖИТЬ" : "CONTINUE",
        ruUi ? "НАСТРОЙКИ" : "SETTINGS",
        ruUi ? "В МЕНЮ" : "MAIN MENU"
      };
      for (int i = 0; i < 3; ++i) {
        glm::vec3 rowColor = glm::vec3(0.22f, 0.24f, 0.29f);
        if (i == 2) {
          rowColor = glm::vec3(0.32f, 0.22f, 0.22f);
        }
        drawMenuRow(i,
                    pauseMenuSelection,
                    bx,
                    by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap),
                    kMenuButtonWidth,
                    kMenuButtonHeight,
                    rowColor);
        drawText(pauseLabels[i],
                 bx + kMenuButtonWidth * 0.5f,
                 by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap) + 12.0f,
                 3.1f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 true);
      }
    } else if (screenState == ScreenState::kDeath) {
      drawText(ruUi ? "ВЫ ПОГИБЛИ" : "YOU DIED",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.96f, 0.40f, 0.40f),
               true);
      drawText((ruUi ? "ЗДОРОВЬЕ 0/" : "HEALTH 0/") + std::to_string(kMaxPlayerHealth),
               panelX + panelWidth * 0.5f,
               panelY + 82.0f,
               1.9f,
               glm::vec3(0.90f, 0.84f, 0.84f),
               true);
      float totalH = kMenuButtonHeight * 2.0f + kMenuButtonGap;
      float bx = panelX + (panelWidth - kMenuButtonWidth) * 0.5f;
      float by = panelY + 116.0f + (panelHeight - 168.0f - totalH) * 0.5f;
      const char* deathLabels[2] = {
        ruUi ? "РЕСПАВН" : "RESPAWN",
        ruUi ? "В МЕНЮ" : "MAIN MENU"
      };
      for (int i = 0; i < 2; ++i) {
        glm::vec3 rowColor = i == 0 ? glm::vec3(0.20f, 0.36f, 0.22f) : glm::vec3(0.32f, 0.22f, 0.22f);
        drawMenuRow(i,
                    deathMenuSelection,
                    bx,
                    by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap),
                    kMenuButtonWidth,
                    kMenuButtonHeight,
                    rowColor);
        drawText(deathLabels[i],
                 bx + kMenuButtonWidth * 0.5f,
                 by + static_cast<float>(i) * (kMenuButtonHeight + kMenuButtonGap) + 12.0f,
                 3.1f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 true);
      }
    } else if (screenState == ScreenState::kLoadingWorld) {
      drawText(ruUi ? "ЗАГРУЗКА МИРА" : "LOADING WORLD", panelX + panelWidth * 0.5f, panelY + 56.0f, 3.0f,
               glm::vec3(0.90f), true);

      float barW = panelWidth - 140.0f;
      float barH = 24.0f;
      float barX = panelX + (panelWidth - barW) * 0.5f;
      float barY = panelY + panelHeight * 0.5f - barH * 0.5f;
      addQuad(barX, barY, barW, barH, glm::vec3(0.20f, 0.23f, 0.29f), backgroundTile);

      float fillW = (barW - 4.0f) * std::clamp(loadingWorldProgress, 0.0f, 1.0f);
      addQuad(barX + 2.0f, barY + 2.0f, fillW, barH - 4.0f, glm::vec3(0.32f, 0.62f, 0.92f), backgroundTile);

      float stripeX = std::fmod(static_cast<float>(glfwGetTime()) * 80.0f, 18.0f);
      for (float sx = barX - stripeX; sx < barX + fillW; sx += 18.0f) {
        float x0 = std::max(sx, barX + 2.0f);
        float w = std::min(8.0f, barX + 2.0f + fillW - x0);
        if (w > 0.0f) {
          addQuad(x0, barY + 2.0f, w, barH - 4.0f, glm::vec3(0.48f, 0.78f, 0.98f), backgroundTile);
        }
      }

      int percent = static_cast<int>(std::round(std::clamp(loadingWorldProgress, 0.0f, 1.0f) * 100.0f));
      std::string pctText = std::to_string(percent) + " PCT";
      drawText(pctText,
               panelX + panelWidth * 0.5f,
               barY + barH + 16.0f,
               2.4f,
               glm::vec3(0.88f, 0.92f, 0.98f),
               true);

      std::string message = loadingWorldMessage.empty() ? (ruUi ? "ЗАГРУЗКА" : "LOADING")
                                                        : loadingWorldMessage;
      drawText(message,
               panelX + panelWidth * 0.5f,
               barY - 24.0f,
               2.5f,
               glm::vec3(0.84f, 0.88f, 0.95f),
               true);
    } else if (screenState == ScreenState::kWorldSelect) {
      drawText(ruUi ? "ВЫБОР МИРА" : "SELECT WORLD",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);

      int totalRows = static_cast<int>(worldSelectEntries.size()) + 2;
      int visibleRows = computeWorldSelectVisibleRows(static_cast<int>(uiH), totalRows);
      int startRow = 0;
      if (totalRows > visibleRows) {
        startRow = std::clamp(worldSelectScroll, 0, totalRows - visibleRows);
      }
      int endRow = std::min(totalRows, startRow + visibleRows);
      int drawRows = endRow - startRow;

      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 40.0f;
      float rowGap = 10.0f;
      float rowsH = static_cast<float>(drawRows) * rowH +
                    static_cast<float>(std::max(0, drawRows - 1)) * rowGap;
      float rowY = panelY + 84.0f + std::max(0.0f, (panelHeight - 116.0f - rowsH) * 0.5f);

      for (int row = startRow; row < endRow; ++row) {
        int drawIndex = row - startRow;
        glm::vec3 rowColor = glm::vec3(0.21f, 0.23f, 0.28f);
        if (row == 0) {
          rowColor = glm::vec3(0.20f, 0.36f, 0.22f);
        } else if (row == totalRows - 1) {
          rowColor = glm::vec3(0.36f, 0.22f, 0.22f);
        }

        drawMenuRow(row,
                    worldSelectSelection,
                    rowX,
                    rowY + static_cast<float>(drawIndex) * (rowH + rowGap),
                    rowW,
                    rowH,
                    rowColor);
      }

      for (int row = startRow; row < endRow; ++row) {
        std::string label;
        if (row == 0) {
          label = ruUi ? "СОЗДАТЬ МИР" : "CREATE NEW WORLD";
        } else if (row == totalRows - 1) {
          label = ruUi ? "НАЗАД В МЕНЮ" : "BACK TO MENU";
        } else {
          label = worldSelectEntries[static_cast<size_t>(row - 1)].displayName;
        }
        int drawIndex = row - startRow;
        drawText(label,
                 rowX + 14.0f,
                 rowY + static_cast<float>(drawIndex) * (rowH + rowGap) + 12.0f,
                 2.45f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 false);
      }

      if (totalRows > visibleRows) {
        std::string hint = (ruUi ? "СПИСОК " : "SCROLL ") +
                           std::to_string(startRow + 1) +
                           "-" +
                           std::to_string(endRow) +
                           "/" +
                           std::to_string(totalRows);
        drawText(hint,
                 panelX + panelWidth - 166.0f,
                 panelY + panelHeight - 22.0f,
                 1.8f,
                 glm::vec3(0.70f, 0.75f, 0.83f),
                 false);
      } else if (worldSelectEntries.empty()) {
        drawText(ruUi ? "МИРОВ ПОКА НЕТ" : "NO WORLDS YET",
                 panelX + panelWidth * 0.5f,
                 panelY + panelHeight - 24.0f,
                 2.0f,
                 glm::vec3(0.70f, 0.75f, 0.83f), true);
      }
    } else if (screenState == ScreenState::kSettings) {
      const bool ru = appliedSettings.language == 1;
      SettingsUiLayout settingsLayout = makeSettingsUiLayout(panelX, panelY, panelWidth, panelHeight);
      int activeCategory = std::clamp(settingsCategory, 0, kSettingsCategoryCount - 1);
      int optionCount = settingsEntryCountForCategory(activeCategory);
      int activeOption = std::clamp(settingsOptionSelection, 0, std::max(0, optionCount - 1));
      int activeAction = std::clamp(settingsActionSelection, 0, kSettingsActionCount - 1);
      SettingsFocusArea focusArea =
        static_cast<SettingsFocusArea>(std::clamp(settingsFocusArea,
                                                  static_cast<int>(SettingsFocusArea::kCategories),
                                                  static_cast<int>(SettingsFocusArea::kActions)));

      auto categoryLabel = [&](int category) -> std::string {
        switch (static_cast<SettingsCategoryTab>(category)) {
          case SettingsCategoryTab::kGraphics:
            return ru ? "ГРАФИКА" : "GRAPHICS";
          case SettingsCategoryTab::kAudio:
            return ru ? "ЗВУК" : "AUDIO";
          case SettingsCategoryTab::kControls:
            return ru ? "УПРАВЛЕНИЕ" : "CONTROLS";
          case SettingsCategoryTab::kInterface:
          default:
            return ru ? "ИНТЕРФЕЙС" : "INTERFACE";
        }
      };

      auto categoryHint = [&](int category) -> std::string {
        switch (static_cast<SettingsCategoryTab>(category)) {
          case SettingsCategoryTab::kGraphics:
            return ru ? "КАЧЕСТВО И МИР" : "VISUALS AND WORLD";
          case SettingsCategoryTab::kAudio:
            return ru ? "ГРОМКОСТЬ ИГРЫ" : "GAME VOLUME";
          case SettingsCategoryTab::kControls:
            return ru ? "МЫШЬ И КАМЕРА" : "MOUSE AND CAMERA";
          case SettingsCategoryTab::kInterface:
          default:
            return ru ? "МАСШТАБ И ЯЗЫК" : "SCALE AND LANGUAGE";
        }
      };

      auto entryTitle = [&](SettingsEntryId entry) -> std::string {
        switch (entry) {
          case SettingsEntryId::kGraphicsQuality:
            return ru ? "КАЧЕСТВО ГРАФИКИ" : "GRAPHICS QUALITY";
          case SettingsEntryId::kRenderDistance:
            return ru ? "ДАЛЬНОСТЬ ПРОРИСОВКИ" : "RENDER DISTANCE";
          case SettingsEntryId::kAudioVolume:
            return ru ? "ГРОМКОСТЬ" : "MASTER VOLUME";
          case SettingsEntryId::kSensitivity:
            return ru ? "ЧУВСТВИТЕЛЬНОСТЬ МЫШИ" : "MOUSE SENSITIVITY";
          case SettingsEntryId::kUiScale:
            return ru ? "МАСШТАБ ИНТЕРФЕЙСА" : "INTERFACE SCALE";
          case SettingsEntryId::kLanguage:
            return ru ? "ЯЗЫК" : "LANGUAGE";
          case SettingsEntryId::kBlockGuides:
          default:
            return ru ? "КОНТУРЫ БЛОКОВ" : "BLOCK GUIDES";
        }
      };

      auto entryHint = [&](SettingsEntryId entry) -> std::string {
        switch (entry) {
          case SettingsEntryId::kGraphicsQuality:
            return ru ? "ВЛИЯЕТ НА КАРТИНКУ И FPS" : "BALANCES LOOKS AND FPS";
          case SettingsEntryId::kRenderDistance:
            return ru ? "СКОЛЬКО ЧАНКОВ ВИДНО ВДАЛЬ" : "HOW FAR CHUNKS STAY VISIBLE";
          case SettingsEntryId::kAudioVolume:
            return ru ? "ОБЩАЯ ГРОМКОСТЬ ВСЕЙ ИГРЫ" : "OVERALL GAME SOUND LEVEL";
          case SettingsEntryId::kSensitivity:
            return ru ? "СКОРОСТЬ ПОВОРОТА КАМЕРЫ" : "CAMERA TURN SPEED";
          case SettingsEntryId::kUiScale:
            return ru ? "РАЗМЕР HUD, МЕНЮ И ИНВЕНТАРЯ" : "SIZE OF HUD, MENUS AND INVENTORY";
          case SettingsEntryId::kLanguage:
            return ru ? "ЯЗЫК МЕНЮ И ПОДПИСЕЙ" : "MENU AND UI LANGUAGE";
          case SettingsEntryId::kBlockGuides:
          default:
            return ru ? "КОНТУР И ПРЕВЬЮ ПОСТАНОВКИ" : "OUTLINE AND PLACEMENT PREVIEW";
        }
      };

      auto graphicsValueText = [&]() -> std::string {
        if (pendingSettings.graphicsQuality == 0) {
          return ru ? "НИЗКО" : "LOW";
        }
        if (pendingSettings.graphicsQuality == 2) {
          return ru ? "ВЫСОКО" : "HIGH";
        }
        return ru ? "СРЕДНЕ" : "MEDIUM";
      };

      auto valueText = [&](SettingsEntryId entry) -> std::string {
        switch (entry) {
          case SettingsEntryId::kGraphicsQuality:
            return graphicsValueText();
          case SettingsEntryId::kRenderDistance:
            return std::to_string(pendingSettings.renderDistance) + (ru ? " ЧАНКОВ" : " CHUNKS");
          case SettingsEntryId::kAudioVolume:
            return std::to_string(pendingSettings.audioVolume) + "%";
          case SettingsEntryId::kSensitivity: {
            std::ostringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(2);
            ss << pendingSettings.sensitivity;
            return ss.str();
          }
          case SettingsEntryId::kUiScale:
            return std::to_string(static_cast<int>(std::lround(pendingSettings.uiScale * 100.0f))) + "%";
          case SettingsEntryId::kLanguage:
            return pendingSettings.language == 1
              ? (ru ? "РУССКИЙ" : "RUSSIAN")
              : (ru ? "АНГЛИЙСКИЙ" : "ENGLISH");
          case SettingsEntryId::kBlockGuides:
          default:
            return pendingSettings.blockGuides
              ? (ru ? "ВКЛЮЧЕНО" : "ENABLED")
              : (ru ? "ВЫКЛЮЧЕНО" : "DISABLED");
        }
      };

      auto actionLabel = [&](int action) -> std::string {
        switch (static_cast<SettingsActionId>(action)) {
          case SettingsActionId::kApply:
            return ru ? "СОХРАНИТЬ И ПРИМЕНИТЬ" : "SAVE AND APPLY";
          case SettingsActionId::kReset:
            return ru ? "СБРОСИТЬ" : "RESET";
          case SettingsActionId::kBack:
          default:
            return settingsReturnState == ScreenState::kPaused
              ? (ru ? "НАЗАД В ПАУЗУ" : "BACK TO PAUSE")
              : (ru ? "НАЗАД В МЕНЮ" : "BACK TO MENU");
        }
      };

      auto sliderNormalized = [&](SettingsEntryId entry) -> float {
        switch (entry) {
          case SettingsEntryId::kRenderDistance:
            return static_cast<float>(pendingSettings.renderDistance - kMinRenderDistance) /
                   static_cast<float>(kMaxRenderDistance - kMinRenderDistance);
          case SettingsEntryId::kAudioVolume:
            return static_cast<float>(pendingSettings.audioVolume) / 100.0f;
          case SettingsEntryId::kSensitivity:
            return (pendingSettings.sensitivity - 0.03f) / (0.40f - 0.03f);
          case SettingsEntryId::kUiScale:
            return (pendingSettings.uiScale - kMinUiScale) / (kMaxUiScale - kMinUiScale);
          default:
            return 0.0f;
        }
      };

      int hoveredCategory = -1;
      for (int i = 0; i < kSettingsCategoryCount; ++i) {
        if (pointInRect(cursorFbX, cursorFbY, settingsCategoryRect(settingsLayout, i))) {
          hoveredCategory = i;
          break;
        }
      }

      int hoveredOption = -1;
      for (int i = 0; i < optionCount; ++i) {
        if (pointInRect(cursorFbX, cursorFbY, settingsOptionRect(settingsLayout, i))) {
          hoveredOption = i;
          break;
        }
      }

      int hoveredAction = -1;
      for (int i = 0; i < kSettingsActionCount; ++i) {
        if (pointInRect(cursorFbX, cursorFbY, settingsActionRect(settingsLayout, i))) {
          hoveredAction = i;
          break;
        }
      }

      addQuad(settingsLayout.sidebarX - 10.0f,
              settingsLayout.sidebarY - 10.0f,
              settingsLayout.sidebarW + 20.0f,
              settingsLayout.panelH - 140.0f,
              glm::vec3(0.16f, 0.18f, 0.22f),
              backgroundTile);
      addQuad(settingsLayout.contentX - 10.0f,
              settingsLayout.contentY - 10.0f,
              settingsLayout.contentW + 20.0f,
              settingsLayout.panelH - 140.0f,
              glm::vec3(0.14f, 0.16f, 0.20f),
              backgroundTile);

      drawText(ru ? "НАСТРОЙКИ" : "SETTINGS",
               panelX + 28.0f,
               panelY + 20.0f,
               3.1f,
               glm::vec3(0.92f, 0.94f, 0.98f),
               false);
      drawText(settingsDirty ? (ru ? "ЕСТЬ НЕСОХРАНЕННЫЕ ИЗМЕНЕНИЯ" : "UNSAVED CHANGES")
                             : (ru ? "ВСЕ ИЗМЕНЕНИЯ СОХРАНЕНЫ" : "ALL CHANGES SAVED"),
               panelX + panelWidth - 28.0f,
               panelY + 22.0f,
               1.9f,
               settingsDirty ? glm::vec3(0.95f, 0.75f, 0.42f) : glm::vec3(0.70f, 0.84f, 0.72f),
               true);
      drawText(ru ? "КАТЕГОРИИ" : "CATEGORIES",
               settingsLayout.sidebarX,
               settingsLayout.sidebarY - 26.0f,
               1.9f,
               glm::vec3(0.72f, 0.78f, 0.86f),
               false);
      drawText(categoryLabel(activeCategory),
               settingsLayout.contentX,
               settingsLayout.contentY - 28.0f,
               2.6f,
               glm::vec3(0.90f, 0.92f, 0.98f),
               false);

      for (int i = 0; i < kSettingsCategoryCount; ++i) {
        glm::vec4 rect = settingsCategoryRect(settingsLayout, i);
        bool active = i == activeCategory;
        bool focused = focusArea == SettingsFocusArea::kCategories && i == activeCategory;
        bool hovered = i == hoveredCategory;
        glm::vec3 color = active
          ? glm::vec3(0.26f, 0.34f, 0.46f)
          : glm::vec3(0.20f, 0.22f, 0.28f);
        if (hovered) {
          color += glm::vec3(0.04f, 0.05f, 0.07f);
        }
        if (focused) {
          addQuad(rect.x - 4.0f, rect.y - 4.0f, rect.z + 8.0f, rect.w + 8.0f,
                  glm::vec3(0.82f, 0.88f, 0.96f), backgroundTile);
        }
        addQuad(rect.x, rect.y, rect.z, rect.w, color, backgroundTile);
        drawText(categoryLabel(i),
                 rect.x + 14.0f,
                 rect.y + 9.0f,
                 2.35f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 false);
        drawText(categoryHint(i),
                 rect.x + 14.0f,
                 rect.y + 27.0f,
                 1.55f,
                 active ? glm::vec3(0.78f, 0.86f, 0.95f) : glm::vec3(0.62f, 0.68f, 0.76f),
                 false);
      }

      auto drawChoiceChips = [&](const glm::vec4& controlRect,
                                 const std::vector<std::string>& labels,
                                 int selectedIndex,
                                 int hoveredIndex,
                                 const glm::vec3& accent) {
        int segments = static_cast<int>(labels.size());
        for (int i = 0; i < segments; ++i) {
          glm::vec4 seg = settingsSegmentRect(controlRect, segments, i);
          bool selected = i == selectedIndex;
          bool hovered = i == hoveredIndex;
          glm::vec3 color = selected ? accent : glm::vec3(0.22f, 0.24f, 0.30f);
          if (hovered && !selected) {
            color += glm::vec3(0.05f);
          }
          addQuad(seg.x, seg.y, seg.z, seg.w, color, backgroundTile);
          drawText(labels[static_cast<size_t>(i)],
                   seg.x + seg.z * 0.5f,
                   seg.y + 8.0f,
                   1.95f,
                   glm::vec3(0.95f, 0.96f, 0.99f),
                   true);
        }
      };

      for (int optionIndex = 0; optionIndex < optionCount; ++optionIndex) {
        SettingsEntryId entry = settingsEntryForCategory(activeCategory, optionIndex);
        glm::vec4 rect = settingsOptionRect(settingsLayout, optionIndex);
        bool selected = focusArea == SettingsFocusArea::kOptions && optionIndex == activeOption;
        bool hovered = optionIndex == hoveredOption;
        if (selected) {
          addQuad(rect.x - 5.0f, rect.y - 5.0f, rect.z + 10.0f, rect.w + 10.0f,
                  glm::vec3(0.80f, 0.86f, 0.95f), backgroundTile);
        }
        glm::vec3 rowColor = glm::vec3(0.20f, 0.23f, 0.29f);
        if (hovered) {
          rowColor += glm::vec3(0.04f, 0.05f, 0.07f);
        }
        addQuad(rect.x, rect.y, rect.z, rect.w, rowColor, backgroundTile);

        drawText(entryTitle(entry),
                 rect.x + 16.0f,
                 rect.y + 10.0f,
                 2.35f,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 false);
        drawText(entryHint(entry),
                 rect.x + 16.0f,
                 rect.y + 32.0f,
                 1.55f,
                 glm::vec3(0.68f, 0.74f, 0.82f),
                 false);
        drawText(valueText(entry),
                 rect.x + rect.z - 148.0f,
                 rect.y + 10.0f,
                 1.9f,
                 glm::vec3(0.88f, 0.92f, 0.98f),
                 false);

        glm::vec4 controlRect = settingsControlRect(rect);
        if (entry == SettingsEntryId::kGraphicsQuality) {
          int hoveredSegment = -1;
          for (int i = 0; i < 3; ++i) {
            if (pointInRect(cursorFbX, cursorFbY, settingsSegmentRect(controlRect, 3, i))) {
              hoveredSegment = i;
              break;
            }
          }
          drawChoiceChips(controlRect,
                          {ru ? "НИЗКО" : "LOW", ru ? "СРЕДНЕ" : "MED", ru ? "ВЫСОКО" : "HIGH"},
                          pendingSettings.graphicsQuality,
                          hoveredSegment,
                          glm::vec3(0.30f, 0.47f, 0.74f));
        } else if (entry == SettingsEntryId::kLanguage) {
          int hoveredSegment = -1;
          for (int i = 0; i < 2; ++i) {
            if (pointInRect(cursorFbX, cursorFbY, settingsSegmentRect(controlRect, 2, i))) {
              hoveredSegment = i;
              break;
            }
          }
          drawChoiceChips(controlRect,
                          {ru ? "ENGLISH" : "ENGLISH", ru ? "РУССКИЙ" : "RUSSIAN"},
                          pendingSettings.language,
                          hoveredSegment,
                          glm::vec3(0.36f, 0.42f, 0.72f));
        } else if (entry == SettingsEntryId::kBlockGuides) {
          int hoveredSegment = -1;
          for (int i = 0; i < 2; ++i) {
            if (pointInRect(cursorFbX, cursorFbY, settingsSegmentRect(controlRect, 2, i))) {
              hoveredSegment = i;
              break;
            }
          }
          drawChoiceChips(controlRect,
                          {ru ? "ВЫКЛ" : "OFF", ru ? "ВКЛ" : "ON"},
                          pendingSettings.blockGuides ? 1 : 0,
                          hoveredSegment,
                          glm::vec3(0.26f, 0.52f, 0.32f));
        } else {
          glm::vec4 sliderRect = settingsSliderTrackRect(rect);
          addQuad(sliderRect.x,
                  sliderRect.y,
                  sliderRect.z,
                  sliderRect.w,
                  glm::vec3(0.14f, 0.15f, 0.19f),
                  backgroundTile);
          float fill = sliderRect.z * std::clamp(sliderNormalized(entry), 0.0f, 1.0f);
          addQuad(sliderRect.x,
                  sliderRect.y,
                  fill,
                  sliderRect.w,
                  glm::vec3(0.38f, 0.66f, 0.94f),
                  backgroundTile);
          float knobX = sliderRect.x + fill - 8.0f;
          addQuad(knobX,
                  sliderRect.y - 5.0f,
                  16.0f,
                  sliderRect.w + 10.0f,
                  glm::vec3(0.92f, 0.95f, 0.99f),
                  backgroundTile);
        }
      }

      for (int actionIndex = 0; actionIndex < kSettingsActionCount; ++actionIndex) {
        glm::vec4 rect = settingsActionRect(settingsLayout, actionIndex);
        bool selected = focusArea == SettingsFocusArea::kActions && actionIndex == activeAction;
        bool hovered = actionIndex == hoveredAction;
        glm::vec3 color = glm::vec3(0.22f, 0.24f, 0.30f);
        if (actionIndex == static_cast<int>(SettingsActionId::kApply)) {
          color = glm::vec3(0.22f, 0.40f, 0.26f);
        } else if (actionIndex == static_cast<int>(SettingsActionId::kReset)) {
          color = glm::vec3(0.44f, 0.32f, 0.18f);
        } else if (settingsReturnState == ScreenState::kPaused) {
          color = glm::vec3(0.28f, 0.24f, 0.36f);
        } else {
          color = glm::vec3(0.36f, 0.22f, 0.22f);
        }
        if (hovered) {
          color += glm::vec3(0.04f, 0.05f, 0.06f);
        }
        if (selected) {
          addQuad(rect.x - 4.0f, rect.y - 4.0f, rect.z + 8.0f, rect.w + 8.0f,
                  glm::vec3(0.82f, 0.88f, 0.96f), backgroundTile);
        }
        addQuad(rect.x, rect.y, rect.z, rect.w, color, backgroundTile);
        drawText(actionLabel(actionIndex),
                 rect.x + rect.z * 0.5f,
                 rect.y + 10.0f,
                 2.1f,
                 glm::vec3(0.96f, 0.97f, 0.99f),
                 true);
      }

      drawText(ru ? "КЛИК ПО КАТЕГОРИИ ОТКРЫВАЕТ ЕЕ НАСТРОЙКИ" : "CLICK A CATEGORY TO OPEN ITS SETTINGS",
               settingsLayout.contentX,
               settingsLayout.panelY + settingsLayout.panelH - 82.0f,
               1.55f,
               glm::vec3(0.68f, 0.74f, 0.82f),
               false);
      drawText(ru ? "ПОЛЗУНКИ МОЖНО ТЯНУТЬ МЫШКОЙ" : "SLIDERS CAN BE DRAGGED WITH THE MOUSE",
               settingsLayout.contentX,
               settingsLayout.panelY + settingsLayout.panelH - 64.0f,
               1.55f,
               glm::vec3(0.68f, 0.74f, 0.82f),
               false);
    } else if (screenState == ScreenState::kCreateWorld) {
      drawText(ruUi ? "СОЗДАТЬ МИР" : "CREATE WORLD",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);
      constexpr int kRowCount = 9;
      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 36.0f;
      float rowGap = 12.0f;
      float rowsH = static_cast<float>(kRowCount) * rowH + static_cast<float>(kRowCount - 1) * rowGap;
      float rowY = panelY + 72.0f + std::max(0.0f, (panelHeight - 92.0f - rowsH) * 0.5f);

      for (int i = 0; i < kRowCount; ++i) {
        glm::vec3 rowColor = glm::vec3(0.20f, 0.23f, 0.29f);
        if (i == 7) {
          rowColor = glm::vec3(0.20f, 0.36f, 0.22f);
        } else if (i == 8) {
          rowColor = glm::vec3(0.36f, 0.22f, 0.22f);
        }
        drawMenuRow(i,
                    createWorldSelection,
                    rowX,
                    rowY + static_cast<float>(i) * (rowH + rowGap),
                    rowW,
                    rowH,
                    rowColor);
      }

      float presetY = rowY + 2.0f * (rowH + rowGap);
      constexpr float kPresetOptionGap = 8.0f;
      float optionW = 82.0f;
      float optionH = rowH - 12.0f;
      float optionY = presetY + 6.0f;
      float totalPresetWidth =
        optionW * static_cast<float>(kCreateWorldPresetOptions.size()) +
        kPresetOptionGap * static_cast<float>(kCreateWorldPresetOptions.size() - 1);
      float firstOptionX = rowX + rowW - totalPresetWidth - 10.0f;
      for (size_t i = 0; i < kCreateWorldPresetOptions.size(); ++i) {
        WorldPreset preset = kCreateWorldPresetOptions[i];
        bool selected = pendingWorldSettings.preset == preset;
        float optionX = firstOptionX + static_cast<float>(i) * (optionW + kPresetOptionGap);
        addQuad(optionX,
                optionY,
                optionW,
                optionH,
                worldPresetButtonColor(preset, selected),
                backgroundTile);
      }

      auto drawSlider = [&](float y, float value) {
        float t = (value - 0.25f) / (2.5f - 0.25f);
        t = std::clamp(t, 0.0f, 1.0f);
        float trackW = 240.0f;
        float trackH = 12.0f;
        float tx = rowX + rowW - trackW - 16.0f;
        float ty = y + (rowH - trackH) * 0.5f;
        addQuad(tx, ty, trackW, trackH, glm::vec3(0.16f, 0.17f, 0.20f), backgroundTile);
        addQuad(tx + 1.0f,
                ty + 1.0f,
                (trackW - 2.0f) * t,
                trackH - 2.0f,
                glm::vec3(0.44f, 0.63f, 0.92f),
                backgroundTile);
      };

      drawSlider(rowY + 3.0f * (rowH + rowGap), pendingWorldSettings.caveDensity);
      drawSlider(rowY + 4.0f * (rowH + rowGap), pendingWorldSettings.ravineFrequency);

      float toggleW = 108.0f;
      float toggleH = rowH - 12.0f;
      float toggleX = rowX + rowW - toggleW - 16.0f;
      float modeY = rowY + 5.0f * (rowH + rowGap) + 6.0f;
      addQuad(toggleX,
              modeY,
              toggleW,
              toggleH,
              pendingWorldSettings.startInventoryMode == 0
                ? glm::vec3(0.24f, 0.40f, 0.24f)
                : glm::vec3(0.40f, 0.28f, 0.22f),
              backgroundTile);
      float cheatsY = rowY + 6.0f * (rowH + rowGap) + 6.0f;
      addQuad(toggleX,
              cheatsY,
              toggleW,
              toggleH,
              pendingWorldSettings.cheatsEnabled
                ? glm::vec3(0.24f, 0.40f, 0.24f)
                : glm::vec3(0.36f, 0.24f, 0.24f),
              backgroundTile);

      std::string nameText = pendingWorldName.empty() ? (ruUi ? "МИР" : "WORLD") : pendingWorldName;
      std::string seedText = pendingSeedText.empty() ? (ruUi ? "СЛУЧАЙНО" : "RANDOM") : pendingSeedText;
      std::string presetText = worldPresetDisplayName(pendingWorldSettings.preset, ruUi);
      std::ostringstream caveText;
      caveText.setf(std::ios::fixed);
      caveText.precision(2);
      caveText << pendingWorldSettings.caveDensity;
      std::ostringstream ravineText;
      ravineText.setf(std::ios::fixed);
      ravineText.precision(2);
      ravineText << pendingWorldSettings.ravineFrequency;
      std::string modeText = pendingWorldSettings.startInventoryMode == 0
        ? (ruUi ? "ВЫЖИВАНИЕ" : "SURVIVAL")
        : (ruUi ? "КРЕАТИВ" : "CREATIVE");
      std::string cheatsText = pendingWorldSettings.cheatsEnabled
        ? (ruUi ? "ВКЛ" : "ON")
        : (ruUi ? "ВЫКЛ" : "OFF");

      std::array<std::string, 9> labels = {
        (ruUi ? "ИМЯ МИРА " : "WORLD NAME ") + nameText,
        (ruUi ? "СИД " : "SEED ") + seedText,
        (ruUi ? "ПРЕСЕТ " : "PRESET ") + presetText,
        (ruUi ? "ПЛОТНОСТЬ ПЕЩЕР " : "CAVE DENSITY ") + caveText.str(),
        (ruUi ? "ЧАСТОТА ОВРАГОВ " : "RAVINE FREQ ") + ravineText.str(),
        (ruUi ? "РЕЖИМ " : "GAME MODE ") + modeText,
        (ruUi ? "ЧИТЫ " : "CHEATS ") + cheatsText,
        ruUi ? "СОЗДАТЬ МИР" : "CREATE WORLD",
        ruUi ? "ОТМЕНА" : "CANCEL"
      };

      for (int i = 0; i < kRowCount; ++i) {
        float px = (i >= 7) ? 2.9f : 2.45f;
        drawText(labels[static_cast<size_t>(i)],
                 rowX + 14.0f,
                 rowY + static_cast<float>(i) * (rowH + rowGap) + 11.0f,
                 px,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 false);
      }

      drawText(modeText,
               toggleX + toggleW * 0.5f,
               modeY + 9.0f,
               1.55f,
               glm::vec3(0.96f, 0.97f, 0.99f),
               true);
      for (size_t i = 0; i < kCreateWorldPresetOptions.size(); ++i) {
        WorldPreset preset = kCreateWorldPresetOptions[i];
        float optionX = firstOptionX + static_cast<float>(i) * (optionW + kPresetOptionGap);
        float textScale = preset == WorldPreset::kAprilFools ? 1.10f : 1.30f;
        drawText(worldPresetButtonLabel(preset, ruUi),
                 optionX + optionW * 0.5f,
                 optionY + 9.0f,
                 textScale,
                 glm::vec3(0.96f, 0.97f, 0.99f),
                 true);
      }
      drawText(cheatsText,
               toggleX + toggleW * 0.5f,
               cheatsY + 9.0f,
               1.55f,
               glm::vec3(0.96f, 0.97f, 0.99f),
               true);
    }

    return;
  }

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

  auto shouldRenderBlockIcon3d = [&](uint8_t type) {
    return isBlockType(type) &&
           !isWaterBlock(type) &&
           !isDecorationBlock(type) &&
           type != kBed;
  };

  auto blockIconTiles = [&](uint8_t type) {
    std::array<int, 3> tiles{tileForBlock(type), tileForBlock(type), tileForBlock(type)};
    switch (type) {
      case kGrass:
        tiles = {kTileGrassTop, kTileGrassSide, kTileGrassSide};
        break;
      case kWood:
        tiles = {kTileWoodTop, kTileWood, kTileWood};
        break;
      case kFurnace:
      case kFurnaceNorth:
      case kFurnaceEast:
      case kFurnaceSouth:
      case kFurnaceWest:
        tiles = {kTileFurnace, kTileFurnaceFront, kTileFurnace};
        break;
      default:
        break;
    }
    return tiles;
  };

  auto drawBlockIcon3d = [&](uint8_t type, float x, float y) {
    std::array<int, 3> tiles = blockIconTiles(type);
    float innerX = x + 4.0f;
    float innerY = y + 4.0f;
    float innerW = kSlotSize - 8.0f;
    float innerH = kSlotSize - 8.0f;

    float frontW = innerW * 0.58f;
    float frontH = innerH * 0.58f;
    float depthX = innerW * 0.24f;
    float depthY = innerH * 0.18f;
    float frontX = innerX + 2.0f;
    float frontY = innerY + depthY + 1.5f;

    glm::vec2 front0(frontX, frontY);
    glm::vec2 front1(frontX + frontW, frontY);
    glm::vec2 front2(frontX + frontW, frontY + frontH);
    glm::vec2 front3(frontX, frontY + frontH);

    glm::vec2 top0(front0.x + depthX, front0.y - depthY);
    glm::vec2 top1(front1.x + depthX, front1.y - depthY);
    glm::vec2 right1(front1.x + depthX, front1.y - depthY);
    glm::vec2 right2(front2.x + depthX, front2.y - depthY);

    addPointTileQuad(top0,
                     top1,
                     front1,
                     front0,
                     glm::vec3(1.10f, 1.10f, 1.10f),
                     tiles[0]);
    addPointTileQuad(front0,
                     front1,
                     front2,
                     front3,
                     glm::vec3(1.0f),
                     tiles[1]);
    addPointTileQuad(front1,
                     right1,
                     right2,
                     front2,
                     glm::vec3(0.76f, 0.76f, 0.76f),
                     tiles[2]);
  };

  auto drawToolIcon = [&](uint8_t type, float x, float y) {
    int tile = tileForBlock(type);
    float innerX = x + 3.0f;
    float innerY = y + 3.0f;
    float innerW = kSlotSize - 6.0f;
    float innerH = kSlotSize - 6.0f;
    float angle = glm::radians(-38.0f);
    float halfW = innerW * 0.44f;
    float halfH = innerH * 0.44f;
    glm::vec2 center(innerX + innerW * 0.54f, innerY + innerH * 0.54f);

    auto orientedRect = [&](glm::vec2 localCenter) {
      glm::vec2 right(std::cos(angle) * halfW, std::sin(angle) * halfW);
      glm::vec2 down(-std::sin(angle) * halfH, std::cos(angle) * halfH);
      return std::array<glm::vec2, 4>{{
        localCenter - right - down,
        localCenter + right - down,
        localCenter + right + down,
        localCenter - right + down
      }};
    };

    auto shadow = orientedRect(center + glm::vec2(1.6f, 1.6f));
    addPointTileQuad(shadow[0],
                     shadow[1],
                     shadow[2],
                     shadow[3],
                     glm::vec3(0.18f, 0.18f, 0.20f),
                     tile);

    auto icon = orientedRect(center);
    addPointTileQuad(icon[0],
                     icon[1],
                     icon[2],
                     icon[3],
                     glm::vec3(1.0f),
                     tile);
  };

  auto drawStack = [&](const ItemStack& stack,
                       float x,
                       float y,
                       const glm::vec3& countColor = glm::vec3(0.95f, 0.95f, 0.98f),
                       int countTile = -1) {
    if (stack.count == 0 || stack.type == kAir) {
      return;
    }
    if (countTile < 0) {
      countTile = backgroundTile;
    }
    if (shouldRenderBlockIcon3d(stack.type)) {
      drawBlockIcon3d(stack.type, x, y);
    } else if (isToolItem(stack.type)) {
      drawToolIcon(stack.type, x, y);
    } else {
      int tile = tileForBlock(stack.type);
      addQuad(x + kIconPadding,
              y + kIconPadding,
              kSlotSize - kIconPadding * 2.0f,
              kSlotSize - kIconPadding * 2.0f,
              glm::vec3(1.0f),
              tile);
    }
    drawNumber(static_cast<int>(stack.count),
               x + kSlotSize - 4.0f,
               y + kSlotSize - 4.0f,
               3.0f,
               countColor,
               countTile);
  };

  if (inventoryOpen) {
    const float gridWidth =
      kSlotSize * static_cast<float>(kInventoryCols) +
      kSlotPadding * static_cast<float>(kInventoryCols - 1);
    const float gridHeight =
      kSlotSize * static_cast<float>(kInventoryRows) +
      kSlotPadding * static_cast<float>(kInventoryRows - 1);

    float gridX = (uiW - gridWidth) * 0.5f;
    float gridY = (uiH - gridHeight) * 0.5f - 30.0f;
    gridY = std::clamp(gridY, 20.0f, uiH - gridHeight - 20.0f);

    addQuad(gridX - kPanelPadding,
            gridY - kPanelPadding,
            gridWidth + kPanelPadding * 2.0f,
            gridHeight + kPanelPadding * 2.0f,
            glm::vec3(0.15f, 0.15f, 0.18f),
            backgroundTile);
    bool creativeInventoryScreen = isCreativeInventoryScreen();
    if (furnaceOpen) {
      FurnaceUiLayout furnaceLayout = makeFurnaceUiLayout(uiLayoutWidth(), uiLayoutHeight());
      FurnaceState furnaceState{};
      auto it = furnaceStates.find(furnaceKeyForBlock(activeFurnaceBlock));
      if (it != furnaceStates.end()) {
        furnaceState = it->second;
      }
      float smeltFill = std::clamp(furnaceState.smeltProgress / kFurnaceSmeltDuration, 0.0f, 1.0f);
      float burnFill = furnaceState.burnDuration > 0.0f
        ? std::clamp(furnaceState.burnTime / furnaceState.burnDuration, 0.0f, 1.0f)
        : 0.0f;
      int smeltPercent = static_cast<int>(std::lround(smeltFill * 100.0f));
      int burnPercent = static_cast<int>(std::lround(burnFill * 100.0f));
      bool furnaceReady = furnaceState.output.count > 0 && furnaceState.output.type != kAir;

      addQuad(furnaceLayout.panelX,
              furnaceLayout.panelY,
              furnaceLayout.panelWidth,
              furnaceLayout.panelHeight,
              glm::vec3(0.13f, 0.14f, 0.17f),
              backgroundTile);
      drawText(appliedSettings.language == 1 ? "ПЕЧКА" : "FURNACE",
               furnaceLayout.inputX,
               furnaceLayout.panelY + 4.0f,
               2.1f,
               glm::vec3(0.92f, 0.94f, 0.98f),
               false);
      drawText(appliedSettings.language == 1 ? "ЖЕЛЕЗО ИЗ РУДЫ" : "SMELT IRON ORE",
               furnaceLayout.infoX,
               furnaceLayout.panelY + 4.0f,
               1.8f,
               glm::vec3(0.76f, 0.84f, 0.93f),
               false);
      drawText(appliedSettings.language == 1 ? "ТОПЛИВО: УГОЛЬ/ДЕРЕВО" : "FUEL: COAL/WOOD",
               furnaceLayout.infoX,
               furnaceLayout.infoY + 16.0f,
               1.55f,
               glm::vec3(0.86f, 0.84f, 0.66f),
               false);
      drawText(appliedSettings.language == 1 ? "ЛКМ ПЕРЕНОС ПКМ 1" : "LMB MOVE RMB 1",
               furnaceLayout.infoX,
               furnaceLayout.infoY + 34.0f,
               1.55f,
               glm::vec3(0.64f, 0.88f, 0.66f),
               false);
      if (furnaceReady) {
        drawText(appliedSettings.language == 1 ? "ГОТОВО" : "READY",
                 furnaceLayout.infoX,
                 furnaceLayout.infoY + 54.0f,
                 1.55f,
                 glm::vec3(0.92f, 0.94f, 0.98f),
                 false);
        drawText(displayNameForBlock(furnaceState.output.type, appliedSettings.language == 1),
                 furnaceLayout.infoX,
                 furnaceLayout.infoY + 72.0f,
                 1.55f,
                 glm::vec3(0.86f, 0.90f, 0.98f),
                 false);
      }

      if (furnaceReady) {
        addSolidQuad(furnaceLayout.outputX - 5.0f,
                     furnaceLayout.outputY - 5.0f,
                     kSlotSize + 10.0f,
                     kSlotSize + 10.0f,
                     glm::vec3(0.88f, 0.72f, 0.24f));
      }
      addQuad(furnaceLayout.inputX, furnaceLayout.inputY, kSlotSize, kSlotSize, glm::vec3(0.25f, 0.25f, 0.28f), backgroundTile);
      addQuad(furnaceLayout.fuelX, furnaceLayout.fuelY, kSlotSize, kSlotSize, glm::vec3(0.25f, 0.25f, 0.28f), backgroundTile);
      addQuad(furnaceLayout.outputX,
              furnaceLayout.outputY,
              kSlotSize,
              kSlotSize,
              furnaceReady ? glm::vec3(0.28f, 0.40f, 0.24f) : glm::vec3(0.25f, 0.25f, 0.28f),
              backgroundTile);
      drawStack(furnaceState.input, furnaceLayout.inputX, furnaceLayout.inputY);
      drawStack(furnaceState.fuel, furnaceLayout.fuelX, furnaceLayout.fuelY);
      drawStack(furnaceState.output, furnaceLayout.outputX, furnaceLayout.outputY);

      drawText(appliedSettings.language == 1 ? "ПЛАВКА" : "SMELT",
               furnaceLayout.progressX,
               furnaceLayout.progressY - 14.0f,
               1.3f,
               glm::vec3(0.92f, 0.90f, 0.66f),
               false);
      drawNumber(smeltPercent,
                 furnaceLayout.progressX + furnaceLayout.progressW + 22.0f,
                 furnaceLayout.progressY + 11.0f,
                 1.7f,
                 glm::vec3(0.96f, 0.96f, 0.98f),
                 backgroundTile);
      addSolidQuad(furnaceLayout.progressX - 2.0f,
                   furnaceLayout.progressY - 2.0f,
                   furnaceLayout.progressW + 4.0f,
                   furnaceLayout.progressH + 4.0f,
                   glm::vec3(0.34f, 0.26f, 0.10f));
      addSolidQuad(furnaceLayout.progressX,
                   furnaceLayout.progressY,
                   furnaceLayout.progressW,
                   furnaceLayout.progressH,
                   glm::vec3(0.18f, 0.18f, 0.22f));
      addSolidQuad(furnaceLayout.progressX,
                   furnaceLayout.progressY,
                   furnaceLayout.progressW * smeltFill,
                   furnaceLayout.progressH,
                   glm::vec3(0.90f, 0.72f, 0.28f));

      drawText(appliedSettings.language == 1 ? "ТОПЛИВО" : "FUEL",
               furnaceLayout.fuelBarX,
               furnaceLayout.fuelBarY - 14.0f,
               1.3f,
               glm::vec3(0.94f, 0.80f, 0.54f),
               false);
      drawNumber(burnPercent,
                 furnaceLayout.fuelBarX + furnaceLayout.fuelBarW + 22.0f,
                 furnaceLayout.fuelBarY + 11.0f,
                 1.7f,
                 glm::vec3(0.96f, 0.96f, 0.98f),
                 backgroundTile);
      addSolidQuad(furnaceLayout.fuelBarX - 2.0f,
                   furnaceLayout.fuelBarY - 2.0f,
                   furnaceLayout.fuelBarW + 4.0f,
                   furnaceLayout.fuelBarH + 4.0f,
                   glm::vec3(0.30f, 0.16f, 0.08f));
      addSolidQuad(furnaceLayout.fuelBarX,
                   furnaceLayout.fuelBarY,
                   furnaceLayout.fuelBarW,
                   furnaceLayout.fuelBarH,
                   glm::vec3(0.18f, 0.18f, 0.22f));
      addSolidQuad(furnaceLayout.fuelBarX,
                   furnaceLayout.fuelBarY,
                   furnaceLayout.fuelBarW * burnFill,
                   furnaceLayout.fuelBarH,
                   burnFill > 0.0f ? glm::vec3(0.98f, 0.54f, 0.14f)
                                   : glm::vec3(0.34f, 0.16f, 0.12f));

      addSolidQuad(furnaceLayout.flameX - 2.0f,
                   furnaceLayout.flameY - 2.0f,
                   furnaceLayout.flameW + 4.0f,
                   furnaceLayout.flameH + 4.0f,
                   glm::vec3(0.28f, 0.16f, 0.08f));
      addSolidQuad(furnaceLayout.flameX,
                   furnaceLayout.flameY,
                   furnaceLayout.flameW,
                   furnaceLayout.flameH,
                   glm::vec3(0.16f, 0.16f, 0.20f));
      float flameFillH = furnaceLayout.flameH * burnFill;
      addSolidQuad(furnaceLayout.flameX + 3.0f,
                   furnaceLayout.flameY + (furnaceLayout.flameH - flameFillH),
                   furnaceLayout.flameW - 6.0f,
                   flameFillH,
                   glm::vec3(0.98f, 0.56f, 0.16f));
    } else if (creativeInventoryScreen) {
      CraftUiLayout creativeLayout = makeCraftUiLayout(uiLayoutWidth(), uiLayoutHeight(), 2);
      addQuad(creativeLayout.panelX,
              creativeLayout.panelY,
              creativeLayout.panelWidth,
              creativeLayout.panelHeight,
              glm::vec3(0.13f, 0.14f, 0.17f),
              backgroundTile);

      drawText(appliedSettings.language == 1 ? "КРЕАТИВ" : "CREATIVE",
               creativeLayout.inputX,
               creativeLayout.panelY + 4.0f,
               2.1f,
               glm::vec3(0.92f, 0.94f, 0.98f),
               false);
      drawText(appliedSettings.language == 1 ? "ВСЕ БЛОКИ В СПИСКЕ" : "ALL BLOCKS IN THE GRID",
               creativeLayout.infoX,
               creativeLayout.panelY + 4.0f,
               1.8f,
               glm::vec3(0.76f, 0.84f, 0.93f),
               false);
      drawText(appliedSettings.language == 1 ? "ЛКМ КОПИЯ СТАКА" : "LMB COPY STACK",
               creativeLayout.infoX,
               creativeLayout.infoY + 16.0f,
               1.55f,
               glm::vec3(0.64f, 0.88f, 0.66f),
               false);
      drawText(appliedSettings.language == 1 ? "ПКМ ОДИН ПРЕДМЕТ" : "RMB COPY ONE",
               creativeLayout.infoX,
               creativeLayout.infoY + 34.0f,
               1.55f,
               glm::vec3(0.64f, 0.88f, 0.66f),
               false);
      drawText(appliedSettings.language == 1 ? "ДВОЙНОЙ ПРОБЕЛ ДЛЯ ПОЛЕТА" : "DOUBLE SPACE TO FLY",
               creativeLayout.infoX,
               creativeLayout.infoY + 52.0f,
               1.55f,
               glm::vec3(0.86f, 0.84f, 0.66f),
               false);

      float sampleX = creativeLayout.inputX;
      float sampleY = creativeLayout.inputY + 6.0f;
      for (size_t i = 0; i < 6 && i < kCreativeCatalogItems.size(); ++i) {
        addQuad(sampleX + static_cast<float>(i) * 26.0f,
                sampleY,
                22.0f,
                22.0f,
                glm::vec3(0.22f, 0.22f, 0.26f),
                backgroundTile);
        addQuad(sampleX + 3.0f + static_cast<float>(i) * 26.0f,
                sampleY + 3.0f,
                16.0f,
                16.0f,
                glm::vec3(1.0f),
                tileForBlock(kCreativeCatalogItems[i]));
      }
    } else {
      int craftGridSize = activeCraftGridSize();
      CraftUiLayout craftLayout = makeCraftUiLayout(uiLayoutWidth(), uiLayoutHeight(), craftGridSize);
      CraftMatch craftMatch{};
      bool hasCraftMatch = findCraftMatch(craftingSlots, craftGridSize, craftMatch);
      float craftFlash = std::clamp(craftResultFlashTimer / kCraftResultFlashDuration, 0.0f, 1.0f);
      addQuad(craftLayout.panelX,
              craftLayout.panelY,
              craftLayout.panelWidth,
              craftLayout.panelHeight,
              glm::vec3(0.13f, 0.14f, 0.17f),
              backgroundTile);

      drawText(workbenchOpen
                 ? (appliedSettings.language == 1 ? "ВЕРСТАК" : "WORKBENCH")
                 : (appliedSettings.language == 1 ? "КРАФТ" : "CRAFTING"),
               craftLayout.inputX,
               craftLayout.panelY + 4.0f,
               2.1f,
               glm::vec3(0.92f, 0.94f, 0.98f),
               false);
      drawText(craftGridSize == 3
                 ? (appliedSettings.language == 1 ? "3X3 ВЕРСТАК РЯДОМ" : "3X3 NEAR WORKBENCH")
                 : (appliedSettings.language == 1 ? "2X2 ИНВЕНТАРЬ" : "2X2 INVENTORY"),
               craftLayout.infoX,
               craftLayout.panelY + 4.0f,
               1.8f,
               glm::vec3(0.76f, 0.84f, 0.93f),
               false);
      drawText(appliedSettings.language == 1 ? "ЛКМ 1 ПКМ ВСЕ" : "LMB 1 RMB MAX",
               craftLayout.infoX,
               craftLayout.infoY + 16.0f,
               1.55f,
               glm::vec3(0.64f, 0.88f, 0.66f),
               false);
      drawText(craftGridSize == 3
                 ? (appliedSettings.language == 1 ? "ВЕРСТАК АКТИВЕН" : "WORKBENCH ACTIVE")
                 : (appliedSettings.language == 1 ? "ПОСТАВЬ ВЕРСТАК ДЛЯ 3X3" : "PLACE A WORKBENCH FOR 3X3"),
               craftLayout.infoX,
               craftLayout.infoY + 34.0f,
               1.55f,
               craftGridSize == 3 ? glm::vec3(0.70f, 0.86f, 0.70f)
                                  : glm::vec3(0.92f, 0.74f, 0.54f),
               false);
      if (hasCraftMatch) {
        drawText(appliedSettings.language == 1 ? "РЕЗУЛЬТАТ" : "RESULT",
                 craftLayout.infoX,
                 craftLayout.infoY + 52.0f,
                 1.55f,
                 glm::vec3(0.92f, 0.94f, 0.98f),
                 false);
        drawText(displayNameForBlock(craftMatch.output, appliedSettings.language == 1),
                 craftLayout.infoX,
                 craftLayout.infoY + 70.0f,
                 1.55f,
                 glm::vec3(0.86f, 0.90f, 0.98f),
                 false);
      }

      std::array<int, 256> storedCounts{};
      auto countSlots = [&](const auto& slots) {
        for (const ItemStack& slot : slots) {
          if (slot.type == kAir || slot.count == 0) {
            continue;
          }
          storedCounts[static_cast<size_t>(slot.type)] += static_cast<int>(slot.count);
        }
      };
      countSlots(hotbar);
      countSlots(inventory);
      countSlots(craftingSlots);
      if (cursorStack.type != kAir && cursorStack.count > 0) {
        storedCounts[static_cast<size_t>(cursorStack.type)] += static_cast<int>(cursorStack.count);
      }

      std::vector<const CraftRecipeDefinition*> hintRecipes;
      hintRecipes.reserve(4);
      for (const CraftRecipeDefinition& recipe : kCraftRecipes) {
        if (craftGridSize < recipe.width || craftGridSize < recipe.height) {
          continue;
        }
        if (hasCraftMatch && craftMatch.output == recipe.output) {
          continue;
        }

        std::array<int, 256> required{};
        for (uint8_t type : recipe.cells) {
          if (type != kAir) {
            required[static_cast<size_t>(type)] += 1;
          }
        }

        bool canCraftSoon = true;
        for (size_t type = 0; type < required.size(); ++type) {
          if (required[type] > storedCounts[type]) {
            canCraftSoon = false;
            break;
          }
        }
        if (!canCraftSoon) {
          continue;
        }

        hintRecipes.push_back(&recipe);
        if (hintRecipes.size() >= 4) {
          break;
        }
      }

      if (!hintRecipes.empty()) {
        float hintHeaderY = craftLayout.panelY + craftLayout.panelHeight - 88.0f;
        drawText(appliedSettings.language == 1 ? "ПОДСКАЗКИ" : "HINTS",
                 craftLayout.inputX,
                 hintHeaderY,
                 1.5f,
                 glm::vec3(0.90f, 0.86f, 0.66f),
                 false);
        for (size_t i = 0; i < hintRecipes.size(); ++i) {
          const CraftRecipeDefinition& recipe = *hintRecipes[i];
          std::array<std::pair<uint8_t, int>, 3> ingredients{};
          int ingredientCount = 0;
          for (uint8_t type : recipe.cells) {
            if (type == kAir) {
              continue;
            }
            bool merged = false;
            for (int j = 0; j < ingredientCount; ++j) {
              if (ingredients[static_cast<size_t>(j)].first == type) {
                ingredients[static_cast<size_t>(j)].second += 1;
                merged = true;
                break;
              }
            }
            if (!merged && ingredientCount < static_cast<int>(ingredients.size())) {
              ingredients[static_cast<size_t>(ingredientCount++)] = {type, 1};
            }
          }

          float rowY = hintHeaderY + 14.0f + static_cast<float>(i) * 20.0f;
          float iconX = craftLayout.inputX;
          for (int j = 0; j < ingredientCount; ++j) {
            addQuad(iconX,
                    rowY,
                    18.0f,
                    18.0f,
                    glm::vec3(0.22f, 0.22f, 0.26f),
                    backgroundTile);
            addQuad(iconX + 3.0f,
                    rowY + 3.0f,
                    12.0f,
                    12.0f,
                    glm::vec3(1.0f),
                    tileForBlock(ingredients[static_cast<size_t>(j)].first));
            if (ingredients[static_cast<size_t>(j)].second > 1) {
              drawNumber(ingredients[static_cast<size_t>(j)].second,
                         iconX + 17.0f,
                         rowY + 17.0f,
                         1.35f,
                         glm::vec3(0.96f, 0.96f, 0.98f),
                         backgroundTile);
            }
            iconX += 22.0f;
          }

          drawText("TO",
                   iconX + 2.0f,
                   rowY + 4.0f,
                   1.25f,
                   glm::vec3(0.72f, 0.80f, 0.90f),
                   false);
          float resultX = iconX + 22.0f;
          addQuad(resultX,
                  rowY,
                  18.0f,
                  18.0f,
                  glm::vec3(0.20f, 0.30f, 0.20f),
                  backgroundTile);
          addQuad(resultX + 3.0f,
                  rowY + 3.0f,
                  12.0f,
                  12.0f,
                  glm::vec3(1.0f),
                  tileForBlock(recipe.output));
          if (recipe.outputCount > 1) {
            drawNumber(static_cast<int>(recipe.outputCount),
                       resultX + 17.0f,
                       rowY + 17.0f,
                       1.35f,
                       glm::vec3(0.96f, 0.96f, 0.98f),
                       backgroundTile);
          }
          drawText(displayNameForBlock(recipe.output, appliedSettings.language == 1),
                   resultX + 24.0f,
                   rowY + 4.0f,
                   1.2f,
                   glm::vec3(0.86f, 0.90f, 0.98f),
                   false);
        }
      }

      for (int row = 0; row < craftGridSize; ++row) {
        for (int col = 0; col < craftGridSize; ++col) {
          int slotIndex = craftingSlotIndex(row, col);
          float x = craftLayout.inputX + static_cast<float>(col) * (kSlotSize + kSlotPadding);
          float y = craftLayout.inputY + static_cast<float>(row) * (kSlotSize + kSlotPadding);
          addQuad(x,
                  y,
                  kSlotSize,
                  kSlotSize,
                  glm::vec3(0.25f, 0.25f, 0.28f),
                  backgroundTile);
          drawStack(craftingSlots[static_cast<size_t>(slotIndex)], x, y);
        }
      }

      if (hasCraftMatch) {
        addSolidQuad(craftLayout.resultX - 18.0f,
                     craftLayout.resultY + (kSlotSize * 0.5f) - 2.0f,
                     12.0f,
                     4.0f,
                     glm::vec3(0.78f, 0.72f, 0.30f));
      }
      if (hasCraftMatch || craftFlash > 0.0f) {
        float auraPad = hasCraftMatch ? 5.0f : (3.0f + craftFlash * 5.0f);
        glm::vec3 auraColor(
          0.32f + craftFlash * 0.52f,
          0.52f + craftFlash * 0.24f,
          0.22f + craftFlash * 0.02f);
        addSolidQuad(craftLayout.resultX - auraPad,
                     craftLayout.resultY - auraPad,
                     kSlotSize + auraPad * 2.0f,
                     kSlotSize + auraPad * 2.0f,
                     auraColor);
      }
      addQuad(craftLayout.resultX,
              craftLayout.resultY,
              kSlotSize,
              kSlotSize,
              hasCraftMatch
                ? glm::vec3(0.30f + craftFlash * 0.18f,
                            0.42f + craftFlash * 0.10f,
                            0.24f)
                : glm::vec3(0.24f, 0.24f, 0.28f),
              backgroundTile);
      if (hasCraftMatch) {
        addSolidQuad(craftLayout.resultX + 6.0f,
                     craftLayout.resultY + 6.0f,
                     kSlotSize - 12.0f,
                     4.0f,
                     glm::vec3(0.88f, 0.82f, 0.42f));
        drawStack(ItemStack{craftMatch.output, craftMatch.outputCount},
                  craftLayout.resultX,
                  craftLayout.resultY);
        if (craftMatch.maxCrafts > 1) {
          drawNumber(craftMatch.maxCrafts,
                     craftLayout.resultX + 16.0f,
                     craftLayout.resultY + 14.0f,
                     2.0f,
                     glm::vec3(0.96f, 0.96f, 0.98f),
                     backgroundTile);
        }
      }
    }

    size_t idx = 0;
    for (int row = 0; row < kInventoryRows; ++row) {
      for (int col = 0; col < kInventoryCols; ++col) {
        float x = gridX + static_cast<float>(col) * (kSlotSize + kSlotPadding);
        float y = gridY + static_cast<float>(row) * (kSlotSize + kSlotPadding);

        addQuad(x, y, kSlotSize, kSlotSize, glm::vec3(0.25f, 0.25f, 0.28f), backgroundTile);

        if (creativeInventoryScreen) {
          if (idx < kCreativeCatalogItems.size()) {
            drawStack(ItemStack{kCreativeCatalogItems[idx], 1}, x, y);
          }
        } else if (idx < inventory.size()) {
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

    drawStack(hotbar[i], x, y, glm::vec3(1.0f, 1.0f, 1.0f), kTileUiWhite);
  }

  if (selectedItemToastTimer > 0.0f && !selectedItemToastText.empty()) {
    float textPixel = 2.4f;
    float textW = measureTextWidth(selectedItemToastText, textPixel);
    float boxW = textW + 24.0f;
    float boxH = textPixel * static_cast<float>(kGlyphHeight) + 14.0f;
    float boxX = uiW * 0.5f - boxW * 0.5f;
    float boxY = startY - boxH - 14.0f;

    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.12f, 0.13f, 0.17f), backgroundTile);
    drawText(selectedItemToastText,
             uiW * 0.5f,
             boxY + 7.0f,
             textPixel,
             glm::vec3(0.92f, 0.94f, 0.98f),
             true);
  }

  if (commandInputOpen) {
    std::string visibleCommand = commandInput.empty() ? "/" : commandInput;
    if ((static_cast<int>(glfwGetTime() * 2.0) & 1) == 0) {
      visibleCommand += "_";
    }
    float textPixel = 2.0f;
    float boxW = std::max(220.0f, measureTextWidth(visibleCommand, textPixel) + 22.0f);
    float boxH = textPixel * static_cast<float>(kGlyphHeight) + 14.0f;
    float boxX = 18.0f;
    float boxY = startY - boxH - 18.0f;
    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.12f, 0.13f, 0.17f), backgroundTile);
    drawText(visibleCommand,
             boxX + 8.0f,
             boxY + 7.0f,
             textPixel,
             glm::vec3(0.92f, 0.94f, 0.98f),
             false);
  }

  if (achievementPopupVisible) {
    AchievementId popupId = static_cast<AchievementId>(std::clamp<int>(activeAchievementPopupId, 0, static_cast<int>(App::kAchievementCount) - 1));
    bool ru = appliedSettings.language == 1;
    std::string title = achievementTitle(popupId, ru);
    std::string label = ru ? "ДОСТИЖЕНИЕ" : "ACHIEVEMENT";
    float titlePixel = 1.9f;
    float labelPixel = 1.45f;
    float textWidth = std::max(measureTextWidth(title, titlePixel), measureTextWidth(label, labelPixel));
    float boxW = textWidth + 88.0f;
    float boxH = 54.0f;
    float boxX = uiW - boxW - 18.0f;
    float boxY = 18.0f;
    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.08f, 0.10f, 0.13f), backgroundTile);
    addQuad(boxX + 8.0f, boxY + 8.0f, 38.0f, 38.0f, glm::vec3(0.20f, 0.22f, 0.26f), backgroundTile);
    addQuad(boxX + 11.0f, boxY + 11.0f, 32.0f, 32.0f, glm::vec3(1.0f), achievementIconTile(popupId));
    drawText(label,
             boxX + 56.0f,
             boxY + 10.0f,
             labelPixel,
             glm::vec3(0.86f, 0.88f, 0.92f),
             false);
    drawText(title,
             boxX + 56.0f,
             boxY + 24.0f,
             titlePixel,
             glm::vec3(0.96f, 0.86f, 0.38f),
             false);
  }

  if (inventoryOpen && cursorStack.count > 0 && cursorStack.type != kAir) {
    float cx = cursorFbX - kSlotSize * 0.5f;
    float cy = cursorFbY - kSlotSize * 0.5f;
    drawStack(cursorStack, cx, cy);
  }

  if (debugWorldgenOverlay && (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused)) {
    auto toFixed = [](float value, int precision) {
      std::ostringstream ss;
      ss.setf(std::ios::fixed, std::ios::floatfield);
      ss.precision(precision);
      ss << value;
      return ss.str();
    };
    auto formatClock = [](float cycle) {
      float wrapped = cycle - std::floor(cycle);
      int totalMinutes = static_cast<int>(std::lround(wrapped * 24.0f * 60.0f));
      totalMinutes %= (24 * 60);
      int hours = totalMinutes / 60;
      int minutes = totalMinutes % 60;
      std::ostringstream ss;
      if (hours < 10) {
        ss << '0';
      }
      ss << hours << ':';
      if (minutes < 10) {
        ss << '0';
      }
      ss << minutes;
      return ss.str();
    };
    auto positiveMod = [](int value, int mod) {
      int m = value % mod;
      if (m < 0) {
        m += mod;
      }
      return m;
    };
    bool ruDebug = appliedSettings.language == 1;
    auto biomeLabel = [&](uint8_t biome) -> std::string {
      switch (biome) {
        case 0:
          return ruDebug ? "ОКЕАН" : "OCEAN";
        case 1:
          return ruDebug ? "ПЛЯЖ" : "BEACH";
        case 2:
          return ruDebug ? "РАВНИНЫ" : "PLAINS";
        case 3:
          return ruDebug ? "ЛЕС" : "FOREST";
        case 4:
          return ruDebug ? "ПУСТЫНЯ" : "DESERT";
        case 5:
          return ruDebug ? "ГОРЫ" : "MOUNTAINS";
        case 6:
          return ruDebug ? "КРАХ" : "CRASH";
        default:
          return ruDebug ? "НЕИЗВЕСТНО" : "UNKNOWN";
      }
    };
    auto visibleBiomeFromSurface = [&](uint8_t climateBiome,
                                       uint8_t surfaceBlock,
                                       int surfaceY,
                                       int aquiferY,
                                       const BiomeClimateSample& climate) -> uint8_t {
      uint8_t visibleBiome = climateBiome;
      if (climateBiome == 6) {
        return 6;
      }
      bool nearWaterline = surfaceY <= aquiferY + 2;
      bool sandySurface = surfaceBlock == kSand;
      bool gravelSurface = surfaceBlock == kGravel;
      bool rockySurface = surfaceBlock == kStone || gravelSurface;
      bool elevatedTerrain = surfaceY > aquiferY + 18;
      bool alpineElevation = surfaceY > std::max(aquiferY + 28, 56);
      bool highlandClimate =
        climate.continentalness > 0.58f &&
        climate.depth > 0.34f &&
        climate.erosion < 0.58f;

      if (sandySurface) {
        bool looksDesert =
          climateBiome == 4 ||
          (climate.temperature > 0.60f && climate.humidity < 0.45f && !nearWaterline);
        bool looksBeach =
          climateBiome == 0 ||
          climateBiome == 1 ||
          nearWaterline ||
          climate.continentalness < 0.42f;
        if (looksDesert) {
          visibleBiome = 4;
        } else if (looksBeach) {
          visibleBiome = 1;
        }
      } else if (gravelSurface &&
                 (climateBiome == 0 || climateBiome == 1 || nearWaterline)) {
        visibleBiome = 1;
      }

      if (rockySurface &&
          elevatedTerrain &&
          (climateBiome == 5 ||
           climate.weirdness > 0.62f ||
           alpineElevation ||
           highlandClimate)) {
        visibleBiome = 5;
      }

      return visibleBiome;
    };

    int worldX = static_cast<int>(std::floor(playerPos.x));
    int worldY = static_cast<int>(std::floor(playerPos.y + 1.62f));
    int worldZ = static_cast<int>(std::floor(playerPos.z));
    int chunkX = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int chunkZ = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    int localX = positiveMod(worldX, kChunkSize);
    int localZ = positiveMod(worldZ, kChunkSize);
    int borderDistX = std::min(localX, (kChunkSize - 1) - localX);
    int borderDistZ = std::min(localZ, (kChunkSize - 1) - localZ);

    uint8_t biomeId = 0;
    BiomeClimateSample climate{};
    world.sampleBiomeClimateAt(worldX, worldZ, biomeId, climate);
    int surfaceY = world.sampleSurfaceHeightAt(worldX, worldZ);
    int aquiferY = world.sampleAquiferLevelAt(worldX, worldZ);
    const WorldGenSettings& gen = world.getGenerationSettings();
    uint8_t surfaceBlock =
      (surfaceY >= gen.minY && surfaceY <= gen.maxY) ? world.getBlock(worldX, surfaceY, worldZ) : kAir;
    uint8_t visibleBiomeId = visibleBiomeFromSurface(biomeId, surfaceBlock, surfaceY, aquiferY, climate);
    int sampleY = std::clamp(worldY + debugDensitySliceOffset, gen.minY, gen.maxY);
    float densityEye = world.sampleDensityAt(worldX, worldY, worldZ);
    float densitySlice = world.sampleDensityAt(worldX, sampleY, worldZ);
    float densitySliceUp = world.sampleDensityAt(worldX, std::clamp(sampleY + 8, gen.minY, gen.maxY), worldZ);
    float densitySliceDown = world.sampleDensityAt(worldX, std::clamp(sampleY - 8, gen.minY, gen.maxY), worldZ);

    std::vector<std::string> lines;
    lines.push_back("WORLDGEN DEBUG");
    lines.push_back((ruDebug ? "КАДРЫ " : "FPS ") + std::to_string(std::max(0, fpsDisplayValue)));
    lines.push_back("XYZ " +
                    toFixed(playerPos.x, 1) + " " +
                    toFixed(playerPos.y, 1) + " " +
                    toFixed(playerPos.z, 1));
    lines.push_back("CHUNK " + std::to_string(chunkX) + " " + std::to_string(chunkZ) +
                    " LOCAL " + std::to_string(localX) + " " + std::to_string(localZ));
    lines.push_back("BORDER DIST " + std::to_string(borderDistX) + " " + std::to_string(borderDistZ));
    lines.push_back("TIME " + formatClock(dayCycleTime) + " DAYL " + toFixed(dayLightFactor, 2));
    lines.push_back("WEATHER " + toFixed(weatherIntensity, 2) +
                    " TARGET " + toFixed(weatherTargetIntensity, 2));
    lines.push_back((ruDebug ? "БИОМ " : "BIOME ") +
                    biomeLabel(visibleBiomeId) +
                    " (" + std::to_string(static_cast<int>(visibleBiomeId)) + ")");
    if (visibleBiomeId != biomeId) {
      lines.push_back((ruDebug ? "КЛИМАТ " : "CLIMATE ") +
                      biomeLabel(biomeId) +
                      " (" + std::to_string(static_cast<int>(biomeId)) + ")");
    }
    lines.push_back("SURFACE Y " + std::to_string(surfaceY) + " AQ Y " + std::to_string(aquiferY));
    if (debugWorldgenOverlayMode >= 1) {
      lines.push_back("CL T " + toFixed(climate.temperature, 2) +
                      " H " + toFixed(climate.humidity, 2) +
                      " C " + toFixed(climate.continentalness, 2));
      lines.push_back("CL E " + toFixed(climate.erosion, 2) +
                      " D " + toFixed(climate.depth, 2) +
                      " W " + toFixed(climate.weirdness, 2));
    }
    lines.push_back("DENS Y " + std::to_string(worldY) + " " + toFixed(densityEye, 2));
    lines.push_back("SLICE Y " + std::to_string(sampleY) + " " + toFixed(densitySlice, 2));
    if (debugWorldgenOverlayMode >= 2) {
      lines.push_back("SLICE UP8 " + toFixed(densitySliceUp, 2) +
                      " DOWN8 " + toFixed(densitySliceDown, 2));
    }
    lines.push_back("F3 TOGGLE F4 MODE PGUP/PGDN SLICE");

    float textPixel = 1.9f;
    float maxLineW = 0.0f;
    for (const std::string& line : lines) {
      maxLineW = std::max(maxLineW, measureTextWidth(line, textPixel));
    }
    float boxW = maxLineW + 20.0f;
    float boxH = static_cast<float>(lines.size()) * (textPixel * static_cast<float>(kGlyphHeight) + 4.0f) + 14.0f;
    float boxX = uiW - boxW - 14.0f;
    float boxY = 12.0f;
    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.05f, 0.08f, 0.11f), backgroundTile);
    float lineY = boxY + 7.0f;
    for (size_t i = 0; i < lines.size(); ++i) {
      glm::vec3 color = (i == 0)
        ? glm::vec3(0.88f, 0.93f, 0.98f)
        : glm::vec3(0.76f, 0.86f, 0.95f);
      drawText(lines[i], boxX + 10.0f, lineY, textPixel, color, false);
      lineY += textPixel * static_cast<float>(kGlyphHeight) + 4.0f;
    }
  }

  if (debugProfilerOverlay && (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused)) {
    auto toFixed = [](double value, int precision) {
      std::ostringstream ss;
      ss.setf(std::ios::fixed, std::ios::floatfield);
      ss.precision(precision);
      ss << value;
      return ss.str();
    };
    auto metricLine = [&](const std::string& label, const App::ProfilerMetric& metric) {
      return label + " " + toFixed(metric.lastMs, 2) + " / " + toFixed(metric.avgMs, 2);
    };

    bool ru = appliedSettings.language == 1;
    VulkanContext::RenderStats renderStats = vk.getLastRenderStats();
    World::RuntimeStats worldStats = world.collectRuntimeStats();
    std::string qualityLabel = "MED";
    if (appliedSettings.graphicsQuality == 0) {
      qualityLabel = ru ? "НИЗК" : "LOW";
    } else if (appliedSettings.graphicsQuality == 2) {
      qualityLabel = ru ? "ВЫС" : "HIGH";
    }

    std::vector<std::string> lines;
    lines.push_back(ru ? "ПРОФАЙЛЕР КАДРА LAST/AVG MS" : "FRAME PROFILER LAST/AVG MS");
    lines.push_back((ru ? "КАДР " : "FRAME ") +
                    toFixed(profiler.frame.lastMs, 2) + " / " + toFixed(profiler.frame.avgMs, 2) +
                    "  MAX " + toFixed(profiler.frame.maxMs, 2) +
                    "  TGT " + toFixed(profiler.targetFrameMs, 2));
    lines.push_back(metricLine(ru ? "ВВОД" : "INPUT", profiler.eventsInput));
    lines.push_back(metricLine("ENV", profiler.environment));
    lines.push_back(metricLine(ru ? "ИГРОК" : "PLAYER", profiler.player));
    lines.push_back(metricLine(ru ? "СТРИМ" : "STREAM", profiler.streaming));
    lines.push_back(metricLine("SIM", profiler.simulation));
    lines.push_back(metricLine(ru ? "ГЕЙМ" : "GAME", profiler.gameplay));
    lines.push_back(metricLine("MESH", profiler.mesh));
    lines.push_back(metricLine("UI", profiler.ui));
    lines.push_back(metricLine(ru ? "ФАКЕЛЫ" : "TORCH", profiler.torch));
    lines.push_back(metricLine("DRAW", profiler.draw));
    lines.push_back(metricLine(ru ? "СОН" : "SLEEP", profiler.sleep));
    lines.push_back((ru ? "DRAWCALLS " : "DRAWCALLS ") +
                    std::to_string(renderStats.totalDrawCalls) +
                    " W " + std::to_string(renderStats.worldDrawCalls) +
                    " FP " + std::to_string(renderStats.firstPersonDrawCalls) +
                    " UI " + std::to_string(renderStats.uiDrawCalls));
    lines.push_back((ru ? "СЕКЦИИ " : "SECTIONS ") +
                    std::to_string(renderStats.worldMeshesTracked) +
                    " DR " + std::to_string(renderStats.worldMeshesDrawn) +
                    " D " + std::to_string(renderStats.worldDistanceCulled) +
                    " F " + std::to_string(renderStats.worldFrustumCulled));
    lines.push_back((ru ? "ИНДЕКСЫ " : "INDICES ") +
                    std::to_string(renderStats.worldIndicesDrawn) +
                    " SP " + std::to_string(renderStats.worldSpecialMeshes));
    lines.push_back((ru ? "ЧАНКИ " : "CHUNKS ") +
                    std::to_string(worldStats.loadedChunks) +
                    " GEN " + std::to_string(worldStats.generatingChunks) +
                    " DIRTY " + std::to_string(worldStats.dirtySections) +
                    " Q " + std::to_string(worldStats.queuedSections));
    lines.push_back((ru ? "ОЧЕРЕДИ G " : "QUEUES G ") +
                    std::to_string(worldStats.generationQueue) + "/" +
                    std::to_string(worldStats.generationResults) +
                    " S " + std::to_string(worldStats.pendingSectionRebuilds) + "/" +
                    std::to_string(worldStats.sectionWorkerQueue) + "/" +
                    std::to_string(worldStats.sectionWorkerResults));
    lines.push_back((ru ? "MESH UP " : "MESH UP ") +
                    std::to_string(worldStats.pendingMeshUploads) +
                    " RM " + std::to_string(worldStats.pendingRemovedMeshes) +
                    " GPU " + std::to_string(profiler.pendingGpuUploads) + "/" +
                    std::to_string(profiler.pendingGpuRemovals));
    lines.push_back((ru ? "КАДР UP " : "FRAME UP ") +
                    std::to_string(profiler.chunkUpdates) +
                    " RM " + std::to_string(profiler.chunkRemovals) +
                    " LIGHTS " + std::to_string(profiler.torchLights));
    lines.push_back((ru ? "FPS CAP " : "FPS CAP ") +
                    toFixed(profiler.targetFps, 0) +
                    " RD " + std::to_string(loadedChunkViewRadius) + "/" + std::to_string(activeChunkViewRadius) +
                    " GFX " + qualityLabel);
    lines.push_back(ru ? "F6 ПРОФАЙЛЕР" : "F6 PROFILER");

    float textPixel = 1.55f;
    float maxLineW = 0.0f;
    for (const std::string& line : lines) {
      maxLineW = std::max(maxLineW, measureTextWidth(line, textPixel));
    }
    float boxW = maxLineW + 20.0f;
    float boxH = static_cast<float>(lines.size()) * (textPixel * static_cast<float>(kGlyphHeight) + 4.0f) + 14.0f;
    float boxX = 14.0f;
    float boxY = 42.0f;
    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.05f, 0.07f, 0.10f), backgroundTile);
    float lineY = boxY + 7.0f;
    for (size_t i = 0; i < lines.size(); ++i) {
      glm::vec3 color = (i == 0)
        ? glm::vec3(0.92f, 0.95f, 0.99f)
        : glm::vec3(0.75f, 0.86f, 0.95f);
      drawText(lines[i], boxX + 10.0f, lineY, textPixel, color, false);
      lineY += textPixel * static_cast<float>(kGlyphHeight) + 4.0f;
    }
  }

  if (achievementTreeOpen && screenState == ScreenState::kPlaying) {
    bool ru = appliedSettings.language == 1;
    int paperTile = tileForBlock(kSand);
    AchievementTreeUiLayout layout = makeAchievementTreeLayout(uiLayoutWidth(), uiLayoutHeight());
    glm::vec2 scroll =
      clampAchievementTreeScroll(layout, {achievementTreeScrollX, achievementTreeScrollY});
    achievementTreeScrollX = scroll.x;
    achievementTreeScrollY = scroll.y;
    int activeTab = activeAchievementTreeTab(layout, scroll.x);

    auto addPaperQuad = [&](float x, float y, float w, float h, const glm::vec3& color) {
      addQuad(x, y, w, h, color, paperTile);
    };

    auto addClippedSolidQuad = [&](float x, float y, float w, float h, const glm::vec3& color) {
      float clipX0 = std::max(x, layout.viewportX);
      float clipY0 = std::max(y, layout.viewportY);
      float clipX1 = std::min(x + w, layout.viewportX + layout.viewportW);
      float clipY1 = std::min(y + h, layout.viewportY + layout.viewportH);
      if (clipX1 <= clipX0 || clipY1 <= clipY0) {
        return;
      }
      addSolidQuad(clipX0, clipY0, clipX1 - clipX0, clipY1 - clipY0, color);
    };

    addSolidQuad(0.0f, 0.0f, uiW, uiH, glm::vec3(0.03f, 0.04f, 0.05f));
    addSolidQuad(layout.frameX + 12.0f,
                 layout.frameY + 14.0f,
                 layout.frameW,
                 layout.frameH,
                 glm::vec3(0.03f, 0.03f, 0.04f));
    addQuad(layout.frameX,
            layout.frameY,
            layout.frameW,
            layout.frameH,
            glm::vec3(0.84f, 0.84f, 0.86f),
            backgroundTile);
    addSolidQuad(layout.frameX + 6.0f,
                 layout.frameY + 6.0f,
                 layout.frameW - 12.0f,
                 layout.frameH - 12.0f,
                 glm::vec3(0.72f, 0.72f, 0.74f));
    addPaperQuad(layout.viewportX,
                 layout.viewportY,
                 layout.viewportW,
                 layout.viewportH,
                 glm::vec3(0.95f, 0.91f, 0.76f));
    addSolidQuad(layout.viewportX + 5.0f,
                 layout.viewportY + 5.0f,
                 layout.viewportW - 10.0f,
                 layout.viewportH - 10.0f,
                 glm::vec3(0.92f, 0.89f, 0.73f));

    float graphX = layout.viewportX + layout.contentPadX - scroll.x;
    float graphY = layout.viewportY + layout.contentPadY - scroll.y;

    int completedCount = 0;
    for (const AchievementTreeNodeView& node : kAchievementTreeNodes) {
      if ((achievementMask & achievementBit(node.id)) != 0u) {
        ++completedCount;
      }
    }

    for (const auto& edge : kAchievementTreeEdges) {
      const AchievementTreeNodeView& a = kAchievementTreeNodes[static_cast<size_t>(edge.first)];
      const AchievementTreeNodeView& b = kAchievementTreeNodes[static_cast<size_t>(edge.second)];
      float x0 = graphX + a.x * layout.spacingX + kAchievementNodeSize * 0.5f;
      float y0 = graphY + a.y * layout.spacingY + kAchievementNodeSize * 0.5f;
      float x1 = graphX + b.x * layout.spacingX + kAchievementNodeSize * 0.5f;
      float y1 = graphY + b.y * layout.spacingY + kAchievementNodeSize * 0.5f;
      bool lit = ((achievementMask & achievementBit(a.id)) != 0u) &&
                 ((achievementMask & achievementBit(b.id)) != 0u);
      glm::vec3 lineColor = lit ? glm::vec3(0.86f, 0.75f, 0.26f)
                                : glm::vec3(0.22f, 0.18f, 0.12f);
      if (std::abs(y1 - y0) < 1.0f) {
        addClippedSolidQuad(std::min(x0, x1), y0 - 3.0f, std::abs(x1 - x0), 6.0f, lineColor);
      } else {
        addClippedSolidQuad(x0 - 3.0f, std::min(y0, y1), 6.0f, std::abs(y1 - y0), lineColor);
        addClippedSolidQuad(std::min(x0, x1), y1 - 3.0f, std::abs(x1 - x0), 6.0f, lineColor);
      }
    }

    const AchievementTreeNodeView* hoveredNode = nullptr;
    for (const AchievementTreeNodeView& node : kAchievementTreeNodes) {
      bool completed = (achievementMask & achievementBit(node.id)) != 0u;
      bool available = completed || ((achievementMask & node.prereqMask) == node.prereqMask);
      float x = graphX + node.x * layout.spacingX;
      float y = graphY + node.y * layout.spacingY;
      bool visible = x + kAchievementNodeSize + 8.0f >= layout.viewportX &&
                     x - 8.0f <= layout.viewportX + layout.viewportW &&
                     y + kAchievementNodeSize + 8.0f >= layout.viewportY &&
                     y - 8.0f <= layout.viewportY + layout.viewportH;
      if (!visible) {
        continue;
      }

      bool hovered = !achievementTreeDragging &&
                     cursorFbX >= x - 5.0f &&
                     cursorFbX <= x + kAchievementNodeSize + 5.0f &&
                     cursorFbY >= y - 5.0f &&
                     cursorFbY <= y + kAchievementNodeSize + 5.0f &&
                     cursorFbX >= layout.viewportX &&
                     cursorFbX <= layout.viewportX + layout.viewportW &&
                     cursorFbY >= layout.viewportY &&
                     cursorFbY <= layout.viewportY + layout.viewportH;
      glm::vec3 frame = completed ? glm::vec3(0.76f, 0.56f, 0.06f)
                                  : (available ? glm::vec3(0.86f, 0.86f, 0.88f)
                                               : glm::vec3(0.42f, 0.42f, 0.44f));
      glm::vec3 fill = completed ? glm::vec3(0.76f, 0.56f, 0.06f)
                                 : glm::vec3(0.80f, 0.80f, 0.82f);
      if (hovered) {
        frame = glm::min(frame + glm::vec3(0.08f), glm::vec3(1.0f));
        fill = glm::min(fill + glm::vec3(0.06f), glm::vec3(1.0f));
      }

      addSolidQuad(x + 4.0f,
                   y + 4.0f,
                   kAchievementNodeSize + 4.0f,
                   kAchievementNodeSize + 4.0f,
                   glm::vec3(0.04f, 0.04f, 0.05f));
      addQuad(x - 2.0f,
              y - 2.0f,
              kAchievementNodeSize + 4.0f,
              kAchievementNodeSize + 4.0f,
              frame,
              backgroundTile);
      addQuad(x + 2.0f,
              y + 2.0f,
              kAchievementNodeSize - 4.0f,
              kAchievementNodeSize - 4.0f,
              fill,
              backgroundTile);
      addQuad(x + 7.0f,
              y + 7.0f,
              kAchievementNodeSize - 14.0f,
              kAchievementNodeSize - 14.0f,
              glm::vec3(completed ? 1.0f : (available ? 0.94f : 0.55f)),
              achievementIconTile(node.id));

      if (hovered) {
        hoveredNode = &node;
      }
    }

    glm::vec3 frameMask = glm::vec3(0.72f, 0.72f, 0.74f);
    float innerX = layout.frameX + 6.0f;
    float innerY = layout.frameY + 6.0f;
    float innerW = layout.frameW - 12.0f;
    float innerH = layout.frameH - 12.0f;
    addSolidQuad(innerX, innerY, innerW, layout.viewportY - innerY, frameMask);
    addSolidQuad(innerX,
                 layout.viewportY + layout.viewportH,
                 innerW,
                 innerY + innerH - (layout.viewportY + layout.viewportH),
                 frameMask);
    addSolidQuad(innerX, layout.viewportY, layout.viewportX - innerX, layout.viewportH, frameMask);
    addSolidQuad(layout.viewportX + layout.viewportW,
                 layout.viewportY,
                 innerX + innerW - (layout.viewportX + layout.viewportW),
                 layout.viewportH,
                 frameMask);

    for (int tab = 0; tab < 3; ++tab) {
      glm::vec4 rect = achievementTreeTabRect(layout, tab);
      bool selected = tab == activeTab;
      glm::vec3 tabFrame = selected ? glm::vec3(0.92f, 0.92f, 0.94f)
                                    : glm::vec3(0.76f, 0.76f, 0.78f);
      glm::vec3 tabFill = selected ? glm::vec3(0.82f, 0.82f, 0.84f)
                                   : glm::vec3(0.66f, 0.66f, 0.69f);
      int iconTile = tab == 0 ? tileForBlock(kGrass)
                   : (tab == 1 ? achievementIconTile(kAchievementCraftStonePickaxe)
                               : achievementIconTile(kAchievementGetDiamond));
      addSolidQuad(rect.x + 4.0f, rect.y + 6.0f, rect.z, rect.w, glm::vec3(0.04f, 0.04f, 0.05f));
      addQuad(rect.x, rect.y, rect.z, rect.w, tabFrame, backgroundTile);
      addQuad(rect.x + 4.0f, rect.y + 4.0f, rect.z - 8.0f, rect.w - 8.0f, tabFill, backgroundTile);
      addQuad(rect.x + 15.0f, rect.y + 12.0f, rect.z - 30.0f, rect.w - 24.0f, glm::vec3(1.0f), iconTile);
    }

    addQuad(layout.headerX,
            layout.headerY,
            layout.headerW,
            layout.headerH,
            glm::vec3(0.82f, 0.82f, 0.84f),
            backgroundTile);
    addSolidQuad(layout.headerX + 5.0f,
                 layout.headerY + 5.0f,
                 layout.headerW - 10.0f,
                 layout.headerH - 10.0f,
                 glm::vec3(0.74f, 0.74f, 0.76f));
    drawText(ru ? "ДОСТИЖЕНИЯ" : "ACHIEVEMENTS",
             layout.headerX + 18.0f,
             layout.headerY + 12.0f,
             3.2f,
             glm::vec3(0.28f, 0.28f, 0.30f),
             false);
    drawText((ru ? "ПРОГРЕСС " : "PROGRESS ") +
               std::to_string(completedCount) + "/" + std::to_string(kAchievementTreeNodes.size()),
             layout.headerX + layout.headerW - 178.0f,
             layout.headerY + 18.0f,
             1.85f,
             glm::vec3(0.52f, 0.42f, 0.10f),
             false);
    drawText(ru ? "КОЛЕСО ИЛИ ТЯНИ МЫШКОЙ" : "WHEEL OR DRAG TO SCROLL",
             layout.frameX + layout.frameW * 0.5f,
             layout.frameY + layout.frameH - 28.0f,
             1.55f,
             glm::vec3(0.30f, 0.30f, 0.34f),
             true);
    drawText(ru ? "L ИЛИ ESC ЗАКРЫТЬ" : "L OR ESC TO CLOSE",
             layout.frameX + layout.frameW - 182.0f,
             layout.frameY + layout.frameH - 28.0f,
             1.55f,
             glm::vec3(0.34f, 0.34f, 0.38f),
             false);

    float maxScrollX = std::max(0.0f, layout.contentW - layout.viewportW);
    float maxScrollY = std::max(0.0f, layout.contentH - layout.viewportH);
    if (maxScrollX > 0.0f) {
      float trackX = layout.viewportX + 20.0f;
      float trackY = layout.viewportY + layout.viewportH - 12.0f;
      float trackW = layout.viewportW - 40.0f;
      addSolidQuad(trackX, trackY, trackW, 4.0f, glm::vec3(0.60f, 0.55f, 0.42f));
      float thumbW = std::max(48.0f, trackW * (layout.viewportW / layout.contentW));
      float thumbX = trackX + (trackW - thumbW) * (scroll.x / maxScrollX);
      addSolidQuad(thumbX, trackY - 1.0f, thumbW, 6.0f, glm::vec3(0.80f, 0.70f, 0.24f));
    }
    if (maxScrollY > 0.0f) {
      float trackX = layout.viewportX + layout.viewportW - 12.0f;
      float trackY = layout.viewportY + 20.0f;
      float trackH = layout.viewportH - 40.0f;
      addSolidQuad(trackX, trackY, 4.0f, trackH, glm::vec3(0.60f, 0.55f, 0.42f));
      float thumbH = std::max(36.0f, trackH * (layout.viewportH / layout.contentH));
      float thumbY = trackY + (trackH - thumbH) * (scroll.y / maxScrollY);
      addSolidQuad(trackX - 1.0f, thumbY, 6.0f, thumbH, glm::vec3(0.80f, 0.70f, 0.24f));
    }

    if (hoveredNode) {
      bool completed = (achievementMask & achievementBit(hoveredNode->id)) != 0u;
      bool available = completed || ((achievementMask & hoveredNode->prereqMask) == hoveredNode->prereqMask);
      std::string title = achievementTitle(hoveredNode->id, ru);
      std::string description = achievementDescription(hoveredNode->id, ru);
      std::string state = completed
        ? (ru ? "ГОТОВО" : "DONE")
        : (available ? (ru ? "ДОСТУПНО" : "AVAILABLE")
                     : (ru ? "ЗАКРЫТО" : "LOCKED"));
      float titlePixel = 1.95f;
      float descPixel = 1.55f;
      float boxW = std::max({measureTextWidth(title, titlePixel),
                             measureTextWidth(description, descPixel),
                             measureTextWidth(state, descPixel)}) + 28.0f;
      float boxH = titlePixel * static_cast<float>(kGlyphHeight) +
                   descPixel * static_cast<float>(kGlyphHeight) * 2.0f + 34.0f;
      float tipX = std::clamp(cursorFbX + 18.0f,
                              layout.frameX + 16.0f,
                              layout.frameX + layout.frameW - boxW - 16.0f);
      float tipY = std::clamp(cursorFbY + 16.0f,
                              layout.frameY + 72.0f,
                              layout.frameY + layout.frameH - boxH - 18.0f);
      addSolidQuad(tipX + 4.0f, tipY + 6.0f, boxW, boxH, glm::vec3(0.04f, 0.04f, 0.05f));
      addQuad(tipX,
              tipY,
              boxW,
              boxH,
              completed ? glm::vec3(0.78f, 0.60f, 0.12f)
                        : (available ? glm::vec3(0.84f, 0.84f, 0.86f)
                                     : glm::vec3(0.56f, 0.56f, 0.58f)),
              backgroundTile);
      addPaperQuad(tipX + 4.0f,
                   tipY + 4.0f,
                   boxW - 8.0f,
                   boxH - 8.0f,
                   glm::vec3(0.95f, 0.91f, 0.76f));
      drawText(title,
               tipX + 12.0f,
               tipY + 10.0f,
               titlePixel,
               glm::vec3(0.24f, 0.24f, 0.28f),
               false);
      drawText(description,
               tipX + 12.0f,
               tipY + 12.0f + titlePixel * static_cast<float>(kGlyphHeight) + 6.0f,
               descPixel,
               glm::vec3(0.34f, 0.34f, 0.38f),
               false);
      drawText(state,
               tipX + 12.0f,
               tipY + boxH - descPixel * static_cast<float>(kGlyphHeight) - 10.0f,
               descPixel,
               completed ? glm::vec3(0.66f, 0.50f, 0.06f)
                         : (available ? glm::vec3(0.28f, 0.44f, 0.22f)
                                      : glm::vec3(0.56f, 0.26f, 0.26f)),
               false);
    }
  }

  if (!inventoryOpen && !achievementTreeOpen && !commandInputOpen) {
    const float centerX = uiW * 0.5f;
    const float centerY = uiH * 0.5f;
    const float crossArm = 5.0f;
    const float crossThickness = 2.6f;
    const int crossTile = 13;
    addQuad(centerX - crossArm, centerY - crossThickness * 0.5f,
            crossArm * 2.0f, crossThickness, glm::vec3(0.97f), crossTile);
    addQuad(centerX - crossThickness * 0.5f, centerY - crossArm,
            crossThickness, crossArm * 2.0f, glm::vec3(0.97f), crossTile);
  }

}

void App::composeMeshData() {
  meshVertices = skyVertices;
  meshIndices = skyIndices;
  skyIndexCount = static_cast<uint32_t>(skyIndices.size());
  worldIndexCount = 0;

  uint32_t vertexOffset = static_cast<uint32_t>(meshVertices.size());
  meshVertices.insert(meshVertices.end(), uiVertices.begin(), uiVertices.end());
  meshIndices.reserve(meshIndices.size() + uiIndices.size());
  for (uint32_t idx : uiIndices) {
    meshIndices.push_back(idx + vertexOffset);
  }
  uiIndexCount = static_cast<uint32_t>(uiIndices.size());
}

void App::refreshSelectedBlock() {
  uint8_t previousSelectedBlock = selectedBlock;
  if (selectedSlot < 0 || selectedSlot >= static_cast<int>(hotbar.size())) {
    selectedSlot = 0;
  }
  const ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
  if (stack.count == 0 || stack.type == kAir) {
    selectedBlock = kAir;
  } else {
    selectedBlock = stack.type;
  }
  if (selectedBlock != previousSelectedBlock) {
    torchLightsCacheValid = false;
    torchLightRefreshTimer = 0.0f;
  }
}

void App::showSelectedItemToast() {
  showToast(displayNameForBlock(selectedBlock, appliedSettings.language == 1), 2.0f);
}

void App::showToast(const std::string& text, float duration) {
  selectedItemToastText = text;
  selectedItemToastTimer = std::max(0.0f, duration);
  uiDirty = true;
}

bool App::unlockAchievement(uint8_t id) {
  AchievementId achievementId =
    static_cast<AchievementId>(std::clamp<int>(id, 0, static_cast<int>(App::kAchievementCount) - 1));
  uint32_t bit = achievementBit(achievementId);
  if ((achievementMask & bit) != 0u) {
    return false;
  }
  achievementMask |= bit;
  if (achievementPopupVisible) {
    achievementPopupQueue.push_back(static_cast<uint8_t>(achievementId));
  } else {
    activeAchievementPopupId = static_cast<uint8_t>(achievementId);
    achievementPopupVisible = true;
    achievementPopupTimer = kAchievementPopupDuration;
  }
  uiDirty = true;
  return true;
}

void App::refreshAchievementsProgress() {
  bool aprilMode = isAprilFoolsPreset(world.getGenerationSettings());
  auto blockUnderPlayerMatches = [&](uint8_t target) -> bool {
    constexpr std::array<glm::vec2, 5> kFootSamples{{
      {0.0f, 0.0f},
      {0.22f, 0.22f},
      {-0.22f, 0.22f},
      {0.22f, -0.22f},
      {-0.22f, -0.22f}
    }};

    for (const glm::vec2& offset : kFootSamples) {
      int x = static_cast<int>(std::floor(playerPos.x + offset.x));
      int y = static_cast<int>(std::floor(playerPos.y - 0.08f));
      int z = static_cast<int>(std::floor(playerPos.z + offset.y));
      if (!world.inBounds(x, y, z)) {
        continue;
      }

      uint8_t block = world.getBlock(x, y, z);
      if ((block == kAir || isWaterBlock(block) || isDecorationBlock(block)) &&
          world.inBounds(x, y - 1, z)) {
        block = world.getBlock(x, y - 1, z);
      }
      if (block == target) {
        return true;
      }
    }

    return false;
  };
  auto isStoneTreeTrunkBlock = [&](int x, int y, int z) -> bool {
    if (!world.inBounds(x, y, z) || world.getBlock(x, y, z) != kStone) {
      return false;
    }

    bool verticalStone = (world.inBounds(x, y - 1, z) && world.getBlock(x, y - 1, z) == kStone) ||
                         (world.inBounds(x, y + 1, z) && world.getBlock(x, y + 1, z) == kStone);
    if (!verticalStone) {
      return false;
    }

    int exposedFaces = 0;
    constexpr std::array<glm::ivec3, 6> kNeighborOffsets{{
      {1, 0, 0},
      {-1, 0, 0},
      {0, 1, 0},
      {0, -1, 0},
      {0, 0, 1},
      {0, 0, -1}
    }};
    for (const glm::ivec3& offset : kNeighborOffsets) {
      int nx = x + offset.x;
      int ny = y + offset.y;
      int nz = z + offset.z;
      if (!world.inBounds(nx, ny, nz)) {
        continue;
      }
      uint8_t neighbor = world.getBlock(nx, ny, nz);
      if (neighbor == kAir || isWaterBlock(neighbor) || isDecorationBlock(neighbor)) {
        ++exposedFaces;
      }
    }
    if (exposedFaces == 0) {
      return false;
    }

    for (int oy = 1; oy <= 4; ++oy) {
      for (int oz = -2; oz <= 2; ++oz) {
        for (int ox = -2; ox <= 2; ++ox) {
          if (std::abs(ox) + std::abs(oz) > 3) {
            continue;
          }
          int nx = x + ox;
          int ny = y + oy;
          int nz = z + oz;
          if (!world.inBounds(nx, ny, nz)) {
            continue;
          }
          uint8_t neighbor = world.getBlock(nx, ny, nz);
          if (neighbor == kLeaves || neighbor == kWood) {
            return true;
          }
        }
      }
    }

    return false;
  };

  if (countStoredItem(kWood) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementGetWood));
  }
  if (countStoredItem(kPlanks) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftPlanks));
  }
  if (countStoredItem(kStick) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftSticks));
  }
  if (countStoredItem(kWorkbench) > 0 || hasWorkbenchAccess()) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftWorkbench));
  }
  if (countStoredItem(kWoodPickaxe) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftWoodPickaxe));
  }
  if (countStoredItem(kStone) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementGetStone));
  }
  if (countStoredItem(kStonePickaxe) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftStonePickaxe));
  }
  if (countStoredItem(kFurnace) > 0 || hasFurnaceAccess()) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftFurnace));
  }
  if (countStoredItem(kTorch) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftTorch));
  }
  if (playerPos.y <= 36.0f) {
    unlockAchievement(static_cast<uint8_t>(kAchievementFindCave));
  }
  if (countStoredItem(kIronIngot) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementSmeltIron));
  }
  if (countStoredItem(kIronPickaxe) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementCraftIronPickaxe));
  }
  if (countStoredItem(kDiamond) > 0) {
    unlockAchievement(static_cast<uint8_t>(kAchievementGetDiamond));
  }
  if (aprilMode && blockUnderPlayerMatches(kGrass)) {
    unlockAchievement(static_cast<uint8_t>(kAchievementTouchSuspiciousGrass));
  }
  if (aprilMode && intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.20f, 0.0f))) {
    unlockAchievement(static_cast<uint8_t>(kAchievementDrinkTheFloor));
  }
  if (aprilMode) {
    int originX = static_cast<int>(std::floor(playerPos.x));
    int originY = static_cast<int>(std::floor(playerPos.y));
    int originZ = static_cast<int>(std::floor(playerPos.z));
    bool foundStoneTree = false;
    for (int oy = -1; oy <= 2 && !foundStoneTree; ++oy) {
      for (int oz = -2; oz <= 2 && !foundStoneTree; ++oz) {
        for (int ox = -2; ox <= 2; ++ox) {
          if (isStoneTreeTrunkBlock(originX + ox, originY + oy, originZ + oz)) {
            foundStoneTree = true;
            break;
          }
        }
      }
    }
    if (foundStoneTree) {
      unlockAchievement(static_cast<uint8_t>(kAchievementStoneTree));
    }
  }
}

void App::renderLoadingFrame(float progress, const std::string& message) {
  loadingWorldProgress = std::clamp(progress, 0.0f, 1.0f);
  loadingWorldMessage = message;
  uiDirty = true;

  if (!vkReady || !window) {
    return;
  }

  glfwPollEvents();

  if (uiDirty) {
    rebuildUiMesh();
  }
  composeMeshData();
  vk.updateMesh(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
  uiDirty = false;

  glm::vec3 eye = glm::vec3(6.0f, 62.0f, -18.0f);
  glm::vec3 front = glm::normalize(glm::vec3(0.2f, -0.16f, 1.0f));
  glm::mat4 view = glm::lookAt(eye, eye + front, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 proj = glm::perspective(glm::radians(70.0f),
                                    width / static_cast<float>(height),
                                    0.1f,
                                    200.0f);
  proj[1][1] *= -1.0f;
  vk.setEnvironmentState(dayLightFactor,
                         weatherIntensity,
                         dayCycleTime,
                         isAprilFoolsPreset(world.getGenerationSettings()));
  vk.setCameraWorldState(eye, front, false);
  vk.setCameraMatrices(view, proj);
  vk.drawFrame();
}

bool App::addToInventory(uint8_t type, uint16_t count, uint16_t* outRemaining) {
  if (type == kAir || count == 0) {
    if (outRemaining) {
      *outRemaining = 0;
    }
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
    refreshAchievementsProgress();
    uiDirty = true;
  }

  if (outRemaining) {
    *outRemaining = count;
  }

  return count == 0;
}

int App::countStoredItem(uint8_t type) const {
  int total = 0;
  auto countIn = [&](const auto& slots) {
    for (const ItemStack& slot : slots) {
      if (slot.type == type && slot.count > 0) {
        total += slot.count;
      }
    }
  };
  countIn(hotbar);
  countIn(inventory);
  countIn(craftingSlots);
  if (cursorStack.type == type && cursorStack.count > 0) {
    total += cursorStack.count;
  }
  return total;
}

bool App::consumeStoredItem(uint8_t type, uint16_t count) {
  if (type == kAir || count == 0) {
    return true;
  }
  if (countStoredItem(type) < count) {
    return false;
  }

  auto consumeFrom = [&](auto& slots) {
    for (ItemStack& slot : slots) {
      if (count == 0) {
        return;
      }
      if (slot.type != type || slot.count == 0) {
        continue;
      }
      uint16_t used = std::min<uint16_t>(slot.count, count);
      slot.count = static_cast<uint16_t>(slot.count - used);
      count = static_cast<uint16_t>(count - used);
      if (slot.count == 0) {
        slot.type = kAir;
      }
    }
  };

  if (cursorStack.type == type && cursorStack.count > 0 && count > 0) {
    uint16_t used = std::min<uint16_t>(cursorStack.count, count);
    cursorStack.count = static_cast<uint16_t>(cursorStack.count - used);
    count = static_cast<uint16_t>(count - used);
    if (cursorStack.count == 0) {
      cursorStack.type = kAir;
    }
  }
  consumeFrom(hotbar);
  consumeFrom(inventory);
  consumeFrom(craftingSlots);
  refreshSelectedBlock();
  uiDirty = true;
  return count == 0;
}

int App::activeCraftGridSize() const {
  return workbenchOpen ? 3 : 2;
}

bool App::tryReturnCraftingItemsToInventory() {
  bool allReturned = true;
  glm::ivec3 dropPos(static_cast<int>(std::floor(playerPos.x)),
                     static_cast<int>(std::floor(playerPos.y + 1.0f)),
                     static_cast<int>(std::floor(playerPos.z)));
  for (ItemStack& slot : craftingSlots) {
    if (slot.type == kAir || slot.count == 0) {
      continue;
    }
    uint16_t remaining = 0;
    addToInventory(slot.type, slot.count, &remaining);
    if (remaining > 0) {
      allReturned = false;
      for (uint16_t i = 0; i < remaining; ++i) {
        spawnDroppedItem(slot.type, dropPos);
      }
    }
    slot.type = kAir;
    slot.count = 0;
  }
  return allReturned;
}

void App::returnCraftingItemsToInventory() {
  (void)tryReturnCraftingItemsToInventory();
}

bool App::hasWorkbenchAccess() const {
  return workbenchOpen && canUseWorkbenchAt(activeWorkbenchBlock);
}

bool App::hasFurnaceAccess() const {
  return furnaceOpen && canUseFurnaceAt(activeFurnaceBlock);
}

bool App::canUseWorkbenchAt(const glm::ivec3& block) const {
  if (!world.inBounds(block.x, block.y, block.z)) {
    return false;
  }
  if (!isWorkbenchBlock(world.getBlock(block.x, block.y, block.z))) {
    return false;
  }

  glm::vec3 workbenchCenter(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f);
  glm::vec3 playerCenter = playerPos + glm::vec3(0.0f, 0.9f, 0.0f);
  glm::vec3 delta = workbenchCenter - playerCenter;
  if (std::abs(delta.y) > 3.0f) {
    return false;
  }
  return glm::dot(delta, delta) <= 25.0f;
}

bool App::canUseFurnaceAt(const glm::ivec3& block) const {
  if (!world.inBounds(block.x, block.y, block.z)) {
    return false;
  }
  if (!isFurnaceBlock(world.getBlock(block.x, block.y, block.z))) {
    return false;
  }

  glm::vec3 furnaceCenter(block.x + 0.5f, block.y + 0.5f, block.z + 0.5f);
  glm::vec3 playerCenter = playerPos + glm::vec3(0.0f, 0.9f, 0.0f);
  glm::vec3 delta = furnaceCenter - playerCenter;
  if (std::abs(delta.y) > 3.0f) {
    return false;
  }
  return glm::dot(delta, delta) <= 25.0f;
}

void App::spawnDroppedItem(uint8_t type, const glm::ivec3& blockPos) {
  if (type == kAir || isWaterBlock(type)) {
    return;
  }

  float noise = std::sin(static_cast<float>(blockPos.x) * 12.9898f +
                         static_cast<float>(blockPos.y) * 78.233f +
                         static_cast<float>(blockPos.z) * 37.719f);
  float seed = fract01(noise * 43758.5453f);
  float angle = seed * 6.2831853f;
  float speed = 0.95f + 0.45f * seed;
  glm::vec3 itemPos(static_cast<float>(blockPos.x) + 0.5f,
                    static_cast<float>(blockPos.y) + 0.18f,
                    static_cast<float>(blockPos.z) + 0.5f);
  glm::vec3 itemVelocity(std::cos(angle) * speed,
                         2.8f + seed * 0.4f,
                         std::sin(angle) * speed);
  spawnDroppedItemWithPhysics(type, itemPos, itemVelocity, 0.26f);
}

void App::spawnDroppedStack(const ItemStack& stack, const glm::ivec3& blockPos) {
  if (stack.type == kAir || stack.count == 0 || isWaterBlock(stack.type)) {
    return;
  }

  float noise = std::sin(static_cast<float>(blockPos.x) * 19.913f +
                         static_cast<float>(blockPos.y) * 63.137f +
                         static_cast<float>(blockPos.z) * 41.611f +
                         static_cast<float>(stack.type) * 0.731f +
                         static_cast<float>(stack.count) * 0.193f);
  float seed = fract01(noise * 43758.5453f);
  float angle = seed * 6.2831853f;
  float radialOffset = 0.05f + seed * 0.09f;
  float lateralSpeed = 0.30f + seed * 0.32f;
  glm::vec3 itemPos(static_cast<float>(blockPos.x) + 0.5f + std::cos(angle) * radialOffset,
                    static_cast<float>(blockPos.y) + 0.24f + seed * 0.08f,
                    static_cast<float>(blockPos.z) + 0.5f + std::sin(angle) * radialOffset);
  glm::vec3 itemVelocity(std::cos(angle) * lateralSpeed,
                         2.9f + seed * 0.45f,
                         std::sin(angle) * lateralSpeed);
  spawnDroppedItemWithPhysics(stack.type, itemPos, itemVelocity, 0.26f, stack.count);
}

void App::spawnDroppedItemWithPhysics(uint8_t type,
                                      const glm::vec3& position,
                                      const glm::vec3& velocity,
                                      float pickupDelay,
                                      uint16_t count) {
  if (type == kAir || isWaterBlock(type) || count == 0) {
    return;
  }

  if (droppedItems.size() >= 384) {
    droppedItems.erase(droppedItems.begin());
  }

  float spinNoise = std::sin(position.x * 12.9898f +
                             position.y * 78.233f +
                             position.z * 37.719f);
  float spinSeed = fract01(spinNoise * 43758.5453f);

  DroppedItemEntity item;
  item.type = type;
  item.count = count;
  item.pos = position;
  item.vel = velocity;
  item.age = 0.0f;
  item.pickupDelay = std::max(0.0f, pickupDelay);
  item.spinPhase = spinSeed * 6.2831853f;
  item.onGround = false;
  droppedItems.push_back(item);

  droppedItemMeshTimer = kDroppedItemMeshUpdateInterval;
}

bool App::droppedItemCollidesAt(const glm::vec3& pos) const {
  glm::vec3 min = {pos.x - kDroppedItemHalfSize,
                   pos.y - kDroppedItemHalfHeight,
                   pos.z - kDroppedItemHalfSize};
  glm::vec3 max = {pos.x + kDroppedItemHalfSize,
                   pos.y + kDroppedItemHalfHeight,
                   pos.z + kDroppedItemHalfSize};

  int minX = static_cast<int>(std::floor(min.x));
  int maxX = static_cast<int>(std::floor(max.x));
  int minY = static_cast<int>(std::floor(min.y));
  int maxY = static_cast<int>(std::floor(max.y));
  int minZ = static_cast<int>(std::floor(min.z));
  int maxZ = static_cast<int>(std::floor(max.z));

  auto chunkReadyAt = [&](int worldX, int worldZ) {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    return world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise;
  };

  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z)) {
          return true;
        }
        if (!chunkReadyAt(x, z)) {
          return true;
        }
        uint8_t block = world.getBlock(x, y, z);
        if (solidBlockIntersectsAabb(block, x, y, z, min, max)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::droppedItemIntersectsWater(const glm::vec3& pos, float* outSurfaceY) const {
  glm::vec3 min = {pos.x - kDroppedItemHalfSize,
                   pos.y - kDroppedItemHalfHeight,
                   pos.z - kDroppedItemHalfSize};
  glm::vec3 max = {pos.x + kDroppedItemHalfSize,
                   pos.y + kDroppedItemHalfHeight,
                   pos.z + kDroppedItemHalfSize};

  int minX = static_cast<int>(std::floor(min.x));
  int maxX = static_cast<int>(std::floor(max.x));
  int minY = static_cast<int>(std::floor(min.y));
  int maxY = static_cast<int>(std::floor(max.y));
  int minZ = static_cast<int>(std::floor(min.z));
  int maxZ = static_cast<int>(std::floor(max.z));

  auto chunkReadyAt = [&](int worldX, int worldZ) {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    return world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise;
  };

  bool foundWater = false;
  float surfaceY = std::numeric_limits<float>::lowest();
  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z) || !chunkReadyAt(x, z)) {
          continue;
        }

        uint8_t block = world.getBlock(x, y, z);
        if (!isWaterVolumeBlock(block)) {
          continue;
        }

        float waterTop = static_cast<float>(y) + 1.0f;
        if (isWaterBlock(block)) {
          uint8_t level = waterLevelFromBlock(block);
          if (level != 255) {
            waterTop -= static_cast<float>(level) / 8.0f;
          }
        }

        if (max.y <= static_cast<float>(y) || min.y >= waterTop) {
          continue;
        }

        foundWater = true;
        surfaceY = std::max(surfaceY, waterTop);
      }
    }
  }

  if (foundWater && outSurfaceY) {
    *outSurfaceY = surfaceY;
  }
  return foundWater;
}

void App::updateDroppedItems(float deltaTime) {
  if (droppedItems.empty()) {
    return;
  }

  if (deltaTime <= 0.0f) {
    return;
  }

  droppedItemMeshTimer += deltaTime;
  glm::vec3 pickupCenter = playerPos + glm::vec3(0.0f, 0.95f, 0.0f);

  for (size_t i = 0; i < droppedItems.size();) {
    DroppedItemEntity& item = droppedItems[i];
    item.age += deltaTime;
    item.pickupDelay = std::max(0.0f, item.pickupDelay - deltaTime);

    float waterSurfaceY = 0.0f;
    bool inWater = droppedItemIntersectsWater(item.pos, &waterSurfaceY);
    if (inWater) {
      // Pull the item gently toward the top of the water column instead of letting it sink.
      float targetFloatY = waterSurfaceY - kDroppedItemHalfHeight + 0.025f;
      float buoyancy = std::clamp((targetFloatY - item.pos.y) * 30.0f, -4.0f, 14.0f);
      item.vel.y += (-2.4f + buoyancy) * deltaTime;
      float verticalDamping = std::clamp(1.0f - deltaTime * 4.5f, 0.0f, 1.0f);
      item.vel.y *= verticalDamping;
      item.vel.y = std::clamp(item.vel.y, -1.8f, 2.4f);
    } else {
      item.vel.y += kDroppedItemGravity * deltaTime;
      item.vel.y = std::max(item.vel.y, -11.5f);
    }

    glm::vec3 next = item.pos;
    next.x += item.vel.x * deltaTime;
    if (droppedItemCollidesAt(next)) {
      next.x = item.pos.x;
      item.vel.x *= -0.2f;
      if (std::abs(item.vel.x) < 0.05f) {
        item.vel.x = 0.0f;
      }
    }

    next.y += item.vel.y * deltaTime;
    if (droppedItemCollidesAt(next)) {
      if (item.vel.y < 0.0f) {
        item.onGround = true;
      }
      next.y = item.pos.y;
      item.vel.y *= -0.24f;
      if (std::abs(item.vel.y) < 0.28f) {
        item.vel.y = 0.0f;
      }
    } else {
      item.onGround = false;
    }

    next.z += item.vel.z * deltaTime;
    if (droppedItemCollidesAt(next)) {
      next.z = item.pos.z;
      item.vel.z *= -0.2f;
      if (std::abs(item.vel.z) < 0.05f) {
        item.vel.z = 0.0f;
      }
    }

    float drag = inWater ? 0.18f : (item.onGround ? 0.24f : 0.05f);
    float dragFactor = std::clamp(1.0f - drag * deltaTime * 8.0f, 0.0f, 1.0f);
    item.vel.x *= dragFactor;
    item.vel.z *= dragFactor;

    item.pos = next;

    bool removeItem = false;
    if (item.pickupDelay <= 0.0f) {
      glm::vec3 toPlayer = pickupCenter - item.pos;
      float distSq = glm::dot(toPlayer, toPlayer);
      if (distSq <= kDroppedItemPickupRadius * kDroppedItemPickupRadius) {
        uint16_t remaining = 0;
        addToInventory(item.type, item.count, &remaining);
        if (remaining == 0) {
          removeItem = true;
        } else {
          item.count = remaining;
        }
      }
    }

    if (item.age >= 180.0f) {
      removeItem = true;
    }

    if (removeItem) {
      droppedItems[i] = droppedItems.back();
      droppedItems.pop_back();
      droppedItemMeshTimer = kDroppedItemMeshUpdateInterval;
      continue;
    }

    ++i;
  }
}

void App::syncDroppedItemMesh(bool force) {
  if (force) {
    droppedItemMeshTimer = kDroppedItemMeshUpdateInterval;
  }

  if (droppedItems.empty()) {
    if (droppedItemMeshUploaded) {
      pendingWorldChunkUploads.erase(kDroppedItemsMeshKey);
      pendingWorldChunkRemovals.insert(kDroppedItemsMeshKey);
      droppedItemMeshUploaded = false;
    }
    return;
  }

  if (!force && droppedItemMeshTimer < kDroppedItemMeshUpdateInterval) {
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(droppedItems.size() * 24);
  indices.reserve(droppedItems.size() * 36);

  for (const DroppedItemEntity& item : droppedItems) {
    if (item.count == 0 || item.type == kAir || isWaterBlock(item.type)) {
      continue;
    }
    float bob = 0.14f + std::sin(item.age * 3.9f + item.spinPhase) * 0.07f;
    float spin = item.age * 2.8f + item.spinPhase;
    glm::vec3 center = item.pos + glm::vec3(0.0f, bob, 0.0f);
    if (shouldRenderDroppedItemAsSprite(item.type)) {
      appendDroppedItemSprite(vertices, indices, center, spin, item.type);
    } else {
      appendDroppedItemCube(vertices, indices, center, spin, item.type);
    }
  }

  if (vertices.empty() || indices.empty()) {
    if (droppedItemMeshUploaded) {
      pendingWorldChunkUploads.erase(kDroppedItemsMeshKey);
      pendingWorldChunkRemovals.insert(kDroppedItemsMeshKey);
      droppedItemMeshUploaded = false;
    }
    droppedItemMeshTimer = 0.0f;
    return;
  }

  VulkanContext::WorldChunkMeshUpload upload;
  upload.key = kDroppedItemsMeshKey;
  upload.vertices = std::move(vertices);
  upload.indices = std::move(indices);

  pendingWorldChunkRemovals.erase(kDroppedItemsMeshKey);
  pendingWorldChunkUploads[kDroppedItemsMeshKey] = std::move(upload);
  droppedItemMeshUploaded = true;
  droppedItemMeshTimer = 0.0f;
}

void App::populateSheepForSession() {
  sheepEntities.clear();
  sheepMeshUploaded = false;
  sheepMeshTimer = kSheepMeshUpdateInterval;
  sheepSpawnTimer = 18.0f;
  sheepRngState = static_cast<uint32_t>(world.getSeed()) ^ 0x51A9B4C3u;
  if (sheepRngState == 0u) {
    sheepRngState = 0x4F1BBCDCu;
  }

  auto trySpawnOne = [&](const glm::vec3& anchor, float maxRadius) -> bool {
    for (int attempt = 0; attempt < 180; ++attempt) {
      float angle = nextUnitRandom(sheepRngState) * kTau;
      float radius = (0.22f + 0.78f * nextUnitRandom(sheepRngState)) * maxRadius;
      int x = static_cast<int>(std::floor(anchor.x + std::cos(angle) * radius));
      int z = static_cast<int>(std::floor(anchor.z + std::sin(angle) * radius));
      if (!world.inBounds(x, 2, z)) {
        continue;
      }

      int spawnY = -1;
      for (int y = world.height() - 3; y >= 2; --y) {
        uint8_t ground = world.getBlock(x, y, z);
        uint8_t feet = world.getBlock(x, y + 1, z);
        uint8_t head = world.getBlock(x, y + 2, z);
        if (ground != kGrass && ground != kDirt) {
          continue;
        }
        if (feet != kAir || head != kAir) {
          continue;
        }
        spawnY = y;
        break;
      }
      if (spawnY < 0) {
        continue;
      }

      glm::vec3 spawnPos(static_cast<float>(x) + 0.5f,
                         static_cast<float>(spawnY) + 1.0f,
                         static_cast<float>(z) + 0.5f);
      glm::vec2 toPlayer(spawnPos.x - playerPos.x, spawnPos.z - playerPos.z);
      if (glm::dot(toPlayer, toPlayer) < 49.0f) {
        continue;
      }
      if (sheepCollidesAt(spawnPos) || sheepIntersectsWater(spawnPos)) {
        continue;
      }

      bool overlapsOtherSheep = false;
      for (const SheepEntity& sheep : sheepEntities) {
        glm::vec2 delta(spawnPos.x - sheep.pos.x, spawnPos.z - sheep.pos.z);
        if (glm::dot(delta, delta) < 12.25f) {
          overlapsOtherSheep = true;
          break;
        }
      }
      if (overlapsOtherSheep) {
        continue;
      }

      SheepEntity sheep;
      sheep.pos = spawnPos;
      sheep.yaw = nextUnitRandom(sheepRngState) * 360.0f;
      sheep.targetYaw = sheep.yaw;
      sheep.decisionTimer = 0.4f + nextUnitRandom(sheepRngState) * 1.4f;
      sheep.walkSpeed = 0.0f;
      sheep.grazeTimer = 0.6f + nextUnitRandom(sheepRngState) * 1.4f;
      sheep.animPhase = nextUnitRandom(sheepRngState) * kTau;
      sheep.health = 6;
      sheep.onGround = false;
      sheepEntities.push_back(sheep);
      return true;
    }
    return false;
  };

  while (sheepEntities.size() < kTargetSheepCount) {
    if (!trySpawnOne(playerPos, kSheepSpawnRadius)) {
      break;
    }
  }
}

void App::updateSheep(float deltaTime) {
  if (deltaTime <= 0.0f) {
    return;
  }

  sheepMeshTimer += deltaTime;
  sheepSpawnTimer = std::max(0.0f, sheepSpawnTimer - deltaTime);
  if (sheepEntities.size() < kTargetSheepCount && sheepSpawnTimer <= 0.0f) {
    sheepSpawnTimer = 18.0f;
    bool spawned = false;
    for (int attempt = 0; attempt < 140 && !spawned; ++attempt) {
      float angle = nextUnitRandom(sheepRngState) * kTau;
      float radius = (0.18f + 0.82f * nextUnitRandom(sheepRngState)) * kSheepSpawnRadius;
      int x = static_cast<int>(std::floor(playerPos.x + std::cos(angle) * radius));
      int z = static_cast<int>(std::floor(playerPos.z + std::sin(angle) * radius));
      if (!world.inBounds(x, 2, z)) {
        continue;
      }

      int spawnY = -1;
      for (int y = world.height() - 3; y >= 2; --y) {
        uint8_t ground = world.getBlock(x, y, z);
        if (ground != kGrass && ground != kDirt) {
          continue;
        }
        if (world.getBlock(x, y + 1, z) != kAir || world.getBlock(x, y + 2, z) != kAir) {
          continue;
        }
        spawnY = y;
        break;
      }
      if (spawnY < 0) {
        continue;
      }

      glm::vec3 spawnPos(static_cast<float>(x) + 0.5f,
                         static_cast<float>(spawnY) + 1.0f,
                         static_cast<float>(z) + 0.5f);
      glm::vec2 toPlayer(spawnPos.x - playerPos.x, spawnPos.z - playerPos.z);
      if (glm::dot(toPlayer, toPlayer) < 49.0f ||
          sheepCollidesAt(spawnPos) ||
          sheepIntersectsWater(spawnPos)) {
        continue;
      }

      bool overlapsOtherSheep = false;
      for (const SheepEntity& sheep : sheepEntities) {
        glm::vec2 delta(spawnPos.x - sheep.pos.x, spawnPos.z - sheep.pos.z);
        if (glm::dot(delta, delta) < 12.25f) {
          overlapsOtherSheep = true;
          break;
        }
      }
      if (overlapsOtherSheep) {
        continue;
      }

      SheepEntity sheep;
      sheep.pos = spawnPos;
      sheep.yaw = nextUnitRandom(sheepRngState) * 360.0f;
      sheep.targetYaw = sheep.yaw;
      sheep.decisionTimer = 0.4f + nextUnitRandom(sheepRngState) * 1.4f;
      sheep.walkSpeed = 0.0f;
      sheep.grazeTimer = 0.7f + nextUnitRandom(sheepRngState) * 1.1f;
      sheep.animPhase = nextUnitRandom(sheepRngState) * kTau;
      sheep.health = 6;
      sheep.onGround = false;
      sheepEntities.push_back(sheep);
      sheepMeshTimer = kSheepMeshUpdateInterval;
      spawned = true;
    }
    if (!spawned) {
      sheepSpawnTimer = 8.0f;
    }
  }

  for (size_t i = 0; i < sheepEntities.size();) {
    SheepEntity& sheep = sheepEntities[i];
    sheep.decisionTimer -= deltaTime;
    if (sheep.grazeTimer > 0.0f) {
      sheep.grazeTimer = std::max(0.0f, sheep.grazeTimer - deltaTime);
    }

    if (sheep.decisionTimer <= 0.0f) {
      sheep.decisionTimer = 1.4f + nextUnitRandom(sheepRngState) * 3.4f;
      if (nextUnitRandom(sheepRngState) < 0.34f) {
        sheep.walkSpeed = 0.0f;
        sheep.grazeTimer = 0.9f + nextUnitRandom(sheepRngState) * 1.8f;
      } else {
        sheep.walkSpeed = 0.45f + nextUnitRandom(sheepRngState) * 0.85f;
        sheep.targetYaw = sheep.yaw + (nextUnitRandom(sheepRngState) * 160.0f - 80.0f);
        sheep.grazeTimer = 0.0f;
      }
    }

    float turnDelta = wrapDegrees(sheep.targetYaw - sheep.yaw);
    float turnStep = 180.0f * deltaTime;
    sheep.yaw += std::clamp(turnDelta, -turnStep, turnStep);

    glm::vec3 moveDir(std::cos(glm::radians(sheep.yaw)), 0.0f, std::sin(glm::radians(sheep.yaw)));
    if (sheep.walkSpeed > 0.05f) {
      sheep.vel.x = moveDir.x * sheep.walkSpeed;
      sheep.vel.z = moveDir.z * sheep.walkSpeed;
    } else {
      float groundDamp = std::clamp(1.0f - deltaTime * 8.0f, 0.0f, 1.0f);
      sheep.vel.x *= groundDamp;
      sheep.vel.z *= groundDamp;
    }

    bool inWater = sheepIntersectsWater(sheep.pos + glm::vec3(0.0f, 0.1f, 0.0f));
    if (inWater) {
      sheep.walkSpeed = kSheepMaxSpeed;
      sheep.targetYaw += 130.0f + nextUnitRandom(sheepRngState) * 90.0f;
      sheep.vel.y = std::max(sheep.vel.y, 3.2f);
    } else {
      sheep.vel.y += kSheepGravity * deltaTime;
      sheep.vel.y = std::max(sheep.vel.y, -11.0f);
    }

    glm::vec3 next = sheep.pos;
    next.x += sheep.vel.x * deltaTime;
    if (sheepCollidesAt(next)) {
      next.x = sheep.pos.x;
      sheep.vel.x = 0.0f;
      sheep.targetYaw += 100.0f + nextUnitRandom(sheepRngState) * 80.0f;
      sheep.walkSpeed = 0.70f + nextUnitRandom(sheepRngState) * 0.35f;
      sheep.decisionTimer = 0.5f + nextUnitRandom(sheepRngState) * 0.8f;
    }

    next.y += sheep.vel.y * deltaTime;
    if (sheepCollidesAt(next)) {
      if (sheep.vel.y < 0.0f) {
        sheep.onGround = true;
      }
      next.y = sheep.pos.y;
      sheep.vel.y = 0.0f;
    } else {
      sheep.onGround = false;
    }

    next.z += sheep.vel.z * deltaTime;
    if (sheepCollidesAt(next)) {
      next.z = sheep.pos.z;
      sheep.vel.z = 0.0f;
      sheep.targetYaw += 100.0f + nextUnitRandom(sheepRngState) * 80.0f;
      sheep.walkSpeed = 0.70f + nextUnitRandom(sheepRngState) * 0.35f;
      sheep.decisionTimer = 0.5f + nextUnitRandom(sheepRngState) * 0.8f;
    }

    sheep.pos = next;
    sheep.animPhase += deltaTime * (3.4f + sheep.walkSpeed * 4.0f);

    if (sheep.pos.y < -4.0f) {
      sheepEntities[i] = sheepEntities.back();
      sheepEntities.pop_back();
      sheepMeshTimer = kSheepMeshUpdateInterval;
      continue;
    }
    ++i;
  }
}

void App::syncSheepMesh(bool force) {
  if (force) {
    sheepMeshTimer = kSheepMeshUpdateInterval;
  }

  if (sheepEntities.empty()) {
    if (sheepMeshUploaded) {
      pendingWorldChunkUploads.erase(kSheepMeshKey);
      pendingWorldChunkRemovals.insert(kSheepMeshKey);
      sheepMeshUploaded = false;
    }
    return;
  }

  if (!force && sheepMeshTimer < kSheepMeshUpdateInterval) {
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(sheepEntities.size() * 168);
  indices.reserve(sheepEntities.size() * 252);

  for (const SheepEntity& sheep : sheepEntities) {
    appendSheepEntityMesh(vertices,
                          indices,
                          sheep.pos,
                          sheep.yaw,
                          sheep.walkSpeed,
                          sheep.animPhase,
                          sheep.grazeTimer);
  }

  if (vertices.empty() || indices.empty()) {
    if (sheepMeshUploaded) {
      pendingWorldChunkUploads.erase(kSheepMeshKey);
      pendingWorldChunkRemovals.insert(kSheepMeshKey);
      sheepMeshUploaded = false;
    }
    sheepMeshTimer = 0.0f;
    return;
  }

  VulkanContext::WorldChunkMeshUpload upload;
  upload.key = kSheepMeshKey;
  upload.vertices = std::move(vertices);
  upload.indices = std::move(indices);
  pendingWorldChunkRemovals.erase(kSheepMeshKey);
  pendingWorldChunkUploads[kSheepMeshKey] = std::move(upload);
  sheepMeshUploaded = true;
  sheepMeshTimer = 0.0f;
}

bool App::sheepCollidesAt(const glm::vec3& pos) const {
  glm::vec3 min = {pos.x - kSheepHalfWidth, pos.y, pos.z - kSheepHalfLength};
  glm::vec3 max = {pos.x + kSheepHalfWidth, pos.y + kSheepHeight, pos.z + kSheepHalfLength};

  int minX = static_cast<int>(std::floor(min.x));
  int maxX = static_cast<int>(std::floor(max.x));
  int minY = static_cast<int>(std::floor(min.y));
  int maxY = static_cast<int>(std::floor(max.y));
  int minZ = static_cast<int>(std::floor(min.z));
  int maxZ = static_cast<int>(std::floor(max.z));

  auto chunkReadyAt = [&](int worldX, int worldZ) {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    return world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise;
  };

  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z) || !chunkReadyAt(x, z)) {
          return true;
        }
        uint8_t block = world.getBlock(x, y, z);
        if (solidBlockIntersectsAabb(block, x, y, z, min, max)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::sheepIntersectsWater(const glm::vec3& pos) const {
  glm::vec3 min = {pos.x - kSheepHalfWidth, pos.y, pos.z - kSheepHalfLength};
  glm::vec3 max = {pos.x + kSheepHalfWidth, pos.y + kSheepHeight * 0.75f, pos.z + kSheepHalfLength};

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
          continue;
        }
        if (isWaterVolumeBlock(world.getBlock(x, y, z))) {
          return true;
        }
      }
    }
  }

  return false;
}

App::SheepHit App::raycastSheep(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const {
  SheepHit result;
  glm::vec3 rayDir = glm::normalize(dir);
  float closestDist = maxDist;

  for (size_t i = 0; i < sheepEntities.size(); ++i) {
    const SheepEntity& sheep = sheepEntities[i];
    glm::vec3 minCorner(sheep.pos.x - kSheepHalfWidth,
                        sheep.pos.y,
                        sheep.pos.z - kSheepHalfLength);
    glm::vec3 maxCorner(sheep.pos.x + kSheepHalfWidth,
                        sheep.pos.y + kSheepHeight,
                        sheep.pos.z + kSheepHalfLength);
    float hitDistance = 0.0f;
    if (!rayIntersectsAabb(origin, rayDir, minCorner, maxCorner, maxDist, hitDistance)) {
      continue;
    }
    if (hitDistance > closestDist) {
      continue;
    }
    closestDist = hitDistance;
    result.hit = true;
    result.index = i;
    result.distance = hitDistance;
    result.point = origin + rayDir * hitDistance;
  }

  return result;
}

void App::damageSheep(size_t sheepIndex, int amount, const glm::vec3& hitDir) {
  if (sheepIndex >= sheepEntities.size() || amount <= 0) {
    return;
  }

  SheepEntity& sheep = sheepEntities[sheepIndex];
  sheep.health -= amount;
  glm::vec3 knockback(hitDir.x, 0.0f, hitDir.z);
  if (glm::dot(knockback, knockback) < 0.0001f) {
    knockback = glm::vec3(1.0f, 0.0f, 0.0f);
  } else {
    knockback = glm::normalize(knockback);
  }
  sheep.vel.x = knockback.x * 2.8f;
  sheep.vel.z = knockback.z * 2.8f;
  sheep.vel.y = std::max(sheep.vel.y, 2.8f);
  sheep.walkSpeed = kSheepMaxSpeed;
  sheep.targetYaw = glm::degrees(std::atan2(knockback.z, knockback.x));
  sheep.decisionTimer = 1.2f + nextUnitRandom(sheepRngState) * 1.8f;
  sheep.grazeTimer = 0.0f;

  if (sheep.health > 0) {
    sheepMeshTimer = kSheepMeshUpdateInterval;
    return;
  }

  int rawCount = 1 + (nextUnitRandom(sheepRngState) > 0.45f ? 1 : 0);
  int woolCount = 1 + static_cast<int>(nextUnitRandom(sheepRngState) * 3.0f);
  glm::vec3 dropOrigin = sheep.pos + glm::vec3(0.0f, 0.38f, 0.0f);

  for (int i = 0; i < rawCount; ++i) {
    float angle = nextUnitRandom(sheepRngState) * kTau;
    float speed = 0.55f + nextUnitRandom(sheepRngState) * 0.45f;
    spawnDroppedItemWithPhysics(kRawMutton,
                                dropOrigin,
                                glm::vec3(std::cos(angle) * speed,
                                          2.6f + nextUnitRandom(sheepRngState) * 0.5f,
                                          std::sin(angle) * speed),
                                0.45f);
  }
  for (int i = 0; i < woolCount; ++i) {
    float angle = nextUnitRandom(sheepRngState) * kTau;
    float speed = 0.45f + nextUnitRandom(sheepRngState) * 0.35f;
    spawnDroppedItemWithPhysics(kWool,
                                dropOrigin,
                                glm::vec3(std::cos(angle) * speed,
                                          2.4f + nextUnitRandom(sheepRngState) * 0.4f,
                                          std::sin(angle) * speed),
                                0.45f);
  }

  sheepEntities[sheepIndex] = sheepEntities.back();
  sheepEntities.pop_back();
  sheepMeshTimer = kSheepMeshUpdateInterval;
}

void App::triggerFirstPersonSwing(float strength) {
  if (screenState != ScreenState::kPlaying || inventoryOpen || achievementTreeOpen || commandInputOpen) {
    return;
  }
  firstPersonSwingActive = true;
  firstPersonSwingProgress = 0.0f;
  firstPersonSwingStrength = std::clamp(strength, 0.35f, 1.35f);
}

void App::triggerFirstPersonUseAnimation(float strength) {
  if (screenState != ScreenState::kPlaying || inventoryOpen || achievementTreeOpen || commandInputOpen) {
    return;
  }
  firstPersonUseActive = true;
  firstPersonUseProgress = 0.0f;
  firstPersonUseStrength = std::clamp(strength, 0.35f, 1.20f);
}

void App::updateFirstPersonState(float deltaTime) {
  bool renderActive = screenState == ScreenState::kPlaying &&
                      !inventoryOpen &&
                      !achievementTreeOpen &&
                      !commandInputOpen;
  if (!renderActive) {
    firstPersonVisibleItem = selectedBlock;
    firstPersonEquipProgress = 0.0f;
    firstPersonSwingProgress = 0.0f;
    firstPersonSwingStrength = 1.0f;
    firstPersonSwingActive = false;
    firstPersonUseProgress = 0.0f;
    firstPersonUseStrength = 1.0f;
    firstPersonUseActive = false;
    firstPersonWalkBobAmount = 0.0f;
    return;
  }

  firstPersonIdleTime += deltaTime;

  uint8_t targetItem = selectedBlock;
  if (firstPersonVisibleItem != targetItem) {
    firstPersonEquipProgress =
      std::max(0.0f, firstPersonEquipProgress - deltaTime * kFirstPersonEquipLowerSpeed);
    if (firstPersonEquipProgress <= 0.08f) {
      firstPersonVisibleItem = targetItem;
    }
  } else {
    firstPersonEquipProgress =
      std::min(1.0f, firstPersonEquipProgress + deltaTime * kFirstPersonEquipRaiseSpeed);
  }

  if (firstPersonSwingActive) {
    firstPersonSwingProgress += deltaTime / kFirstPersonSwingDuration;
    if (firstPersonSwingProgress >= 1.0f) {
      firstPersonSwingProgress = 0.0f;
      firstPersonSwingStrength = 1.0f;
      firstPersonSwingActive = false;
    }
  } else {
    firstPersonSwingProgress = 0.0f;
  }

  if (firstPersonUseActive) {
    firstPersonUseProgress += deltaTime / kFirstPersonUseDuration;
    if (firstPersonUseProgress >= 1.0f) {
      firstPersonUseProgress = 0.0f;
      firstPersonUseStrength = 1.0f;
      firstPersonUseActive = false;
    }
  } else {
    firstPersonUseProgress = 0.0f;
  }

  float horizontalSpeed = glm::length(glm::vec2(playerVel.x, playerVel.z));
  bool swimming = intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.20f, 0.0f));
  float targetBobAmount = 0.0f;
  if (onGround) {
    targetBobAmount = std::clamp(horizontalSpeed / 6.0f, 0.0f, 1.0f);
  } else if (swimming) {
    targetBobAmount = std::clamp(horizontalSpeed / 3.2f, 0.0f, 0.42f);
  }
  float bobBlend = std::clamp(deltaTime * 8.5f, 0.0f, 1.0f);
  firstPersonWalkBobAmount = glm::mix(firstPersonWalkBobAmount, targetBobAmount, bobBlend);
  if (firstPersonWalkBobAmount > 0.001f) {
    firstPersonWalkBobPhase += deltaTime * (6.4f + horizontalSpeed * 0.55f);
  }
}

void App::syncFirstPersonMesh() {
  bool renderActive = screenState == ScreenState::kPlaying &&
                      !inventoryOpen &&
                      !achievementTreeOpen &&
                      !commandInputOpen;
  if (!renderActive || firstPersonEquipProgress <= 0.001f) {
    if (firstPersonMeshUploaded) {
      pendingWorldChunkUploads.erase(kFirstPersonMeshKey);
      pendingWorldChunkRemovals.insert(kFirstPersonMeshKey);
      firstPersonMeshUploaded = false;
    }
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(160);
  indices.reserve(240);

  float equip = std::clamp(firstPersonEquipProgress, 0.0f, 1.0f);
  float swing = std::clamp(firstPersonSwingProgress, 0.0f, 1.0f);
  float use = std::clamp(firstPersonUseProgress, 0.0f, 1.0f);
  float swingArc = std::sin(std::sqrt(swing) * (kTau * 0.5f)) * firstPersonSwingStrength;
  float swingLift = std::sin(swing * (kTau * 0.5f)) * firstPersonSwingStrength;
  float useArc = std::sin(use * (kTau * 0.5f)) * firstPersonUseStrength;
  float bobSide = std::sin(firstPersonWalkBobPhase * 1.65f) * 0.026f * firstPersonWalkBobAmount;
  float bobLift = std::abs(std::cos(firstPersonWalkBobPhase * 1.65f)) * 0.034f * firstPersonWalkBobAmount;
  float bobDepth = std::sin(firstPersonWalkBobPhase * 1.65f + 0.70f) * 0.016f * firstPersonWalkBobAmount;
  float idleSide = std::sin(firstPersonIdleTime * 1.45f) * 0.008f;
  float idleLift = std::cos(firstPersonIdleTime * 1.10f) * 0.010f;
  uint8_t itemType = firstPersonVisibleItem;
  bool toolItem = itemType != kAir && isToolItem(itemType);
  bool flatSpriteItem = itemType != kAir && shouldRenderHeldItemAsSprite(itemType);
  bool blockItem = itemType != kAir &&
                   isPlaceableItem(itemType) &&
                   !flatSpriteItem &&
                   !shouldRenderDroppedItemAsSprite(itemType);

  glm::mat4 handTransform(1.0f);
  handTransform = glm::translate(handTransform,
                                 glm::vec3(0.50f + bobSide + idleSide + swingArc * 0.03f,
                                           -0.54f + bobLift + idleLift - (1.0f - equip) * 0.56f - useArc * 0.05f,
                                           -0.92f + bobDepth + swingArc * 0.06f + useArc * 0.04f));
  handTransform = glm::rotate(handTransform, glm::radians(16.0f + bobSide * 80.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  handTransform = glm::rotate(handTransform, glm::radians(-24.0f + swingArc * 12.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  handTransform = glm::rotate(handTransform,
                              glm::radians(-28.0f - swingLift * 30.0f - useArc * 18.0f),
                              glm::vec3(1.0f, 0.0f, 0.0f));

  const glm::vec3 skinColor(0.84f, 0.72f, 0.62f);
  const glm::vec3 sleeveColor(0.20f, 0.35f, 0.66f);
  if (!toolItem) {
    appendFirstPersonPrism(vertices,
                           indices,
                           handTransform,
                           glm::vec3(0.0f, -0.12f, 0.0f),
                           glm::vec3(0.095f, 0.31f, 0.095f),
                           skinColor,
                           kTileUiWhite);
    appendFirstPersonPrism(vertices,
                           indices,
                           handTransform,
                           glm::vec3(0.0f, -0.50f, 0.0f),
                           glm::vec3(0.14f, 0.20f, 0.14f),
                           sleeveColor,
                           kTileUiWhite);
    appendFirstPersonPrism(vertices,
                           indices,
                           handTransform,
                           glm::vec3(0.0f, 0.16f, 0.0f),
                           glm::vec3(0.082f, 0.105f, 0.105f),
                           skinColor * 1.03f,
                           kTileUiWhite);
  }

  glm::mat4 itemTransform = handTransform;
  glm::vec3 itemHalfExtents(0.0f);
  glm::vec3 itemColor(0.0f);
  int itemTile = kTileUiWhite;
  float itemSpriteHalfWidth = 0.0f;
  float itemSpriteHalfHeight = 0.0f;

  if (toolItem) {
    itemTransform = glm::mat4(1.0f);
    itemTransform = glm::translate(itemTransform,
                                   glm::vec3(0.46f + bobSide * 0.55f + idleSide * 0.20f + swingArc * 0.03f,
                                             -0.48f + bobLift * 0.35f + idleLift * 0.14f -
                                               (1.0f - equip) * 0.42f - useArc * 0.05f,
                                             -0.86f + bobDepth * 0.45f + swingArc * 0.04f + useArc * 0.03f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(14.0f + swingArc * 10.0f),
                                glm::vec3(0.0f, 1.0f, 0.0f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(34.0f + swingArc * 14.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(-58.0f - swingLift * 10.0f - useArc * 18.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));
  } else if (blockItem) {
    itemTransform = glm::translate(itemTransform, glm::vec3(-0.17f, -0.02f, -0.05f));
    itemTransform = glm::rotate(itemTransform, glm::radians(-38.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    itemTransform = glm::rotate(itemTransform, glm::radians(14.0f + swingArc * 5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(-18.0f - useArc * 12.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));
    itemHalfExtents = glm::vec3(0.18f);
  } else {
    itemTransform = glm::translate(itemTransform, glm::vec3(-0.18f, 0.02f, -0.03f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(68.0f + swingArc * 5.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(-78.0f - useArc * 20.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));
    itemTransform = glm::rotate(itemTransform,
                                glm::radians(20.0f + swingArc * 8.0f),
                                glm::vec3(0.0f, 1.0f, 0.0f));

    if (itemType == kTorch || itemType == kStick) {
      itemHalfExtents = glm::vec3(0.05f, 0.30f, 0.04f);
    } else if (itemType == kIronIngot) {
      itemHalfExtents = glm::vec3(0.18f, 0.06f, 0.035f);
    } else if (itemType == kDiamond) {
      itemHalfExtents = glm::vec3(0.14f, 0.14f, 0.05f);
    } else if (itemType == kSeagrass) {
      itemSpriteHalfWidth = 0.16f;
      itemSpriteHalfHeight = 0.28f;
    } else if (itemType == kCoral) {
      itemSpriteHalfWidth = 0.18f;
      itemSpriteHalfHeight = 0.24f;
    } else if (itemType == kWool) {
      itemSpriteHalfWidth = 0.22f;
      itemSpriteHalfHeight = 0.20f;
    } else if (itemType == kRawMutton || itemType == kCookedMutton) {
      itemSpriteHalfWidth = 0.24f;
      itemSpriteHalfHeight = 0.18f;
    } else if (isToolItem(itemType)) {
      itemHalfExtents = glm::vec3(0.10f, 0.28f, 0.035f);
    } else if (itemType != kAir) {
      itemHalfExtents = glm::vec3(0.12f, 0.18f, 0.045f);
    }
  }

  if (itemType != kAir) {
    itemColor = glm::vec3(1.0f);
    itemTile = tileForBlock(itemType);
  }

  if (toolItem) {
    appendFirstPersonPickaxe(vertices, indices, itemTransform, itemType);
  } else if (flatSpriteItem) {
    appendFirstPersonSprite(vertices,
                            indices,
                            itemTransform,
                            itemSpriteHalfWidth,
                            itemSpriteHalfHeight,
                            glm::vec3(1.0f),
                            itemTile);
  } else {
    appendFirstPersonPrism(vertices,
                           indices,
                           itemTransform,
                           glm::vec3(0.0f),
                           itemHalfExtents,
                           itemColor,
                           itemTile);
  }

  if (vertices.empty() || indices.empty()) {
    if (firstPersonMeshUploaded) {
      pendingWorldChunkUploads.erase(kFirstPersonMeshKey);
      pendingWorldChunkRemovals.insert(kFirstPersonMeshKey);
      firstPersonMeshUploaded = false;
    }
    return;
  }

  VulkanContext::WorldChunkMeshUpload upload;
  upload.key = kFirstPersonMeshKey;
  upload.vertices = std::move(vertices);
  upload.indices = std::move(indices);
  pendingWorldChunkRemovals.erase(kFirstPersonMeshKey);
  pendingWorldChunkUploads[kFirstPersonMeshKey] = std::move(upload);
  firstPersonMeshUploaded = true;
}

bool App::claimLootCache(const glm::ivec3& block) {
  if (!world.inBounds(block.x, block.y, block.z) ||
      world.getBlock(block.x, block.y, block.z) != kLootCache) {
    return false;
  }

  uint32_t state = lootCacheStateForBlock(block, world.getSeed());
  std::vector<std::pair<uint8_t, uint16_t>> rewards;
  rewards.reserve(6);

  auto addReward = [&](uint8_t type, uint16_t minCount, uint16_t maxCount, float chance) {
    if (type == kAir || maxCount == 0 || nextUnitRandom(state) > chance) {
      return;
    }
    uint16_t count = minCount;
    if (maxCount > minCount) {
      count = static_cast<uint16_t>(minCount +
                                    static_cast<uint16_t>(nextUnitRandom(state) *
                                                          static_cast<float>(maxCount - minCount + 1)));
      count = std::clamp<uint16_t>(count, minCount, maxCount);
    }
    rewards.push_back({type, count});
  };

  if (nextUnitRandom(state) < 0.55f) {
    addReward(kTorch, 3, 6, 1.0f);
  } else {
    addReward(kCoalOre, 2, 4, 1.0f);
  }
  addReward(kCoalOre, 1, 3, 0.72f);
  if (nextUnitRandom(state) < 0.58f) {
    addReward(kIronOre, 1, 3, 1.0f);
  } else {
    addReward(kIronIngot, 1, 2, 1.0f);
  }
  addReward(kTorch, 2, 4, 0.36f);
  if (nextUnitRandom(state) < 0.08f) {
    addReward(kIronPickaxe, 1, 1, 1.0f);
  } else {
    addReward(kStonePickaxe, 1, 1, 0.26f);
  }
  addReward(kDiamond, 1, 1, 0.14f);

  if (rewards.empty()) {
    rewards.push_back({kTorch, 3});
  }

  audio.playCue(AudioSystem::Cue::kChestOpen, 1.0f);
  world.setBlock(block.x, block.y, block.z, kAir);
  waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);

  uint8_t featuredType = rewards.front().first;
  for (const auto& reward : rewards) {
    if (reward.first == kDiamond || reward.first == kIronPickaxe) {
      featuredType = reward.first;
      break;
    }
  }

  for (const auto& reward : rewards) {
    uint16_t remaining = 0;
    addToInventory(reward.first, reward.second, &remaining);
    while (remaining > 0) {
      spawnDroppedItem(reward.first, block);
      --remaining;
    }
  }

  showToast((appliedSettings.language == 1 ? "СУНДУК " : "CHEST ") +
            displayNameForBlock(featuredType, appliedSettings.language == 1),
            2.3f);
  return true;
}

void App::clearInteractionOverlayMesh() {
  pendingWorldChunkUploads.erase(kInteractionOverlayMeshKey);
  pendingWorldChunkRemovals.insert(kInteractionOverlayMeshKey);
  interactionOverlayUploaded = false;
  interactionOutlineActive = false;
  interactionPreviewActive = false;
  interactionPreviewType = kAir;
}

void App::updateInteractionOverlayMesh() {
  bool active = screenState == ScreenState::kPlaying &&
                !inventoryOpen &&
                !achievementTreeOpen &&
                !commandInputOpen &&
                appliedSettings.blockGuides;
  if (!active) {
    if (interactionOverlayUploaded || interactionOutlineActive || interactionPreviewActive) {
      clearInteractionOverlayMesh();
    }
    return;
  }

  glm::vec3 origin = playerPos + glm::vec3(0.0f, cameraEyeHeight(), 0.0f);
  glm::vec3 lookDir = cameraFront();
  RaycastHit hit = raycast(origin, lookDir, kBreakMaxDistance);

  bool outlineActive = false;
  glm::ivec3 outlineBlock{};
  bool previewActive = false;
  glm::ivec3 previewBlock{};
  uint8_t previewType = kAir;

  if (hit.hit) {
    glm::ivec3 targetBlock = hit.block;
    uint8_t hitType = world.getBlock(hit.block.x, hit.block.y, hit.block.z);

    if (isWaterBlock(hitType)) {
      glm::ivec3 lastVoxel = hit.block;
      for (float d = 0.06f; d <= kBreakMaxDistance; d += 0.06f) {
        glm::vec3 sample = origin + lookDir * d;
        glm::ivec3 voxel(static_cast<int>(std::floor(sample.x)),
                         static_cast<int>(std::floor(sample.y)),
                         static_cast<int>(std::floor(sample.z)));
        if (voxel == lastVoxel) {
          continue;
        }
        lastVoxel = voxel;
        if (!world.inBounds(voxel.x, voxel.y, voxel.z)) {
          break;
        }
        uint8_t candidate = world.getBlock(voxel.x, voxel.y, voxel.z);
        if (candidate == kAir || isWaterBlock(candidate)) {
          continue;
        }
        targetBlock = voxel;
        hitType = candidate;
        break;
      }
    }

    if (hitType != kAir && !isWaterBlock(hitType)) {
      outlineActive = true;
      outlineBlock = targetBlock;
    }

    if (selectedBlock != kAir && isPlaceableItem(selectedBlock)) {
      glm::ivec3 target = hit.block + hit.normal;
      glm::ivec3 placementNormal = hit.normal;
      if (isWaterBlock(world.getBlock(hit.block.x, hit.block.y, hit.block.z))) {
        bool foundPlace = false;
        glm::ivec3 lastReplaceable = hit.block;
        glm::ivec3 lastVoxel = hit.block;
        for (float d = 0.06f; d <= kBreakMaxDistance; d += 0.06f) {
          glm::vec3 sample = origin + lookDir * d;
          glm::ivec3 voxel(static_cast<int>(std::floor(sample.x)),
                           static_cast<int>(std::floor(sample.y)),
                           static_cast<int>(std::floor(sample.z)));
          if (voxel == lastVoxel) {
            continue;
          }
          lastVoxel = voxel;
          if (!world.inBounds(voxel.x, voxel.y, voxel.z)) {
            break;
          }
          uint8_t candidate = world.getBlock(voxel.x, voxel.y, voxel.z);
          if (candidate == kAir || isWaterBlock(candidate) || isDecorationBlock(candidate)) {
            lastReplaceable = voxel;
            continue;
          }

          uint8_t replaceType = world.getBlock(lastReplaceable.x, lastReplaceable.y, lastReplaceable.z);
          if (replaceType == kAir || isWaterBlock(replaceType) || isDecorationBlock(replaceType)) {
            target = lastReplaceable;
            foundPlace = true;
          }
          break;
        }
        if (!foundPlace) {
          target = hit.block;
        }
      }

      uint8_t targetType = world.getBlock(target.x, target.y, target.z);
      bool targetReplaceable = targetType == kAir ||
                               isWaterBlock(targetType) ||
                               isDecorationBlock(targetType);
      uint8_t placedType = placedBlockTypeForItem(selectedBlock, lookDir, placementNormal);
      bool supported = placedType != kAir &&
                       hasPlacementSupportAt(world, target.x, target.y, target.z, placedType, targetType);
      glm::vec3 adjustedPlayerPos = playerPos;
      if (world.inBounds(target.x, target.y, target.z) &&
          targetReplaceable &&
          supported &&
          canPlaceBlockAt(target.x, target.y, target.z, placedType, &adjustedPlayerPos)) {
        previewActive = true;
        previewBlock = target;
        previewType = placedType;
      }
    }
  }

  if (!outlineActive && !previewActive) {
    if (interactionOverlayUploaded || interactionOutlineActive || interactionPreviewActive) {
      clearInteractionOverlayMesh();
    }
    return;
  }

  bool unchanged = interactionOverlayUploaded &&
                   interactionOutlineActive == outlineActive &&
                   interactionPreviewActive == previewActive &&
                   (!outlineActive || interactionOutlineBlock == outlineBlock) &&
                   (!previewActive || (interactionPreviewBlock == previewBlock &&
                                       interactionPreviewType == previewType));
  if (unchanged) {
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(768);
  indices.reserve(1152);

  if (outlineActive) {
    glm::vec3 minCorner = glm::vec3(outlineBlock) - glm::vec3(0.012f);
    glm::vec3 maxCorner = glm::vec3(outlineBlock) + glm::vec3(1.012f);
    appendOverlayWireCube(vertices, indices, minCorner, maxCorner, 0.014f, glm::vec3(0.96f, 0.80f, 0.22f));
  }
  if (previewActive) {
    glm::vec3 tint = glm::mix(droppedItemColor(previewType), glm::vec3(0.96f), 0.28f);
    glm::vec3 minCorner = glm::vec3(previewBlock) + glm::vec3(0.028f);
    glm::vec3 maxCorner = glm::vec3(previewBlock) + glm::vec3(0.972f);
    appendOverlayWireCube(vertices, indices, minCorner, maxCorner, 0.018f, tint);
  }

  if (vertices.empty() || indices.empty()) {
    clearInteractionOverlayMesh();
    return;
  }

  VulkanContext::WorldChunkMeshUpload upload;
  upload.key = kInteractionOverlayMeshKey;
  upload.vertices = std::move(vertices);
  upload.indices = std::move(indices);
  pendingWorldChunkRemovals.erase(kInteractionOverlayMeshKey);
  pendingWorldChunkUploads[kInteractionOverlayMeshKey] = std::move(upload);
  interactionOverlayUploaded = true;
  interactionOutlineActive = outlineActive;
  interactionOutlineBlock = outlineBlock;
  interactionPreviewActive = previewActive;
  interactionPreviewBlock = previewBlock;
  interactionPreviewType = previewType;
}

void App::refreshCursorMode() {
  if (!window) {
    return;
  }
  bool lockCursor = screenState == ScreenState::kPlaying &&
                    !inventoryOpen &&
                    !achievementTreeOpen &&
                    !commandInputOpen;
  glfwSetInputMode(window, GLFW_CURSOR, lockCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void App::setAchievementTreeOpen(bool open) {
  if (achievementTreeOpen == open) {
    return;
  }
  if (open && inventoryOpen) {
    setInventoryOpen(false);
  }
  if (open && commandInputOpen) {
    commandInputOpen = false;
    commandInput.clear();
    commandBackspaceDown = false;
    commandEnterDown = false;
  }
  achievementTreeOpen = open;
  if (achievementTreeOpen && window) {
    AchievementTreeUiLayout layout = makeAchievementTreeLayout(uiLayoutWidth(), uiLayoutHeight());
    glm::vec2 clamped = clampAchievementTreeScroll(layout, {achievementTreeScrollX, achievementTreeScrollY});
    achievementTreeScrollX = clamped.x;
    achievementTreeScrollY = clamped.y;
    achievementTreeDragging = false;
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 fb = cursorToFramebuffer(xpos, ypos);
    cursorFbX = fb.x;
    cursorFbY = fb.y;
    achievementTreeDragLastX = cursorFbX;
    achievementTreeDragLastY = cursorFbY;
  }
  if (!achievementTreeOpen) {
    achievementTreeDragging = false;
    firstMouse = true;
  }
  refreshCursorMode();
  uiDirty = true;
}

void App::setInventoryOpen(bool open) {
  if (inventoryOpen == open) {
    return;
  }

  inventoryOpen = open;
  if (inventoryOpen) {
    achievementTreeOpen = false;
    commandInputOpen = false;
    commandInput.clear();
    commandBackspaceDown = false;
    commandEnterDown = false;
  }
  refreshCursorMode();
  if (inventoryOpen && window) {
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 fb = cursorToFramebuffer(xpos, ypos);
    cursorFbX = fb.x;
    cursorFbY = fb.y;
  }
  if (!inventoryOpen) {
    workbenchOpen = false;
    furnaceOpen = false;
    craftResultFlashTimer = 0.0f;
    if (cursorStack.count > 0 && cursorStack.type != kAir) {
      uint16_t remaining = 0;
      addToInventory(cursorStack.type, cursorStack.count, &remaining);
      if (remaining == 0) {
        cursorStack.type = kAir;
        cursorStack.count = 0;
      } else {
        cursorStack.count = remaining;
      }
    }
    returnCraftingItemsToInventory();
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
  float uiScale = uiScaleFactor();
  return {
    (static_cast<float>(xpos) * scaleX) / uiScale,
    (static_cast<float>(ypos) * scaleY) / uiScale
  };
}

float App::uiScaleFactor() const {
  return clampUiScaleValue(appliedSettings.uiScale);
}

int App::uiLayoutWidth() const {
  float uiWidth = std::round(static_cast<float>(std::max(width, 1)) / uiScaleFactor());
  return std::max(1, static_cast<int>(uiWidth));
}

int App::uiLayoutHeight() const {
  float uiHeight = std::round(static_cast<float>(std::max(height, 1)) / uiScaleFactor());
  return std::max(1, static_cast<int>(uiHeight));
}

bool App::hitTestHotbar(float x, float y, int& outSlot) const {
  if (width <= 0 || height <= 0) {
    return false;
  }

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (static_cast<float>(uiLayoutWidth()) - totalWidth) * 0.5f;
  const float startY = static_cast<float>(uiLayoutHeight()) - kMarginBottom - kSlotSize;

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

  float uiW = static_cast<float>(uiLayoutWidth());
  float uiH = static_cast<float>(uiLayoutHeight());
  float gridX = (uiW - gridWidth) * 0.5f;
  float gridY = (uiH - gridHeight) * 0.5f - 30.0f;
  gridY = std::clamp(gridY, 20.0f, uiH - gridHeight - 20.0f);

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

bool App::hitTestCraftInput(float x, float y, int& outIndex) const {
  if (!inventoryOpen || width <= 0 || height <= 0) {
    return false;
  }

  CraftUiLayout layout = makeCraftUiLayout(uiLayoutWidth(), uiLayoutHeight(), activeCraftGridSize());
  if (x < layout.inputX || x > layout.inputX + layout.inputWidth ||
      y < layout.inputY || y > layout.inputY + layout.inputHeight) {
    return false;
  }

  float localX = x - layout.inputX;
  float localY = y - layout.inputY;
  int col = static_cast<int>(localX / (kSlotSize + kSlotPadding));
  int row = static_cast<int>(localY / (kSlotSize + kSlotPadding));
  if (col < 0 || col >= layout.gridSize || row < 0 || row >= layout.gridSize) {
    return false;
  }

  float colX = static_cast<float>(col) * (kSlotSize + kSlotPadding);
  float rowY = static_cast<float>(row) * (kSlotSize + kSlotPadding);
  if (localX > colX + kSlotSize || localY > rowY + kSlotSize) {
    return false;
  }

  outIndex = craftingSlotIndex(row, col);
  return true;
}

bool App::hitTestCraftResult(float x, float y) const {
  if (!inventoryOpen || width <= 0 || height <= 0) {
    return false;
  }

  CraftUiLayout layout = makeCraftUiLayout(uiLayoutWidth(), uiLayoutHeight(), activeCraftGridSize());
  return x >= layout.resultX &&
         x <= layout.resultX + kSlotSize &&
         y >= layout.resultY &&
         y <= layout.resultY + kSlotSize;
}

bool App::hitTestFurnaceSlot(float x, float y, int& outIndex) const {
  if (!inventoryOpen || !furnaceOpen || width <= 0 || height <= 0) {
    return false;
  }

  FurnaceUiLayout layout = makeFurnaceUiLayout(uiLayoutWidth(), uiLayoutHeight());
  const std::array<std::pair<float, float>, App::kFurnaceSlotCount> slots = {{
    {layout.inputX, layout.inputY},
    {layout.fuelX, layout.fuelY},
    {layout.outputX, layout.outputY}
  }};

  for (size_t i = 0; i < slots.size(); ++i) {
    float sx = slots[i].first;
    float sy = slots[i].second;
    if (x >= sx && x <= sx + kSlotSize &&
        y >= sy && y <= sy + kSlotSize) {
      outIndex = static_cast<int>(i);
      return true;
    }
  }

  return false;
}

bool App::handleInventoryClick(double xpos, double ypos, bool rightClick) {
  glm::vec2 pos = cursorToFramebuffer(xpos, ypos);
  cursorFbX = pos.x;
  cursorFbY = pos.y;

  auto clearStack = [](ItemStack& stack) {
    stack.type = kAir;
    stack.count = 0;
  };

  auto interactWithSlot = [&](ItemStack& slot) {
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

    return false;
  };

  auto takeOutputFromSlot = [&](ItemStack& slot) {
    bool slotEmpty = (slot.count == 0 || slot.type == kAir);
    bool cursorEmpty = (cursorStack.count == 0 || cursorStack.type == kAir);
    if (slotEmpty) {
      return false;
    }

    if (!rightClick) {
      if (cursorEmpty) {
        cursorStack = slot;
        clearStack(slot);
        return true;
      }
      if (cursorStack.type != slot.type || cursorStack.count >= kMaxStack) {
        return false;
      }
      uint16_t space = static_cast<uint16_t>(kMaxStack - cursorStack.count);
      uint16_t toMove = std::min(space, slot.count);
      cursorStack.count = static_cast<uint16_t>(cursorStack.count + toMove);
      slot.count = static_cast<uint16_t>(slot.count - toMove);
      if (slot.count == 0) {
        slot.type = kAir;
      }
      return toMove > 0;
    }

    if (cursorEmpty) {
      cursorStack.type = slot.type;
      cursorStack.count = 1;
      slot.count = static_cast<uint16_t>(slot.count - 1);
      if (slot.count == 0) {
        slot.type = kAir;
      }
      return true;
    }
    if (cursorStack.type != slot.type || cursorStack.count >= kMaxStack) {
      return false;
    }
    cursorStack.count = static_cast<uint16_t>(cursorStack.count + 1);
    slot.count = static_cast<uint16_t>(slot.count - 1);
    if (slot.count == 0) {
      slot.type = kAir;
    }
    return true;
  };

  if (furnaceOpen) {
    int furnaceIndex = -1;
    if (hitTestFurnaceSlot(pos.x, pos.y, furnaceIndex)) {
      App::FurnaceState& furnace = furnaceStates[furnaceKeyForBlock(activeFurnaceBlock)];
      ItemStack& slot =
        furnaceIndex == 0 ? furnace.input :
        furnaceIndex == 1 ? furnace.fuel :
                            furnace.output;
      bool changed = false;

      if (furnaceIndex == 2) {
        changed = takeOutputFromSlot(slot);
      } else {
        auto canPlaceType = [&](uint8_t type) {
          if (type == kAir) {
            return false;
          }
          if (furnaceIndex == 0) {
            uint8_t resultType = kAir;
            return furnaceResultForInput(type, resultType);
          }
          return isFurnaceFuel(type);
        };

        bool slotEmpty = (slot.count == 0 || slot.type == kAir);
        bool cursorEmpty = (cursorStack.count == 0 || cursorStack.type == kAir);

        if (!rightClick) {
          if (cursorEmpty) {
            if (!slotEmpty) {
              cursorStack = slot;
              clearStack(slot);
              changed = true;
            }
          } else if (slotEmpty) {
            if (canPlaceType(cursorStack.type)) {
              slot = cursorStack;
              clearStack(cursorStack);
              changed = true;
            }
          } else if (slot.type == cursorStack.type) {
            uint16_t space = static_cast<uint16_t>(kMaxStack - slot.count);
            if (space > 0) {
              uint16_t toMove = std::min(space, cursorStack.count);
              slot.count = static_cast<uint16_t>(slot.count + toMove);
              cursorStack.count = static_cast<uint16_t>(cursorStack.count - toMove);
              if (cursorStack.count == 0) {
                cursorStack.type = kAir;
              }
              changed = toMove > 0;
            }
          } else if (canPlaceType(cursorStack.type)) {
            std::swap(slot, cursorStack);
            changed = true;
          }
        } else {
          if (cursorEmpty) {
            if (!slotEmpty) {
              uint16_t take = static_cast<uint16_t>((slot.count + 1) / 2);
              cursorStack.type = slot.type;
              cursorStack.count = take;
              slot.count = static_cast<uint16_t>(slot.count - take);
              if (slot.count == 0) {
                slot.type = kAir;
              }
              changed = true;
            }
          } else if ((slotEmpty || slot.type == cursorStack.type) &&
                     canPlaceType(cursorStack.type) &&
                     (slotEmpty || slot.count < kMaxStack)) {
            if (slotEmpty) {
              slot.type = cursorStack.type;
              slot.count = 1;
            } else {
              slot.count = static_cast<uint16_t>(slot.count + 1);
            }
            cursorStack.count = static_cast<uint16_t>(cursorStack.count - 1);
            if (cursorStack.count == 0) {
              cursorStack.type = kAir;
            }
            changed = true;
          }
        }
      }

      if (changed) {
        refreshAchievementsProgress();
        uiDirty = true;
      }
      return changed;
    }
  }

  if (!furnaceOpen && hitTestCraftResult(pos.x, pos.y)) {
    CraftMatch match{};
    if (!findCraftMatch(craftingSlots, activeCraftGridSize(), match)) {
      return false;
    }
    if (cursorStack.count > 0 && cursorStack.type != match.output) {
      return false;
    }
    int cursorSpace = cursorStack.count > 0
      ? static_cast<int>(kMaxStack - cursorStack.count)
      : static_cast<int>(kMaxStack);
    int maxByCursor = match.outputCount > 0 ? cursorSpace / static_cast<int>(match.outputCount) : 0;
    int craftCount = rightClick ? match.maxCrafts : 1;
    craftCount = std::min(craftCount, match.maxCrafts);
    craftCount = std::min(craftCount, maxByCursor);
    if (craftCount <= 0) {
      return false;
    }

    if (cursorStack.count == 0 || cursorStack.type == kAir) {
      cursorStack.type = match.output;
      cursorStack.count = 0;
    }
    cursorStack.count = static_cast<uint16_t>(
      cursorStack.count + craftCount * static_cast<int>(match.outputCount));

    for (size_t i = 0; i < craftingSlots.size(); ++i) {
      uint8_t useCount = match.useCounts[i];
      if (useCount == 0) {
        continue;
      }
      ItemStack& slot = craftingSlots[i];
      uint16_t consumed = static_cast<uint16_t>(craftCount * static_cast<int>(useCount));
      slot.count = static_cast<uint16_t>(slot.count - consumed);
      if (slot.count == 0) {
        slot.type = kAir;
      }
    }

    craftResultFlashTimer = kCraftResultFlashDuration;
    audio.playCue(AudioSystem::Cue::kCraftComplete, rightClick ? 1.08f : 1.0f);
    refreshAchievementsProgress();
    uiDirty = true;
    return true;
  }

  int index = -1;
  bool onCraft = !furnaceOpen && hitTestCraftInput(pos.x, pos.y, index);
  bool onHotbar = false;
  bool onInventory = false;
  if (!onCraft) {
    onHotbar = hitTestHotbar(pos.x, pos.y, index);
    if (!onHotbar) {
      onInventory = hitTestInventory(pos.x, pos.y, index);
    }
  }

  if (!onCraft && !onHotbar && !onInventory) {
    return false;
  }

  if (onInventory && isCreativeInventoryScreen()) {
    size_t catalogIndex = static_cast<size_t>(index);
    if (catalogIndex >= kCreativeCatalogItems.size()) {
      return false;
    }
    uint8_t catalogType = kCreativeCatalogItems[catalogIndex];
    if (catalogType == kAir) {
      return false;
    }
    cursorStack.type = catalogType;
    cursorStack.count = rightClick ? 1 : static_cast<uint16_t>(isToolItem(catalogType) ? 1 : kMaxStack);
    uiDirty = true;
    return true;
  }

  bool selectionChanged = false;
  if (onHotbar && selectedSlot != index) {
    selectedSlot = index;
    selectionChanged = true;
  }

  ItemStack& slot = onCraft
    ? craftingSlots[static_cast<size_t>(index)]
    : (onHotbar
         ? hotbar[static_cast<size_t>(index)]
         : inventory[static_cast<size_t>(index)]);

  bool changed = interactWithSlot(slot);

  if (onHotbar && (changed || selectionChanged)) {
    refreshSelectedBlock();
    if (selectionChanged) {
      showSelectedItemToast();
    }
  }

  if (changed) {
    refreshAchievementsProgress();
    uiDirty = true;
  }

  return changed || selectionChanged;
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

  auto chunkReadyAt = [&](int worldX, int worldZ) {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    return world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise;
  };

  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z)) {
          return true;
        }
        // Prevent falling through not-yet-generated chunk areas while streaming.
        if (!chunkReadyAt(x, z)) {
          return true;
        }
        uint8_t block = world.getBlock(x, y, z);
        if (solidBlockIntersectsAabb(block, x, y, z, min, max)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::collidesAtWithPlacedBlock(const glm::vec3& pos,
                                    int blockX,
                                    int blockY,
                                    int blockZ,
                                    uint8_t placedType) const {
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

  auto chunkReadyAt = [&](int worldX, int worldZ) {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / static_cast<float>(kChunkSize)));
    return world.getChunkGenerationStatus(cx, cz) >= ChunkGenStatus::kNoise;
  };

  for (int y = minY; y <= maxY; ++y) {
    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        if (!world.inBounds(x, y, z)) {
          return true;
        }
        if (!chunkReadyAt(x, z)) {
          return true;
        }
        bool placedBlock = x == blockX && y == blockY && z == blockZ;
        uint8_t block = placedBlock ? placedType : world.getBlock(x, y, z);
        if (solidBlockIntersectsAabb(block, x, y, z, min, max)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::intersectsWaterAt(const glm::vec3& pos) const {
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
          continue;
        }
        if (isWaterVolumeBlock(world.getBlock(x, y, z))) {
          return true;
        }
      }
    }
  }

  return false;
}

bool App::blockIntersectsPlayer(int x, int y, int z, uint8_t placedType) const {
  const float halfWidth = 0.3f;
  const float height = 1.8f;
  glm::vec3 min = {playerPos.x - halfWidth, playerPos.y, playerPos.z - halfWidth};
  glm::vec3 max = {playerPos.x + halfWidth, playerPos.y + height, playerPos.z + halfWidth};

  return solidBlockIntersectsAabb(placedType, x, y, z, min, max);
}

bool App::canPlaceBlockAt(int x, int y, int z, uint8_t placedType, glm::vec3* outAdjustedPlayerPos) const {
  glm::vec3 adjustedPos = playerPos;
  if (placedType == kAir || isWaterBlock(placedType) || isDecorationBlock(placedType)) {
    if (outAdjustedPlayerPos) {
      *outAdjustedPlayerPos = adjustedPos;
    }
    return true;
  }
  if (!blockIntersectsPlayer(x, y, z, placedType)) {
    if (outAdjustedPlayerPos) {
      *outAdjustedPlayerPos = adjustedPos;
    }
    return true;
  }

  float blockTop = collisionTopForBlock(placedType, y) + 0.001f;
  if (blockTop <= playerPos.y + 1.05f) {
    adjustedPos.y = std::max(playerPos.y, blockTop);
    if (!collidesAtWithPlacedBlock(adjustedPos, x, y, z, placedType)) {
      if (outAdjustedPlayerPos) {
        *outAdjustedPlayerPos = adjustedPos;
      }
      return true;
    }
  }

  return false;
}

glm::vec3 App::cameraFront() const {
  glm::vec3 front;
  front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  front.y = std::sin(glm::radians(pitch));
  front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  return glm::normalize(front);
}

float App::cameraEyeHeight() const {
  return crouching ? 1.52f : 1.8f;
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

    uint8_t block = world.getBlock(voxel.x, voxel.y, voxel.z);
    if (block != kAir && !isWaterBlock(block)) {
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
  if ((screenState == ScreenState::kPlaying ||
       screenState == ScreenState::kPaused ||
       screenState == ScreenState::kLoadingWorld ||
       screenState == ScreenState::kDeath) &&
      !currentWorldPath.empty()) {
    if (inventoryOpen) {
      setInventoryOpen(false);
    }
    saveCurrentPlayerState();
    saveWorldWithWarning(world, currentWorldPath, "app cleanup");
  }
  saveSettingsWithWarning("app cleanup");
  audio.shutdown();
  vk.cleanup();

  if (window) {
    glfwDestroyWindow(window);
    window = nullptr;
  }

  glfwTerminate();
}

void App::windowSizeCallback(GLFWwindow* window, int width, int height) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }

  if (width <= 0 || height <= 0) {
    return;
  }

  app->width = width;
  app->height = height;
  app->uiDirty = true;
}

void App::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }

  if (width == 0 || height == 0) {
    return;
  }

  app->framebufferWidth = width;
  app->framebufferHeight = height;
  app->framebufferResized = true;
  app->uiDirty = true;
}

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)xoffset;

  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app || yoffset == 0.0) {
    return;
  }

  int direction = yoffset > 0.0 ? -1 : 1;
  int stepCount = scrollStepsFromOffset(yoffset);
  int delta = direction * stepCount;

  if (app->achievementTreeOpen && app->screenState == ScreenState::kPlaying) {
    AchievementTreeUiLayout layout = makeAchievementTreeLayout(app->uiLayoutWidth(), app->uiLayoutHeight());
    glm::vec2 scroll =
      clampAchievementTreeScroll(layout, {app->achievementTreeScrollX, app->achievementTreeScrollY});
    bool shiftPressed =
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    float horizontalStep = 88.0f * static_cast<float>(stepCount);
    float verticalStep = 72.0f * static_cast<float>(stepCount);
    if (shiftPressed || layout.contentH > layout.viewportH + 8.0f) {
      scroll.y += verticalStep * static_cast<float>(delta);
    } else {
      scroll.x += horizontalStep * static_cast<float>(delta);
    }
    scroll = clampAchievementTreeScroll(layout, scroll);
    if (std::abs(scroll.x - app->achievementTreeScrollX) > 0.01f ||
        std::abs(scroll.y - app->achievementTreeScrollY) > 0.01f) {
      app->achievementTreeScrollX = scroll.x;
      app->achievementTreeScrollY = scroll.y;
      app->uiDirty = true;
    }
  } else if (app->screenState == ScreenState::kWorldSelect) {
    int rowCount = static_cast<int>(app->worldSelectEntries.size()) + 2;
    if (rowCount <= 0) {
      return;
    }
    int nextSelection = std::clamp(app->worldSelectSelection + delta, 0, rowCount - 1);
    if (nextSelection == app->worldSelectSelection) {
      return;
    }
    app->worldSelectSelection = nextSelection;
    int visibleRows = computeWorldSelectVisibleRows(app->uiLayoutHeight(), rowCount);
    if (rowCount <= visibleRows) {
      app->worldSelectScroll = 0;
    } else {
      int maxScroll = rowCount - visibleRows;
      if (app->worldSelectSelection < app->worldSelectScroll) {
        app->worldSelectScroll = app->worldSelectSelection;
      } else if (app->worldSelectSelection >= app->worldSelectScroll + visibleRows) {
        app->worldSelectScroll = app->worldSelectSelection - visibleRows + 1;
      }
      app->worldSelectScroll = std::clamp(app->worldSelectScroll, 0, maxScroll);
    }
    app->updateWindowTitle();
    app->uiDirty = true;
  } else if (app->screenState == ScreenState::kSettings) {
    int focusArea = std::clamp(app->settingsFocusArea,
                               static_cast<int>(SettingsFocusArea::kCategories),
                               static_cast<int>(SettingsFocusArea::kActions));
    if (focusArea == static_cast<int>(SettingsFocusArea::kCategories)) {
      int nextCategory = std::clamp(app->settingsCategory + delta, 0, kSettingsCategoryCount - 1);
      if (nextCategory == app->settingsCategory) {
        return;
      }
      app->settingsCategory = nextCategory;
      app->settingsOptionSelection = 0;
    } else if (focusArea == static_cast<int>(SettingsFocusArea::kActions)) {
      int nextAction = std::clamp(app->settingsActionSelection + delta, 0, kSettingsActionCount - 1);
      if (nextAction == app->settingsActionSelection) {
        return;
      }
      app->settingsActionSelection = nextAction;
    } else {
      int optionCount = settingsEntryCountForCategory(app->settingsCategory);
      int nextOption = std::clamp(app->settingsOptionSelection + delta, 0, std::max(0, optionCount - 1));
      if (nextOption == app->settingsOptionSelection) {
        return;
      }
      app->settingsOptionSelection = nextOption;
    }
    app->uiDirty = true;
  }
}

void App::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }

  if (app->screenState != ScreenState::kPlaying) {
    app->lastMouseX = static_cast<float>(xpos);
    app->lastMouseY = static_cast<float>(ypos);
    app->firstMouse = true;
    return;
  }

  if (app->inventoryOpen || app->achievementTreeOpen || app->commandInputOpen) {
    glm::vec2 fb = app->cursorToFramebuffer(xpos, ypos);
    app->cursorFbX = fb.x;
    app->cursorFbY = fb.y;
    app->uiDirty = true;
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

  xoffset *= app->appliedSettings.sensitivity;
  yoffset *= app->appliedSettings.sensitivity;

  app->yaw += xoffset;
  app->pitch += yoffset;

  if (app->pitch > 89.0f) {
    app->pitch = 89.0f;
  }
  if (app->pitch < -89.0f) {
    app->pitch = -89.0f;
  }
}

void App::charCallback(GLFWwindow* window, unsigned int codepoint) {
  auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  if (!app) {
    return;
  }
  app->onCharInput(codepoint);
}
