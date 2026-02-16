# CubeOS

Vulkan-based voxel MVP (chunks, meshing, basic physics) targeting Windows.

## Windows build (Visual Studio)
1. Install Vulkan SDK (from LunarG) and ensure `VULKAN_SDK` is set.
2. Install CMake and Visual Studio 2022 (Desktop C++).
3. Configure and build:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The first configure/build will fetch GLFW via CMake `FetchContent`.

## macOS build (MoltenVK)
1. Install the Vulkan SDK for macOS (MoltenVK) from LunarG and ensure `VULKAN_SDK` is set.
2. Install CMake and Xcode command line tools.
3. Configure and build:

```bash
cmake -S . -B build
cmake --build build
```

The shaders are compiled with `glslc` from the Vulkan SDK and copied next to the binary.

## Preset-based build
If you prefer presets (see `CMakePresets.json`):

```bash
cmake --preset debug
cmake --build --preset debug
```

For optimized builds:

```bash
cmake --preset release
cmake --build --preset release
```

## Running
The executable will be under:

```
build/Debug/cubeos_voxel.exe
```

On macOS and other non-Windows builds:

```
build/cubeos_voxel
```

## Save data layout
- `saves/*.bin`: world data files.
- `saves/*.meta`: optional world display-name metadata.
- `saves/*.player`: per-world player position/rotation state.
- `settings.cfg`: user settings (graphics, controls sensitivity, language, etc.).

## Controls
- WASD move, Space jump, Mouse look.
- LMB remove block (adds to inventory), RMB place block (consumes from hotbar).
- 1-9 select hotbar slot.
- On hotbar selection change, item name appears briefly on screen.
- Tab toggle inventory (cursor enabled).
- Inventory: LMB drag/drop stacks, RMB split/place single items, same-type stacks merge up to 64.
- Esc closes inventory.
- Esc (with inventory closed) opens pause menu with `Continue`, `Settings`, `Main Menu`.

## v0.2.1 Direction
- Expand world generation toward Minecraft Java 1.18+ style logic:
  - richer multi-noise climate sampling and biome mapping
  - density-router terrain shaping with stronger land/continent behavior
  - improved aquifer logic to prevent water-world outcomes
  - more advanced deterministic structures and structure references
- Continue improving in-game UX and debugability for rapid worldgen iteration.

## Notes
- Validation layers are disabled by default (enable with `-DCUBEOS_ENABLE_VALIDATION=ON`).
- Next steps will add chunked voxel world, greedy meshing, and a simple player controller.

## Troubleshooting
- `glslc not found`: install Vulkan SDK and ensure `VULKAN_SDK` is exported in your shell.
- `Vulkan headers not found`: make sure Vulkan headers are installed, or pass include paths via CMake cache options.
- Blank/black output after startup: verify shader `.spv` files exist next to the executable in `shaders/`.
