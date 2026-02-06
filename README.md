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

## Running
The executable will be under:

```
build/Debug/cubeos_voxel.exe
```

## Controls
- WASD move, Space jump, Mouse look.
- LMB remove block (adds to inventory), RMB place block (consumes from hotbar).
- 1-9 select hotbar slot.
- Tab toggle inventory (cursor enabled).
- Inventory: LMB drag/drop stacks, RMB split/place single items, same-type stacks merge up to 64.
- Esc closes inventory; exits when inventory is closed.

## Notes
- Validation layers are enabled by default (toggle with `-DCUBEOS_ENABLE_VALIDATION=OFF`).
- Next steps will add chunked voxel world, greedy meshing, and a simple player controller.
