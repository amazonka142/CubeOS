#pragma once

#include <cstdint>
#include <memory>

class AudioSystem {
public:
  enum class Cue : uint8_t {
    kChestOpen = 0,
    kCraftComplete = 1,
    kFurnaceLoop = 2,
    kBlockBreakDirt = 3,
    kBlockPlaceDirt = 4,
    kBlockBreakSand = 5,
    kBlockPlaceSand = 6,
    kBlockBreakWood = 7,
    kBlockPlaceWood = 8,
    kBlockBreakStone = 9,
    kBlockPlaceStone = 10,
    kWaterSwim = 11,
    kCount
  };

  AudioSystem();
  ~AudioSystem();

  AudioSystem(const AudioSystem&) = delete;
  AudioSystem& operator=(const AudioSystem&) = delete;

  bool init();
  void shutdown();

  void setMasterVolume(int percent);
  void playCue(Cue cue, float gain = 1.0f);
  void setLoopCueActive(Cue cue, bool active, float gain = 1.0f);

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
