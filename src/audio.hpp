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
    kPlayerHurt = 12,
    kFootstepGrass1 = 13,
    kFootstepGrass2 = 14,
    kFootstepGrass3 = 15,
    kFootstepGrass4 = 16,
    kFootstepGrass5 = 17,
    kFootstepGrass6 = 18,
    kFootstepStone1 = 19,
    kFootstepStone2 = 20,
    kFootstepStone3 = 21,
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
