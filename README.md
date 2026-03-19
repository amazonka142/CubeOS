# CubeOS

CubeOS is a single-player voxel sandbox prototype made with C++20 and Vulkan
(MoltenVK on macOS). The project aims to deliver a Minecraft-style gameplay
loop while staying small, readable, and easy to iterate on.

## What Kind of Game Is It?

CubeOS is about exploring and shaping a procedural block world:

- Generate a world from a seed.
- Move through mountains, caves, beaches, and oceans.
- Mine blocks and place them back into the world.
- Pick up dropped block items.
- Manage hotbar + inventory stacks.
- Save/load worlds with per-world player state.

Current scope is core sandbox gameplay and engine foundations, not a content-
complete survival game yet.

## Current Feature Set (v0.2.2)

- Procedural chunked world generation.
- Multi-noise climate sampling (`temperature/humidity/continentalness/erosion/depth/weirdness`) with deterministic biome mapping.
- Router-style terrain shaping with stronger continents/peaks/valleys behavior.
- Aquifer-driven underground fluid placement tuned to avoid surface flood bias.
- Region-anchored deterministic structure placement with biome-aware variants.
- Layered surface rules and biome/climate-weighted feature placement.
- Runtime chunk streaming around player position.
- Asynchronous chunk generation and mesh rebuild workers.
- Voxel meshing with block atlas texturing.
- Basic water and falling-sand simulation.
- First-person movement, collision, and block raycast interaction.
- Crafting/workbench panel with highlighted result feedback plus furnace smelting UI.
- Early progression loop: wood -> stone -> caves -> iron -> diamonds.
- Reward chests and basic interaction sound effects.
- Achievement tree overlay (`L`) for milestone tracking.
- Main menu, world creation, world selection, pause, and settings screens.
- Worldgen debug overlay (`F3`/`F4` + density slice controls).
- Deterministic worldgen regression suite target (`cubeos_worldgen_regression`).
- CPU-load throttling for menus/unfocused state and conservative worker budgets.
- Per-world saves and basic settings persistence.

## Known Limitation

- CPU usage is reduced compared to earlier builds, but can still be higher than
  desired in heavy world-streaming scenarios on macOS and Windows.

## Controls

- `WASD`: move
- `Space`: jump / swim up
- `Shift`: swim down
- Mouse: look around
- `LMB`: break block (adds to inventory)
- `RMB`: place block (consumes from hotbar)
- `1-9`: select hotbar slot
- `Tab`: open/close inventory
- `Esc`: close inventory or open pause menu
- `F3`: toggle worldgen debug overlay
- `F4`: cycle debug overlay detail mode
- `PageUp` / `PageDown`: move density slice in debug overlay

## Build

### Windows (Visual Studio)

1. Install Vulkan SDK (LunarG) and ensure `VULKAN_SDK` is set.
2. Install CMake and Visual Studio 2022 (Desktop C++).
3. Configure and build:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The first configure/build fetches GLFW/GLM/volk via CMake `FetchContent`.

### macOS (MoltenVK)

1. Install Vulkan SDK for macOS (MoltenVK) and ensure `VULKAN_SDK` is set.
2. Install CMake and Xcode command line tools.
3. Configure and build:

```bash
cmake -S . -B build
cmake --build build
```

Shaders are compiled with `glslc` and copied near the executable.

### Presets

```bash
cmake --preset debug
cmake --build --preset debug
```

For optimized builds:

```bash
cmake --preset release
cmake --build --preset release
```

### Worldgen Regression Suite

The project includes a deterministic worldgen regression runner:

```bash
cmake -S . -B build
cmake --build build --target cubeos_worldgen_regression
ctest --test-dir build -R cubeos_worldgen_regression --output-on-failure
```

Disable building this suite with:

```bash
cmake -S . -B build -DCUBEOS_BUILD_WORLDGEN_REGRESSION=OFF
```

## Run

- Windows binary:
  - `build/Release/cubeos_voxel.exe` (or `build/Debug/cubeos_voxel.exe`)
- macOS binary:
  - `build/cubeos_voxel`
- macOS app bundle (from this repo build setup):
  - `build/CubeOS v0.2.2 Beta.app`

## Save Data Layout

- `saves/*.bin`: world data files
- `saves/*.meta`: world display-name metadata
- `saves/*.player`: per-world player position/rotation state
- `settings.cfg`: graphics, sensitivity, language, and other user settings

## Project Structure

- `src/app.*`: game loop, UI, player controls, high-level state
- `src/world.*`: generation, streaming, meshing, block simulation
- `src/vk_context.*`: Vulkan setup and rendering path
- `shaders/`: GLSL shader sources compiled during build
- `cmake/BundleMacOS.cmake`: macOS bundle packaging logic

## Roadmap Direction

- Push worldgen toward Minecraft Java 1.18+ style logic:
  - richer multi-noise climate sampling and biome mapping
  - density-router terrain shaping with stronger land/continent behavior
  - improved aquifer logic to reduce water-world outcomes
  - more deterministic structure starts/references
- Continue improving in-game UX and debugging tools for worldgen iteration.

## Notes

- Validation layers are off by default (enable with
  `-DCUBEOS_ENABLE_VALIDATION=ON`).

## Troubleshooting

- `glslc not found`: install Vulkan SDK and export `VULKAN_SDK`.
- `Vulkan headers not found`: install Vulkan headers or pass include paths in
  CMake cache options.
- Black output after startup: ensure `.spv` shader files are present in
  `shaders/` near the executable.
