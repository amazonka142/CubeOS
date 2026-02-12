#include "app.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <stdexcept>

namespace {

constexpr float kSlotSize = 40.0f;
constexpr float kSlotPadding = 6.0f;
constexpr float kSlotBorder = 3.0f;
constexpr float kMarginBottom = 20.0f;
constexpr float kIconPadding = 6.0f;
constexpr float kPanelPadding = 12.0f;
constexpr int kInventoryCols = 9;
constexpr int kInventoryRows = 3;
constexpr uint16_t kMaxStack = 64;
constexpr int kDigitWidth = 3;
constexpr int kDigitHeight = 5;
constexpr float kBreakDuration = 0.6f;
constexpr float kBreakMaxDistance = 6.0f;
constexpr float kMenuButtonWidth = 320.0f;
constexpr float kMenuButtonHeight = 44.0f;
constexpr float kMenuButtonGap = 14.0f;
constexpr char kSettingsFilePath[] = "settings.cfg";

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

int tileForBlock(uint8_t type) {
  if (isWaterBlock(type)) {
    return kTileWater;
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

  switch (type) {
    case kAir:
      return russian ? "ПУСТЫЕ РУКИ" : "EMPTY HAND";
    case kGrass:
      return russian ? "ТРАВА" : "GRASS BLOCK";
    case kDirt:
      return russian ? "ЗЕМЛЯ" : "DIRT";
    case kStone:
      return russian ? "КАМЕНЬ" : "STONE";
    case kSand:
      return russian ? "ПЕСОК" : "SAND";
    case kGravel:
      return russian ? "ГРАВИЙ" : "GRAVEL";
    case kWood:
      return russian ? "ДЕРЕВО" : "WOOD";
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
    case kGoldOre:
      return russian ? "ЗОЛОТАЯ РУДА" : "GOLD ORE";
    default:
      return russian ? "БЛОК" : "BLOCK";
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
  std::filesystem::path savesDir("saves");
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
  std::filesystem::path savesDir("saves");
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

} // namespace

void App::setScreenState(ScreenState state) {
  if (screenState == state) {
    return;
  }

  // Persist current world whenever leaving gameplay.
  bool leavingGameplay =
    (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused) &&
    (state != ScreenState::kPlaying && state != ScreenState::kPaused);
  bool pauseToInGameSettings =
    screenState == ScreenState::kPaused &&
    state == ScreenState::kSettings &&
    settingsReturnState == ScreenState::kPaused;
  if (leavingGameplay &&
      !pauseToInGameSettings &&
      !currentWorldPath.empty()) {
    saveCurrentPlayerState();
    world.save(currentWorldPath);
  }

  if (state != ScreenState::kPlaying && state != ScreenState::kPaused && inventoryOpen) {
    setInventoryOpen(false);
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
  if (window) {
    int cursorMode = (screenState == ScreenState::kPlaying && !inventoryOpen)
      ? GLFW_CURSOR_DISABLED
      : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(window, GLFW_CURSOR, cursorMode);
  }
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

  std::string title = "CubeOS v0.2.0";
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
        ? " | Настройки | Вверх/Вниз выбор | Влево/Вправо изменить | Enter действие"
        : " | Settings | Up/Down Select | Left/Right Adjust | Enter Action";
      break;
    case ScreenState::kPaused:
      title += " | " + loc("Paused: ", "Пауза: ") + mark(pauseMenuSelection, 0, loc("Continue", "Продолжить")) + "  "
            + mark(pauseMenuSelection, 1, loc("Settings", "Настройки")) + "  "
            + mark(pauseMenuSelection, 2, loc("Main Menu", "Меню"));
      break;
    case ScreenState::kLoadingWorld:
      title += " | " + loc("Loading World", "Загрузка мира");
      break;
    case ScreenState::kCreateWorld: {
      std::string presetName = pendingWorldSettings.preset == WorldPreset::kClassicFlat
        ? loc("Classic Flat", "Классический плоский")
        : loc("Minecraft-style", "Minecraft-стиль");
      std::string invMode = pendingWorldSettings.startInventoryMode == 0
        ? loc("Empty", "Пусто")
        : loc("Creative test", "Тест креатива");
      std::string seedText = pendingSeedText.empty() ? loc("random", "случайно") : pendingSeedText;
      std::string nameText = pendingWorldName.empty() ? loc("World", "Мир") : pendingWorldName;
      title += " | " + loc("Create World", "Создание мира") +
               " | " + loc("Name", "Имя") + ": " + nameText +
               " | " + loc("Seed", "Сид") + ": " + seedText +
               " | " + loc("Preset", "Пресет") + ": " + presetName +
               " | " + loc("Cave", "Пещеры") + ": " + std::to_string(pendingWorldSettings.caveDensity).substr(0, 4) +
               " | " + loc("Ravine", "Овраги") + ": " + std::to_string(pendingWorldSettings.ravineFrequency).substr(0, 4) +
               " | " + loc("Start", "Старт") + ": " + invMode;
      break;
    }
    case ScreenState::kPlaying:
      title += " | " + loc("In Game", "В игре");
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

void App::saveCurrentPlayerState() const {
  if (currentWorldPath.empty()) {
    return;
  }
  std::string statePath = playerStatePathForWorld(currentWorldPath);
  if (statePath.empty()) {
    return;
  }

  std::ofstream out(statePath, std::ios::trunc);
  if (!out.is_open()) {
    return;
  }
  out << playerPos.x << " "
      << playerPos.y << " "
      << playerPos.z << " "
      << yaw << " "
      << pitch << "\n";
}

bool App::loadPlayerStateForWorld(const std::string& worldPath,
                                  glm::vec3& outPos,
                                  float& outYaw,
                                  float& outPitch) const {
  std::string statePath = playerStatePathForWorld(worldPath);
  if (statePath.empty()) {
    return false;
  }

  std::ifstream in(statePath);
  if (!in.is_open()) {
    return false;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float loadedYaw = -90.0f;
  float loadedPitch = 0.0f;
  if (!(in >> x >> y >> z >> loadedYaw >> loadedPitch)) {
    return false;
  }

  outPos = glm::vec3(x, y, z);
  outYaw = loadedYaw;
  outPitch = loadedPitch;
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
  hasPendingPlayerResume = loadPlayerStateForWorld(currentWorldPath,
                                                   pendingResumePlayerPos,
                                                   pendingResumeYaw,
                                                   pendingResumePitch);

  for (ItemStack& slot : hotbar) {
    slot.type = kAir;
    slot.count = 0;
  }
  for (ItemStack& slot : inventory) {
    slot.type = kAir;
    slot.count = 0;
  }
  cursorStack.type = kAir;
  cursorStack.count = 0;
  selectedSlot = 0;

  if (pendingWorldSettings.startInventoryMode != 0) {
    std::array<uint8_t, 9> creativeBlocks = {
      kGrass, kDirt, kStone, kSand, kWater, kWood, kLeaves, kCoalOre, kGoldOre
    };
    for (size_t i = 0; i < hotbar.size() && i < creativeBlocks.size(); ++i) {
      hotbar[i].type = creativeBlocks[i];
      hotbar[i].count = 64;
    }
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
  pendingSeedText.clear();
  hasPendingPlayerResume = false;
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
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    seedValue = static_cast<int>(static_cast<uint64_t>(now) & 0x7fffffffu);
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
  cursorStack.type = kAir;
  cursorStack.count = 0;
  selectedSlot = 0;

  if (settings.startInventoryMode != 0) {
    std::array<uint8_t, 9> creativeBlocks = {
      kGrass, kDirt, kStone, kSand, kWater, kWood, kLeaves, kCoalOre, kGoldOre
    };
    for (size_t i = 0; i < hotbar.size() && i < creativeBlocks.size(); ++i) {
      hotbar[i].type = creativeBlocks[i];
      hotbar[i].count = 64;
    }
  }

  renderLoadingFrame(0.25f, ru ? "Подготовка спавна" : "Preparing spawn");
  setupGameplaySession();
}

void App::setupGameplaySession() {
  if (screenState != ScreenState::kLoadingWorld) {
    setScreenState(ScreenState::kLoadingWorld);
  }

  playerPos = glm::vec3(8.0f, static_cast<float>(world.height() - 6), 8.0f);
  playerVel = glm::vec3(0.0f);
  onGround = false;
  inventoryOpen = false;
  breakingActive = false;
  breakingProgress = 0.0f;
  breakingStage = 0;
  world.clearBreakOverlay();

  int initialCx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int initialCz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  int spawnChunkRadius = std::max(8, activeChunkViewRadius + 1);
  int preloadRadius = std::max(4, std::min(activeChunkViewRadius, 6));
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
           !isUnderwaterPlantBlock(block) &&
           block != kLeaves &&
           block != kGravel;
  };

  auto isPreferredSpawnGround = [](uint8_t block) {
    return block == kGrass || block == kDirt || block == kSand;
  };

  auto isSkyPassable = [](uint8_t block) {
    return block == kAir || block == kLeaves || isUnderwaterPlantBlock(block);
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

    auto probeSurface = [&](int x, int z, int& outY, uint8_t& outGround) -> bool {
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

        outY = y;
        outGround = ground;
        return true;
      }
      return false;
    };

    auto probeWaterSurface = [&](int x, int z, int& outWaterY) -> bool {
      for (int y = world.height() - 3; y >= 1; --y) {
        uint8_t waterBlock = world.getBlock(x, y, z);
        uint8_t above = world.getBlock(x, y + 1, z);
        if (!isWaterBlock(waterBlock) || above != kAir) {
          continue;
        }
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

    glm::vec3 bestPos{};
    auto pickBest = [&](bool highBandOnly, bool preferredOnly, int maxRoughness) -> bool {
      int bestScore = std::numeric_limits<int>::min();
      bool found = false;
      int highBandFloor = maxSurfaceY - 10;

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

        int score = c.y * 140 - c.radius * 6 - surfaceRoughness * 12;
        if (isPreferredSpawnGround(c.ground)) {
          score += 260;
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
            if (below == kAir || isWaterBlock(below) || isUnderwaterPlantBlock(below) || below == kLeaves) {
              world.setBlock(x, platformTopY - 1, z, kStone);
            }

            uint8_t floorBlock = world.getBlock(x, platformTopY, z);
            if (floorBlock == kAir || isWaterBlock(floorBlock) || isUnderwaterPlantBlock(floorBlock) || floorBlock == kLeaves) {
              world.setBlock(x, platformTopY, z, kDirt);
            }

            for (int y = platformTopY + 1; y <= platformTopY + 3; ++y) {
              uint8_t headBlock = world.getBlock(x, y, z);
              if (isWaterBlock(headBlock) || isUnderwaterPlantBlock(headBlock) || headBlock == kLeaves) {
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
        if (block == kAir || isWaterBlock(block) || isUnderwaterPlantBlock(block) || block == kLeaves) {
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

    if (!pickBest(true, true, 8) &&
        !pickBest(true, false, 10) &&
        !pickBest(false, true, 14) &&
        !pickBest(false, false, 20)) {
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
  if (hasPendingPlayerResume) {
    glm::vec3 resumePos = pendingResumePlayerPos;
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
        if (block == kAir || block == kLeaves || isUnderwaterPlantBlock(block)) {
          continue;
        }
        return true;
      }
      return false;
    };

    bool resumeInBounds = world.inBounds(rx, ry, rz) &&
                          world.inBounds(rx, ry + 1, rz);
    bool resumeSafe = hasNearbySupport(rx, ry, rz);
    if (resumeInBounds && !collidesAt(resumePos) && resumeSafe) {
      playerPos = resumePos;
      yaw = pendingResumeYaw;
      pitch = std::clamp(pendingResumePitch, -89.0f, 89.0f);
    }
    hasPendingPlayerResume = false;
  }
  while (collidesAt(playerPos) && playerPos.y < static_cast<float>(world.height() - 3)) {
    playerPos.y += 0.35f;
  }
  playerVel = glm::vec3(0.0f);
  onGround = false;

  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  if (cx != initialCx || cz != initialCz) {
    world.updateActiveChunks(cx, cz, activeChunkViewRadius);
  }
  currentChunkX = cx;
  currentChunkZ = cz;
  chunkCenterValid = true;

  const bool ruUi = appliedSettings.language == 1;
  renderLoadingFrame(0.92f, ruUi ? "Сборка рендера чанков" : "Building chunk render data");
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

  renderLoadingFrame(1.0f, ruUi ? "Готово" : "Done");
  setScreenState(ScreenState::kPlaying);

  world.save(currentWorldPath);
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
    pendingWorldSettings.preset =
      (pendingWorldSettings.preset == WorldPreset::kMinecraftStyle)
      ? WorldPreset::kClassicFlat
      : WorldPreset::kMinecraftStyle;
    updateWindowTitle();
    uiDirty = true;
  };

  auto adjustCreateSetting = [&](bool increase) {
    constexpr float kStep = 0.25f;
    bool changed = false;
    switch (createWorldSelection) {
      case 2:
        togglePreset();
        return;
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
      std::abs(pendingSettings.sensitivity - appliedSettings.sensitivity) > 0.0001f ||
      pendingSettings.audioVolume != appliedSettings.audioVolume ||
      pendingSettings.language != appliedSettings.language;
  };

  auto adjustSettingsValue = [&](bool increase) {
    switch (settingsSelection) {
      case 0: {
        int delta = increase ? 1 : -1;
        pendingSettings.graphicsQuality = std::clamp(pendingSettings.graphicsQuality + delta, 0, 2);
        break;
      }
      case 1: {
        float delta = increase ? 0.01f : -0.01f;
        pendingSettings.sensitivity = std::clamp(pendingSettings.sensitivity + delta, 0.03f, 0.40f);
        break;
      }
      case 2: {
        int delta = increase ? 5 : -5;
        pendingSettings.audioVolume = std::clamp(pendingSettings.audioVolume + delta, 0, 100);
        break;
      }
      case 3: {
        pendingSettings.language = pendingSettings.language == 0 ? 1 : 0;
        break;
      }
      default:
        return;
    }

    updateSettingsDirtyFlag();
    uiDirty = true;
  };

  if (screenState == ScreenState::kMainMenu) {
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
        settingsSelection = 0;
        settingsReturnState = ScreenState::kMainMenu;
        pendingSettings = appliedSettings;
        settingsDirty = false;
        setScreenState(ScreenState::kSettings);
      } else {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
    }
    if (escPressed && !menuEscDown) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  } else if (screenState == ScreenState::kPaused) {
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
        settingsSelection = 0;
        settingsReturnState = ScreenState::kPaused;
        pendingSettings = appliedSettings;
        settingsDirty = false;
        setScreenState(ScreenState::kSettings);
      } else {
        setScreenState(ScreenState::kMainMenu);
      }
    }
    if (escPressed && !menuEscDown) {
      setScreenState(ScreenState::kPlaying);
    }
  } else if (screenState == ScreenState::kWorldSelect) {
    int rowCount = static_cast<int>(worldSelectEntries.size()) + 2;
    constexpr int kVisibleRows = 8;

    auto syncWorldSelectScroll = [&]() {
      if (rowCount <= kVisibleRows) {
        worldSelectScroll = 0;
        return;
      }
      int maxScroll = rowCount - kVisibleRows;
      if (worldSelectSelection < worldSelectScroll) {
        worldSelectScroll = worldSelectSelection;
      } else if (worldSelectSelection >= worldSelectScroll + kVisibleRows) {
        worldSelectScroll = worldSelectSelection - kVisibleRows + 1;
      }
      worldSelectScroll = std::clamp(worldSelectScroll, 0, maxScroll);
    };

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
      float panelWidth = std::min(static_cast<float>(width) * 0.72f, 640.0f);
      float panelHeight = std::min(static_cast<float>(height) * 0.78f, 540.0f);
      float panelX = (static_cast<float>(width) - panelWidth) * 0.5f;
      float panelY = (static_cast<float>(height) - panelHeight) * 0.5f;
      panelY += (1.0f - intro) * 28.0f;
      panelY += std::sin(t * 1.4f) * 3.0f;

      int startRow = 0;
      if (rowCount > kVisibleRows) {
        startRow = std::clamp(worldSelectScroll, 0, rowCount - kVisibleRows);
      }
      int endRow = std::min(rowCount, startRow + kVisibleRows);
      int visibleRows = endRow - startRow;

      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 40.0f;
      float rowGap = 10.0f;
      float rowsH = static_cast<float>(visibleRows) * rowH +
                    static_cast<float>(std::max(0, visibleRows - 1)) * rowGap;
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
      int hoveredRow = worldSelectRowAtMouse();
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
    constexpr int kSettingsFieldCount = 7;
    if (upPressed && !menuUpDown) {
      settingsSelection = std::max(0, settingsSelection - 1);
      uiDirty = true;
    }
    if (downPressed && !menuDownDown) {
      settingsSelection = std::min(kSettingsFieldCount - 1, settingsSelection + 1);
      uiDirty = true;
    }
    if ((leftPressed && !menuLeftDown) || (rightPressed && !menuRightDown)) {
      adjustSettingsValue(rightPressed && !menuRightDown);
    }
    if (enterPressed && !menuEnterDown) {
      if (settingsSelection <= 3) {
        adjustSettingsValue(true);
      } else if (settingsSelection == 4) {
        bool inGameContext =
          settingsReturnState == ScreenState::kPlaying ||
          settingsReturnState == ScreenState::kPaused;
        applySettings(inGameContext);
        saveSettings();
      } else if (settingsSelection == 5) {
        pendingSettings = UserSettings{};
        updateSettingsDirtyFlag();
        uiDirty = true;
      } else if (settingsSelection == 6) {
        pendingSettings = appliedSettings;
        settingsDirty = false;
        setScreenState(settingsReturnState);
      }
    }
    if (escPressed && !menuEscDown) {
      pendingSettings = appliedSettings;
      settingsDirty = false;
      setScreenState(settingsReturnState);
    }
  } else if (screenState == ScreenState::kCreateWorld) {
    constexpr int kCreateFieldCount = 8;

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
      if (createWorldSelection >= 2 && createWorldSelection <= 5) {
        adjustCreateSetting(true);
      } else if (createWorldSelection == 6) {
        createWorldFromMenu();
      } else if (createWorldSelection == 7) {
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
  std::ifstream in(kSettingsFilePath);
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
      } else if (key == "language") {
        std::string lowered = value;
        for (char& c : lowered) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        appliedSettings.language = (lowered == "ru" || lowered == "russian" || lowered == "1") ? 1 : 0;
      }
    }
  }

  pendingSettings = appliedSettings;
  applySettings(false);
  settingsDirty = false;
}

bool App::saveSettings() const {
  std::ofstream out(kSettingsFilePath, std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }

  out << "graphics_quality=" << appliedSettings.graphicsQuality << "\n";
  out << "sensitivity=" << appliedSettings.sensitivity << "\n";
  out << "audio_volume=" << appliedSettings.audioVolume << "\n";
  out << "language=" << (appliedSettings.language == 1 ? "ru" : "en") << "\n";
  return true;
}

void App::applySettings(bool refreshWorldStreaming) {
  appliedSettings.graphicsQuality = std::clamp(pendingSettings.graphicsQuality, 0, 2);
  appliedSettings.sensitivity = std::clamp(pendingSettings.sensitivity, 0.03f, 0.40f);
  appliedSettings.audioVolume = std::clamp(pendingSettings.audioVolume, 0, 100);
  appliedSettings.language = std::clamp(pendingSettings.language, 0, 1);
  pendingSettings = appliedSettings;
  settingsDirty = false;

  static constexpr int kQualityToRadius[3] = {6, 8, 10};
  activeChunkViewRadius = kQualityToRadius[appliedSettings.graphicsQuality];

  if (refreshWorldStreaming && screenState == ScreenState::kPlaying) {
    int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
    int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
    world.updateActiveChunks(cx, cz, activeChunkViewRadius);
    currentChunkX = cx;
    currentChunkZ = cz;
    chunkCenterValid = true;
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

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, App::framebufferResizeCallback);
  glfwSetCursorPosCallback(window, App::mouseCallback);
  glfwSetCharCallback(window, App::charCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  loadSettings();
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

  world.setGenerationSettings(startupSettings);
  world.setSeed(1337);
  world.generate();
  world.updateActiveChunks(0, 0, 3);
  world.waitForChunkRegion(0, 0, 3, 2500);

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
}

void App::mainLoop() {
  constexpr double kWorldChunkUploadMinIntervalSec = 1.0 / 30.0; // Section uploads are cheap enough to stream at ~30 Hz.
  constexpr int kMeshRebuildTaskBudgetPerTick = 10;
  constexpr int kMeshUploadBudgetPerTick = 16;
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;

    glfwPollEvents();
    processInput(deltaTime);

    if (selectedItemToastTimer > 0.0f) {
      selectedItemToastTimer = std::max(0.0f, selectedItemToastTimer - deltaTime);
      uiDirty = true;
    }

    if (screenState != ScreenState::kPlaying) {
      if (menuIntro < 1.0f) {
        menuIntro = std::min(1.0f, menuIntro + deltaTime * 3.2f);
      }
      // Keep menu animated (focus pulse + subtle panel motion).
      uiDirty = true;
    }
    if (screenState == ScreenState::kPlaying && !inventoryOpen) {
      updatePlayer(deltaTime);
    }

    if (screenState == ScreenState::kPlaying) {
      updateStreaming();
      waterSimBoostTimer = std::max(0.0f, waterSimBoostTimer - deltaTime);
      waterSimAccumulator += deltaTime;
      if (waterSimAccumulator > 0.6f) {
        waterSimAccumulator = 0.6f;
      }
      while (waterSimAccumulator >= 0.24f) {
        int px = static_cast<int>(std::floor(playerPos.x));
        int pz = static_cast<int>(std::floor(playerPos.z));
        bool nearWater = intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.65f, 0.0f));
        bool highActivity = waterSimBoostTimer > 0.0f;

        // Keep background water work very small to avoid frame drops in ocean areas.
        if (highActivity || nearWater) {
          int simRadius = highActivity ? 20 : 12;
          int waterUpdates = highActivity ? 52 : 8;
          int fallingUpdates = highActivity ? 44 : 10;
          world.simulateWater(px, pz, simRadius, waterUpdates);
          world.simulateFallingBlocks(px, pz, simRadius, fallingUpdates);
        }

        waterSimAccumulator -= 0.24f;
      }
    } else {
      waterSimBoostTimer = 0.0f;
      waterSimAccumulator = 0.0f;
    }

    std::vector<ChunkMeshUpload> chunkUpdates;
    std::vector<uint64_t> removedChunkKeys;
    bool hasChunkUpdateBatch = world.consumeChunkMeshUpdates(
      chunkUpdates,
      removedChunkKeys,
      kMeshRebuildTaskBudgetPerTick,
      kMeshUploadBudgetPerTick);
    if (hasChunkUpdateBatch) {
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

    if (uiDirty) {
      rebuildUiMesh();
      composeMeshData();
      vk.updateMesh(meshVertices, meshIndices, skyIndexCount, worldIndexCount, uiIndexCount);
      uiDirty = false;
    }

    glm::vec3 eye;
    glm::vec3 front;
    bool cameraInWater = false;
    if (screenState == ScreenState::kPlaying || screenState == ScreenState::kPaused) {
      eye = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
      front = cameraFront();
      cameraInWater = isWaterBlock(world.getBlock(static_cast<int>(std::floor(eye.x)),
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

    vk.setCameraWorldState(eye, cameraInWater);
    vk.setCameraMatrices(view, proj);
    vk.drawFrame();
  }

  vk.waitIdle();
}

void App::processInput(float deltaTime) {
  if (screenState != ScreenState::kPlaying) {
    processMenuInput(deltaTime);
    escDown = false;
    tabDown = false;
    mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouseRightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    return;
  }

  bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (escPressed && !escDown) {
    if (inventoryOpen) {
      setInventoryOpen(false);
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
    setInventoryOpen(!inventoryOpen);
    tabDown = true;
  } else if (!tabPressed) {
    tabDown = false;
  }

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

  bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  if (inventoryOpen) {
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

  bool inWater = intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f));
  const float moveSpeed = inWater ? 3.2f : 6.0f;
  playerVel.x = wishDir.x * moveSpeed;
  playerVel.z = wishDir.z * moveSpeed;

  if (inWater) {
    float verticalIntent = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      verticalIntent += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
      verticalIntent -= 1.0f;
    }

    if (std::abs(verticalIntent) > 0.001f) {
      playerVel.y = verticalIntent * 4.4f;
    } else if (playerVel.y > -0.9f) {
      playerVel.y = -0.9f;
    }
    onGround = false;
  } else if (onGround && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    playerVel.y = 6.5f;
    onGround = false;
  }

  if (leftPressed) {
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    glm::vec3 breakDir = cameraFront();
    RaycastHit hit = raycast(origin, breakDir, kBreakMaxDistance);
    if (hit.hit) {
      float breakDuration = inWater ? (kBreakDuration * 2.0f) : kBreakDuration;
      glm::ivec3 targetBlock = hit.block;
      uint8_t hitType = world.getBlock(targetBlock.x, targetBlock.y, targetBlock.z);

      auto breakTarget = [&](const glm::ivec3& blockPos) {
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
          uint8_t removed = world.getBlock(blockPos.x, blockPos.y, blockPos.z);
          if (removed != kAir && !isWaterBlock(removed)) {
            world.setBlock(blockPos.x, blockPos.y, blockPos.z, kAir);
            waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);
            addToInventory(removed, 1);
            refreshSelectedBlock();
            uiDirty = true;
          }
          breakingActive = false;
          breakingProgress = 0.0f;
          breakingStage = 0;
          world.clearBreakOverlay();
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
          if (candidate == kAir || isWaterBlock(candidate) || isUnderwaterPlantBlock(candidate)) {
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
            if (belowType == kAir || isWaterBlock(belowType) || isUnderwaterPlantBlock(belowType)) {
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
          breakTarget(targetBlock);
        }
      } else {
        breakTarget(targetBlock);
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
    glm::vec3 origin = playerPos + glm::vec3(0.0f, 1.8f, 0.0f);
    glm::vec3 placeDir = cameraFront();
    RaycastHit hit = raycast(origin, placeDir, kBreakMaxDistance);
    if (hit.hit) {
      glm::ivec3 target = hit.block + hit.normal;
      uint8_t hitType = world.getBlock(hit.block.x, hit.block.y, hit.block.z);

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
          if (candidate == kAir || isWaterBlock(candidate) || isUnderwaterPlantBlock(candidate)) {
            lastReplaceable = voxel;
            continue;
          }

          uint8_t replaceType = world.getBlock(lastReplaceable.x, lastReplaceable.y, lastReplaceable.z);
          if (replaceType == kAir || isWaterBlock(replaceType) || isUnderwaterPlantBlock(replaceType)) {
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
                               isUnderwaterPlantBlock(targetType);
      if (world.inBounds(target.x, target.y, target.z) &&
          targetReplaceable &&
          !blockIntersectsPlayer(target.x, target.y, target.z)) {
        ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
        if (stack.count > 0 && stack.type != kAir) {
          world.setBlock(target.x, target.y, target.z, stack.type);
          waterSimBoostTimer = std::max(waterSimBoostTimer, 2.0f);
          stack.count -= 1;
          if (stack.count == 0) {
            stack.type = kAir;
          }
          refreshSelectedBlock();
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
  bool inWater = intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f));
  if (inWater) {
    const float waterGravity = -5.0f;
    playerVel.y += waterGravity * deltaTime;
    playerVel.y = std::clamp(playerVel.y, -2.7f, 4.0f);
  } else {
    const float gravity = -18.0f;
    playerVel.y += gravity * deltaTime;
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
  if (intersectsWaterAt(playerPos + glm::vec3(0.0f, 0.2f, 0.0f))) {
    onGround = false;
  }
}

void App::updateStreaming() {
  int cx = static_cast<int>(std::floor(playerPos.x / static_cast<float>(kChunkSize)));
  int cz = static_cast<int>(std::floor(playerPos.z / static_cast<float>(kChunkSize)));
  if (!chunkCenterValid || cx != currentChunkX || cz != currentChunkZ) {
    currentChunkX = cx;
    currentChunkZ = cz;
    chunkCenterValid = true;
    world.updateActiveChunks(cx, cz, activeChunkViewRadius);
  }
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

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
  const float startY = static_cast<float>(height) - kMarginBottom - kSlotSize;

  auto toNdc = [&](float px, float py) -> glm::vec2 {
    float x = (px / static_cast<float>(width)) * 2.0f - 1.0f;
    // Vulkan NDC maps +Y toward the bottom with a positive viewport height.
    float y = (py / static_cast<float>(height)) * 2.0f - 1.0f;
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

  addSkyQuad(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

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

  const int backgroundTile = tileForBlock(kStone);

  auto measureTextWidth = [&](const std::string& text, float pixel) -> float {
    float widthPx = 0.0f;
    size_t index = 0;
    while (index < text.size()) {
      uint32_t cp = 0;
      if (!nextUtf8Codepoint(text, index, cp)) {
        break;
      }
      cp = toUpperCodepoint(cp);
      if (cp == static_cast<uint32_t>(' ')) {
        widthPx += pixel * 2.0f;
        continue;
      }
      widthPx += pixel * static_cast<float>(kGlyphWidth + 1);
    }
    if (widthPx > 0.0f) {
      widthPx -= pixel;
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

    if (centered) {
      x -= measureTextWidth(text, pixel) * 0.5f;
    }

    size_t index = 0;
    while (index < text.size()) {
      uint32_t cp = 0;
      if (!nextUtf8Codepoint(text, index, cp)) {
        break;
      }
      cp = toUpperCodepoint(cp);
      if (cp == static_cast<uint32_t>(' ')) {
        x += pixel * 2.0f;
        continue;
      }

      uint8_t fallback[kGlyphHeight] = {0, 0, 0, 0, 0};
      const uint8_t* glyph = glyphForCodepoint(cp);
      if (!glyph) {
        switch (cp) {
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

      x += pixel * static_cast<float>(kGlyphWidth + 1);
    }
  };

  if (screenState != ScreenState::kPlaying) {
    const bool ruUi = appliedSettings.language == 1;
    float t = static_cast<float>(glfwGetTime());
    float intro = menuIntro * menuIntro * (3.0f - 2.0f * menuIntro);
    float pulse = 0.5f + 0.5f * std::sin(t * 6.5f);
    float panelWidth = std::min(static_cast<float>(width) * 0.72f, 640.0f);
    float panelHeight = std::min(static_cast<float>(height) * 0.78f, 540.0f);
    float panelX = (static_cast<float>(width) - panelWidth) * 0.5f;
    float panelY = (static_cast<float>(height) - panelHeight) * 0.5f;
    panelY += (1.0f - intro) * 28.0f;
    panelY += std::sin(t * 1.4f) * 3.0f;

    addQuad(panelX, panelY, panelWidth, panelHeight, glm::vec3(0.10f, 0.10f, 0.13f), backgroundTile);
    addQuad(panelX + 8.0f,
            panelY + 8.0f,
            panelWidth - 16.0f,
            40.0f,
            glm::vec3(0.16f, 0.19f, 0.24f),
            backgroundTile);
    drawText("CUBEOS V0 2 0 BETA", panelX + panelWidth * 0.5f, panelY + 18.0f, 3.0f,
             glm::vec3(0.91f, 0.94f, 0.98f), true);

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
      constexpr int kVisibleRows = 8;
      int startRow = 0;
      if (totalRows > kVisibleRows) {
        startRow = std::clamp(worldSelectScroll, 0, totalRows - kVisibleRows);
      }
      int endRow = std::min(totalRows, startRow + kVisibleRows);
      int visibleRows = endRow - startRow;

      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 40.0f;
      float rowGap = 10.0f;
      float rowsH = static_cast<float>(visibleRows) * rowH +
                    static_cast<float>(std::max(0, visibleRows - 1)) * rowGap;
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

      if (totalRows > kVisibleRows) {
        std::string hint = (ruUi ? "СПИСОК " : "SCROLL ") +
                           std::to_string(startRow + 1) +
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
      drawText(ru ? "НАСТРОЙКИ" : "SETTINGS",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);
      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 40.0f;
      float rowGap = 14.0f;
      float rowY = panelY + 92.0f;
      for (int i = 0; i < 7; ++i) {
        drawMenuRow(i,
                    settingsSelection,
                    rowX,
                    rowY + static_cast<float>(i) * (rowH + rowGap),
                    rowW,
                    rowH,
                    glm::vec3(0.21f, 0.23f, 0.28f));
      }

      std::string graphicsValue = "MEDIUM";
      if (pendingSettings.graphicsQuality == 0) {
        graphicsValue = "LOW";
      } else if (pendingSettings.graphicsQuality == 2) {
        graphicsValue = "HIGH";
      }
      if (ru) {
        if (pendingSettings.graphicsQuality == 0) {
          graphicsValue = "НИЗКО";
        } else if (pendingSettings.graphicsQuality == 2) {
          graphicsValue = "ВЫСОКО";
        } else {
          graphicsValue = "СРЕДНЕ";
        }
      }

      std::ostringstream sens;
      sens.setf(std::ios::fixed);
      sens.precision(2);
      sens << pendingSettings.sensitivity;

      std::string dirtyMark = settingsDirty
        ? (ru ? " НЕ СОХРАНЕНО" : " UNSAVED")
        : (ru ? " СОХРАНЕНО" : " SAVED");
      std::string backText =
        settingsReturnState == ScreenState::kPaused
          ? (ru ? "НАЗАД В ПАУЗУ" : "BACK TO PAUSE")
          : (ru ? "НАЗАД В МЕНЮ" : "BACK TO MENU");
      std::string languageValue = pendingSettings.language == 1
        ? (ru ? "РУССКИЙ" : "RUSSIAN")
        : (ru ? "АНГЛИЙСКИЙ" : "ENGLISH");
      std::string line0 = (ru ? "ГРАФИКА " : "GRAPHICS ") + graphicsValue;
      std::string line1 = (ru ? "ЧУВСТВИТЕЛЬНОСТЬ " : "SENSITIVITY ") + sens.str();
      std::string line2 = (ru ? "ГРОМКОСТЬ " : "AUDIO ") + std::to_string(pendingSettings.audioVolume);
      std::string line3 = (ru ? "ЯЗЫК " : "LANGUAGE ") + languageValue;
      std::string line4 = ru ? "СОХРАНИТЬ И ПРИМЕНИТЬ" : "SAVE AND APPLY";
      std::string line5 = ru ? "СБРОС ПО УМОЛЧАНИЮ" : "RESET DEFAULTS";
      std::string line6 = backText + dirtyMark;

      std::array<std::string, 7> lines = {line0, line1, line2, line3, line4, line5, line6};
      for (int i = 0; i < 7; ++i) {
        drawText(lines[static_cast<size_t>(i)],
                 rowX + 16.0f,
                 rowY + static_cast<float>(i) * (rowH + rowGap) + 12.0f,
                 2.6f,
                 glm::vec3(0.92f, 0.94f, 0.98f),
                 false);
      }
    } else if (screenState == ScreenState::kCreateWorld) {
      drawText(ruUi ? "СОЗДАТЬ МИР" : "CREATE WORLD",
               panelX + panelWidth * 0.5f,
               panelY + 56.0f,
               3.0f,
               glm::vec3(0.90f),
               true);
      constexpr int kRowCount = 8;
      float rowW = panelWidth - 120.0f;
      float rowX = panelX + (panelWidth - rowW) * 0.5f;
      float rowH = 36.0f;
      float rowGap = 12.0f;
      float rowsH = static_cast<float>(kRowCount) * rowH + static_cast<float>(kRowCount - 1) * rowGap;
      float rowY = panelY + 72.0f + std::max(0.0f, (panelHeight - 92.0f - rowsH) * 0.5f);

      for (int i = 0; i < kRowCount; ++i) {
        glm::vec3 rowColor = glm::vec3(0.20f, 0.23f, 0.29f);
        if (i == 6) {
          rowColor = glm::vec3(0.20f, 0.36f, 0.22f);
        } else if (i == 7) {
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
      float optionW = 120.0f;
      float optionH = rowH - 12.0f;
      float optionY = presetY + 6.0f;
      float leftOptionX = rowX + rowW - optionW * 2.0f - 18.0f;
      float rightOptionX = rowX + rowW - optionW - 10.0f;
      bool minecraftPreset = pendingWorldSettings.preset == WorldPreset::kMinecraftStyle;
      addQuad(leftOptionX,
              optionY,
              optionW,
              optionH,
              minecraftPreset ? glm::vec3(0.30f, 0.36f, 0.45f) : glm::vec3(0.20f, 0.23f, 0.28f),
              backgroundTile);
      addQuad(rightOptionX,
              optionY,
              optionW,
              optionH,
              minecraftPreset ? glm::vec3(0.20f, 0.23f, 0.28f) : glm::vec3(0.40f, 0.34f, 0.22f),
              backgroundTile);

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

      float toggleW = 90.0f;
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

      std::string nameText = pendingWorldName.empty() ? (ruUi ? "МИР" : "WORLD") : pendingWorldName;
      std::string seedText = pendingSeedText.empty() ? (ruUi ? "СЛУЧАЙНО" : "RANDOM") : pendingSeedText;
      std::string presetText = pendingWorldSettings.preset == WorldPreset::kClassicFlat
        ? (ruUi ? "КЛАССИЧЕСКИЙ ПЛОСКИЙ" : "CLASSIC FLAT")
        : (ruUi ? "MINECRAFT СТИЛЬ" : "MINECRAFT STYLE");
      std::ostringstream caveText;
      caveText.setf(std::ios::fixed);
      caveText.precision(2);
      caveText << pendingWorldSettings.caveDensity;
      std::ostringstream ravineText;
      ravineText.setf(std::ios::fixed);
      ravineText.precision(2);
      ravineText << pendingWorldSettings.ravineFrequency;
      std::string invText = pendingWorldSettings.startInventoryMode == 0
        ? (ruUi ? "ПУСТО" : "EMPTY")
        : (ruUi ? "ТЕСТ КРЕАТИВА" : "CREATIVE TEST");

      std::array<std::string, 8> labels = {
        (ruUi ? "ИМЯ МИРА " : "WORLD NAME ") + nameText,
        (ruUi ? "СИД " : "SEED ") + seedText,
        (ruUi ? "ПРЕСЕТ " : "PRESET ") + presetText,
        (ruUi ? "ПЛОТНОСТЬ ПЕЩЕР " : "CAVE DENSITY ") + caveText.str(),
        (ruUi ? "ЧАСТОТА ОВРАГОВ " : "RAVINE FREQ ") + ravineText.str(),
        (ruUi ? "СТАРТ ИНВЕНТАРЬ " : "START INVENTORY ") + invText,
        ruUi ? "СОЗДАТЬ МИР" : "CREATE WORLD",
        ruUi ? "ОТМЕНА" : "CANCEL"
      };

      for (int i = 0; i < kRowCount; ++i) {
        float px = (i >= 6) ? 2.9f : 2.45f;
        drawText(labels[static_cast<size_t>(i)],
                 rowX + 14.0f,
                 rowY + static_cast<float>(i) * (rowH + rowGap) + 11.0f,
                 px,
                 glm::vec3(0.94f, 0.95f, 0.98f),
                 false);
      }
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

  auto drawStack = [&](const ItemStack& stack, float x, float y) {
    if (stack.count == 0 || stack.type == kAir) {
      return;
    }
    int tile = tileForBlock(stack.type);
    addQuad(x + kIconPadding,
            y + kIconPadding,
            kSlotSize - kIconPadding * 2.0f,
            kSlotSize - kIconPadding * 2.0f,
            glm::vec3(1.0f),
            tile);
    drawNumber(static_cast<int>(stack.count),
               x + kSlotSize - 4.0f,
               y + kSlotSize - 4.0f,
               3.0f,
               glm::vec3(0.95f, 0.95f, 0.98f),
               backgroundTile);
  };

  if (inventoryOpen) {
    const float gridWidth =
      kSlotSize * static_cast<float>(kInventoryCols) +
      kSlotPadding * static_cast<float>(kInventoryCols - 1);
    const float gridHeight =
      kSlotSize * static_cast<float>(kInventoryRows) +
      kSlotPadding * static_cast<float>(kInventoryRows - 1);

    float gridX = (static_cast<float>(width) - gridWidth) * 0.5f;
    float gridY = (static_cast<float>(height) - gridHeight) * 0.5f - 30.0f;
    gridY = std::clamp(gridY, 20.0f, static_cast<float>(height) - gridHeight - 20.0f);

    addQuad(gridX - kPanelPadding,
            gridY - kPanelPadding,
            gridWidth + kPanelPadding * 2.0f,
            gridHeight + kPanelPadding * 2.0f,
            glm::vec3(0.15f, 0.15f, 0.18f),
            backgroundTile);

    size_t idx = 0;
    for (int row = 0; row < kInventoryRows; ++row) {
      for (int col = 0; col < kInventoryCols; ++col) {
        float x = gridX + static_cast<float>(col) * (kSlotSize + kSlotPadding);
        float y = gridY + static_cast<float>(row) * (kSlotSize + kSlotPadding);

        addQuad(x, y, kSlotSize, kSlotSize, glm::vec3(0.25f, 0.25f, 0.28f), backgroundTile);

        if (idx < inventory.size()) {
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

    drawStack(hotbar[i], x, y);
  }

  if (selectedItemToastTimer > 0.0f && !selectedItemToastText.empty()) {
    float textPixel = 2.4f;
    float textW = measureTextWidth(selectedItemToastText, textPixel);
    float boxW = textW + 24.0f;
    float boxH = textPixel * static_cast<float>(kGlyphHeight) + 14.0f;
    float boxX = static_cast<float>(width) * 0.5f - boxW * 0.5f;
    float boxY = startY - boxH - 14.0f;

    addQuad(boxX, boxY, boxW, boxH, glm::vec3(0.12f, 0.13f, 0.17f), backgroundTile);
    drawText(selectedItemToastText,
             static_cast<float>(width) * 0.5f,
             boxY + 7.0f,
             textPixel,
             glm::vec3(0.92f, 0.94f, 0.98f),
             true);
  }

  if (inventoryOpen && cursorStack.count > 0 && cursorStack.type != kAir) {
    float cx = cursorFbX - kSlotSize * 0.5f;
    float cy = cursorFbY - kSlotSize * 0.5f;
    drawStack(cursorStack, cx, cy);
  }

  const float centerX = static_cast<float>(width) * 0.5f;
  const float centerY = static_cast<float>(height) * 0.5f;
  const float crossArm = 4.0f;
  const float crossThickness = 2.0f;
  const int crossTile = 13;
  addQuad(centerX - crossArm, centerY - crossThickness * 0.5f,
          crossArm * 2.0f, crossThickness, glm::vec3(0.97f), crossTile);
  addQuad(centerX - crossThickness * 0.5f, centerY - crossArm,
          crossThickness, crossArm * 2.0f, glm::vec3(0.97f), crossTile);
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
  if (selectedSlot < 0 || selectedSlot >= static_cast<int>(hotbar.size())) {
    selectedSlot = 0;
  }
  const ItemStack& stack = hotbar[static_cast<size_t>(selectedSlot)];
  if (stack.count == 0 || stack.type == kAir) {
    selectedBlock = kAir;
  } else {
    selectedBlock = stack.type;
  }
}

void App::showSelectedItemToast() {
  selectedItemToastText = displayNameForBlock(selectedBlock, appliedSettings.language == 1);
  selectedItemToastTimer = 2.0f;
  uiDirty = true;
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
  vk.setCameraWorldState(eye, false);
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
    uiDirty = true;
  }

  if (outRemaining) {
    *outRemaining = count;
  }

  return count == 0;
}

void App::setInventoryOpen(bool open) {
  if (inventoryOpen == open) {
    return;
  }

  inventoryOpen = open;
  if (window) {
    glfwSetInputMode(window,
                     GLFW_CURSOR,
                     inventoryOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
  }
  if (inventoryOpen && window) {
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 fb = cursorToFramebuffer(xpos, ypos);
    cursorFbX = fb.x;
    cursorFbY = fb.y;
  }
  if (!inventoryOpen) {
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
  return {static_cast<float>(xpos) * scaleX, static_cast<float>(ypos) * scaleY};
}

bool App::hitTestHotbar(float x, float y, int& outSlot) const {
  if (width <= 0 || height <= 0) {
    return false;
  }

  const float totalWidth =
    kSlotSize * static_cast<float>(hotbar.size()) +
    kSlotPadding * static_cast<float>(hotbar.size() - 1);
  const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
  const float startY = static_cast<float>(height) - kMarginBottom - kSlotSize;

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

  float gridX = (static_cast<float>(width) - gridWidth) * 0.5f;
  float gridY = (static_cast<float>(height) - gridHeight) * 0.5f - 30.0f;
  gridY = std::clamp(gridY, 20.0f, static_cast<float>(height) - gridHeight - 20.0f);

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

bool App::handleInventoryClick(double xpos, double ypos, bool rightClick) {
  glm::vec2 pos = cursorToFramebuffer(xpos, ypos);
  cursorFbX = pos.x;
  cursorFbY = pos.y;
  int index = -1;
  bool onHotbar = hitTestHotbar(pos.x, pos.y, index);
  bool onInventory = false;
  if (!onHotbar) {
    onInventory = hitTestInventory(pos.x, pos.y, index);
  }

  if (!onHotbar && !onInventory) {
    return false;
  }

  bool selectionChanged = false;
  if (onHotbar) {
    if (selectedSlot != index) {
      selectedSlot = index;
      selectionChanged = true;
      refreshSelectedBlock();
      showSelectedItemToast();
    }
  }

  auto clearStack = [](ItemStack& stack) {
    stack.type = kAir;
    stack.count = 0;
  };

  ItemStack& slot = onHotbar
    ? hotbar[static_cast<size_t>(index)]
    : inventory[static_cast<size_t>(index)];

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

  return selectionChanged;
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
        if (block != kAir && !isWaterBlock(block) && !isUnderwaterPlantBlock(block)) {
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
        if (isWaterBlock(world.getBlock(x, y, z))) {
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
  if ((screenState == ScreenState::kPlaying ||
       screenState == ScreenState::kPaused ||
       screenState == ScreenState::kLoadingWorld) &&
      !currentWorldPath.empty()) {
    saveCurrentPlayerState();
    world.save(currentWorldPath);
  }
  saveSettings();
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
  app->uiDirty = true;
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

  if (app->inventoryOpen) {
    glm::vec2 fb = app->cursorToFramebuffer(xpos, ypos);
    app->cursorFbX = fb.x;
    app->cursorFbY = fb.y;
    if (app->cursorStack.count > 0 && app->cursorStack.type != kAir) {
      app->uiDirty = true;
    }
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
