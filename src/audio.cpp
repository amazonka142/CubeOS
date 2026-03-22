#include "audio.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

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
#else
#include <unistd.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace {

constexpr size_t cueIndex(AudioSystem::Cue cue) {
  return static_cast<size_t>(cue);
}

struct ClipConfig {
  const char* relativePath = "";
  float baseVolume = 1.0f;
  ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
  bool loop = false;
};

constexpr std::array<ClipConfig, cueIndex(AudioSystem::Cue::kCount)> kClipConfigs{{
  {"chest_open.wav", 0.72f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"craft_complete.wav", 0.78f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"furnace_fire.wav", 0.22f, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_STREAM, true},
  {"block_break_dirt.wav", 0.88f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_place_dirt.wav", 0.78f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_break_sand.wav", 0.98f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_place_sand.wav", 0.84f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_break_wood.wav", 0.90f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_place_wood.wav", 0.82f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_break_stone.wav", 0.92f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"block_place_stone.wav", 0.80f, MA_SOUND_FLAG_NO_SPATIALIZATION, false},
  {"water_swim.wav", 0.84f, MA_SOUND_FLAG_NO_SPATIALIZATION, false}
}};

std::string getExecutableDir() {
#ifdef _WIN32
  char buffer[MAX_PATH];
  DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  if (length == 0) {
    return ".";
  }
  std::string path(buffer, length);
  size_t pos = path.find_last_of("\\/");
  return pos == std::string::npos ? std::string(".") : path.substr(0, pos);
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
    }
  }

  char resolved[PATH_MAX];
  if (!path.empty() && realpath(path.c_str(), resolved) != nullptr) {
    path = resolved;
  }

  size_t pos = path.find_last_of('/');
  return pos == std::string::npos ? std::string(".") : path.substr(0, pos);
#else
  char pathbuf[4096];
  ssize_t length = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf) - 1);
  if (length <= 0) {
    return ".";
  }
  pathbuf[length] = '\0';
  std::string path(pathbuf);
  size_t pos = path.find_last_of('/');
  return pos == std::string::npos ? std::string(".") : path.substr(0, pos);
#endif
}

bool fileExists(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec) && !ec;
}

std::string buildAudioPath(const char* relativePath) {
  if (relativePath == nullptr || relativePath[0] == '\0') {
    return {};
  }

  const std::string execDir = getExecutableDir();
  const std::array<std::filesystem::path, 4> candidates{{
    std::filesystem::path(execDir) / "audio" / relativePath,
    std::filesystem::path(execDir) / ".." / "audio" / relativePath,
    std::filesystem::path("assets") / "audio" / relativePath,
    std::filesystem::path("audio") / relativePath
  }};

  for (const std::filesystem::path& candidate : candidates) {
    if (fileExists(candidate)) {
      return candidate.lexically_normal().string();
    }
  }

  return {};
}

float clampGain(float gain) {
  return std::clamp(gain, 0.0f, 2.0f);
}

} // namespace

struct AudioSystem::Impl {
  struct ClipState {
    bool loaded = false;
    bool active = false;
    std::string resolvedPath{};
    ma_sound sound{};
  };

  ma_engine engine{};
  bool engineReady = false;
  float masterVolume = 0.8f;
  std::array<ClipState, cueIndex(AudioSystem::Cue::kCount)> clips{};
};

AudioSystem::AudioSystem()
  : impl(std::make_unique<Impl>()) {}

AudioSystem::~AudioSystem() {
  shutdown();
}

bool AudioSystem::init() {
  if (!impl) {
    return false;
  }
  if (impl->engineReady) {
    return true;
  }

  if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
    return false;
  }

  impl->engineReady = true;
  ma_engine_set_volume(&impl->engine, impl->masterVolume);

  for (size_t i = 0; i < kClipConfigs.size(); ++i) {
    const ClipConfig& config = kClipConfigs[i];
    Impl::ClipState& clip = impl->clips[i];
    clip.resolvedPath = buildAudioPath(config.relativePath);
    if (clip.resolvedPath.empty()) {
      continue;
    }

    if (ma_sound_init_from_file(&impl->engine,
                                clip.resolvedPath.c_str(),
                                config.flags,
                                nullptr,
                                nullptr,
                                &clip.sound) != MA_SUCCESS) {
      clip.resolvedPath.clear();
      continue;
    }

    ma_sound_set_looping(&clip.sound, config.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&clip.sound, config.baseVolume);
    clip.loaded = true;
  }

  return true;
}

void AudioSystem::shutdown() {
  if (!impl) {
    return;
  }

  for (Impl::ClipState& clip : impl->clips) {
    if (!clip.loaded) {
      clip.active = false;
      clip.resolvedPath.clear();
      continue;
    }
    ma_sound_uninit(&clip.sound);
    clip.loaded = false;
    clip.active = false;
    clip.resolvedPath.clear();
  }

  if (impl->engineReady) {
    ma_engine_uninit(&impl->engine);
    impl->engineReady = false;
  }
}

void AudioSystem::setMasterVolume(int percent) {
  if (!impl) {
    return;
  }

  impl->masterVolume = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);
  if (impl->engineReady) {
    ma_engine_set_volume(&impl->engine, impl->masterVolume);
  }
}

void AudioSystem::playCue(Cue cue, float gain) {
  if (!impl || !impl->engineReady) {
    return;
  }

  const size_t index = cueIndex(cue);
  if (index >= kClipConfigs.size()) {
    return;
  }

  const ClipConfig& config = kClipConfigs[index];
  Impl::ClipState& clip = impl->clips[index];
  if (config.loop || !clip.loaded) {
    return;
  }

  ma_sound_stop(&clip.sound);
  ma_sound_seek_to_pcm_frame(&clip.sound, 0);
  ma_sound_set_volume(&clip.sound, config.baseVolume * clampGain(gain));
  ma_sound_start(&clip.sound);
}

void AudioSystem::setLoopCueActive(Cue cue, bool active, float gain) {
  if (!impl || !impl->engineReady) {
    return;
  }

  const size_t index = cueIndex(cue);
  if (index >= kClipConfigs.size()) {
    return;
  }

  const ClipConfig& config = kClipConfigs[index];
  Impl::ClipState& clip = impl->clips[index];
  if (!config.loop || !clip.loaded) {
    return;
  }

  ma_sound_set_volume(&clip.sound, config.baseVolume * clampGain(gain));

  if (active) {
    if (!clip.active || !ma_sound_is_playing(&clip.sound)) {
      ma_sound_seek_to_pcm_frame(&clip.sound, 0);
      ma_sound_start(&clip.sound);
    }
    clip.active = true;
    return;
  }

  if (clip.active || ma_sound_is_playing(&clip.sound)) {
    ma_sound_stop(&clip.sound);
    ma_sound_seek_to_pcm_frame(&clip.sound, 0);
  }
  clip.active = false;
}
