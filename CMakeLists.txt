cmake_minimum_required(VERSION 3.20)
# CMake 4.x dropped compatibility with <3.5. Keep deps like older GLM working.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CUBEOS_VERSION "0.2.0")
set(CUBEOS_MACOS_APP_NAME "CubeOS v${CUBEOS_VERSION} Beta")

project(cubeos_voxel VERSION ${CUBEOS_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CUBEOS_ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

find_package(Threads REQUIRED)
find_path(CUBEOS_VULKAN_INCLUDE_DIR vulkan/vulkan.h
  HINTS
    /usr/local/include
    /opt/homebrew/include
)
if (NOT CUBEOS_VULKAN_INCLUDE_DIR)
  message(FATAL_ERROR "Vulkan headers not found. Install Vulkan SDK or Vulkan headers.")
endif()

FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

find_program(GLSLC glslc)
if (NOT GLSLC AND DEFINED ENV{VULKAN_SDK})
  if (WIN32)
    set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
  else()
    set(GLSLC "$ENV{VULKAN_SDK}/bin/glslc")
  endif()
endif()

if (NOT GLSLC)
  message(FATAL_ERROR "glslc not found. Install Vulkan SDK and ensure VULKAN_SDK is set.")
endif()

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

set(SHADER_SOURCES
  ${SHADER_SOURCE_DIR}/cube.vert
  ${SHADER_SOURCE_DIR}/ui.vert
  ${SHADER_SOURCE_DIR}/cube.frag
)

set(SHADER_BINARIES
  ${SHADER_BINARY_DIR}/cube.vert.spv
  ${SHADER_BINARY_DIR}/ui.vert.spv
  ${SHADER_BINARY_DIR}/cube.frag.spv
)

add_custom_command(
  OUTPUT ${SHADER_BINARY_DIR}/cube.vert.spv
  COMMAND "${GLSLC}" -o ${SHADER_BINARY_DIR}/cube.vert.spv ${SHADER_SOURCE_DIR}/cube.vert
  DEPENDS ${SHADER_SOURCE_DIR}/cube.vert
)

add_custom_command(
  OUTPUT ${SHADER_BINARY_DIR}/ui.vert.spv
  COMMAND "${GLSLC}" -o ${SHADER_BINARY_DIR}/ui.vert.spv ${SHADER_SOURCE_DIR}/ui.vert
  DEPENDS ${SHADER_SOURCE_DIR}/ui.vert
)

add_custom_command(
  OUTPUT ${SHADER_BINARY_DIR}/cube.frag.spv
  COMMAND "${GLSLC}" -o ${SHADER_BINARY_DIR}/cube.frag.spv ${SHADER_SOURCE_DIR}/cube.frag
  DEPENDS ${SHADER_SOURCE_DIR}/cube.frag
)

add_custom_target(shaders ALL DEPENDS ${SHADER_BINARIES})

add_executable(cubeos_voxel
  src/main.cpp
  src/app.cpp
  src/vk_context.cpp
  src/world.cpp
)

target_include_directories(cubeos_voxel PRIVATE src)
target_include_directories(cubeos_voxel PRIVATE ${CUBEOS_VULKAN_INCLUDE_DIR})
target_link_libraries(cubeos_voxel PRIVATE glfw volk::volk glm::glm Threads::Threads)
target_compile_definitions(cubeos_voxel PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(cubeos_voxel PRIVATE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_dependencies(cubeos_voxel shaders)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(cubeos_voxel PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,-Bstatic
    -lwinpthread
    -Wl,-Bdynamic
  )
endif()

if (APPLE)
  target_compile_definitions(cubeos_voxel PRIVATE VK_USE_PLATFORM_MACOS_MVK)
  set_target_properties(cubeos_voxel PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "${CUBEOS_MACOS_APP_NAME}"
    MACOSX_BUNDLE_BUNDLE_NAME "${CUBEOS_MACOS_APP_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cubeos.beta"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${CUBEOS_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${CUBEOS_VERSION}"
  )

  set(CUBEOS_MACOS_VULKAN_LIB "/usr/local/lib/libvulkan.1.dylib" CACHE FILEPATH
      "Path to bundled libvulkan.1.dylib for macOS app distribution")
  set(CUBEOS_MACOS_MOLTENVK_LIB "/usr/local/lib/libMoltenVK.dylib" CACHE FILEPATH
      "Path to bundled libMoltenVK.dylib for macOS app distribution")
  if (NOT EXISTS "${CUBEOS_MACOS_VULKAN_LIB}")
    message(FATAL_ERROR "CUBEOS_MACOS_VULKAN_LIB not found: ${CUBEOS_MACOS_VULKAN_LIB}")
  endif()
  if (NOT EXISTS "${CUBEOS_MACOS_MOLTENVK_LIB}")
    message(FATAL_ERROR "CUBEOS_MACOS_MOLTENVK_LIB not found: ${CUBEOS_MACOS_MOLTENVK_LIB}")
  endif()
endif()

add_custom_command(
  TARGET cubeos_voxel POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cubeos_voxel>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_BINARIES} $<TARGET_FILE_DIR:cubeos_voxel>/shaders
)

if (APPLE)
  add_custom_command(
    TARGET cubeos_voxel POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      -DAPP_BUNDLE="$<TARGET_BUNDLE_DIR:cubeos_voxel>"
      -DAPP_BIN="$<TARGET_FILE:cubeos_voxel>"
      -DVULKAN_LIB="${CUBEOS_MACOS_VULKAN_LIB}"
      -DMOLTENVK_LIB="${CUBEOS_MACOS_MOLTENVK_LIB}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleMacOS.cmake"
  )
endif()

if (CUBEOS_ENABLE_VALIDATION)
  target_compile_definitions(cubeos_voxel PRIVATE CUBEOS_ENABLE_VALIDATION=1)
endif()

if (MSVC)
  target_compile_options(cubeos_voxel PRIVATE /W4 /permissive-)
else()
  target_compile_options(cubeos_voxel PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (APPLE)
  install(TARGETS cubeos_voxel BUNDLE DESTINATION . COMPONENT Runtime)

  set(CPACK_PACKAGE_NAME "CubeOS")
  set(CPACK_PACKAGE_VENDOR "CubeOS")
  set(CPACK_PACKAGE_VERSION "${CUBEOS_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "CubeOS-v${CUBEOS_VERSION}-Beta-macOS")
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "${CUBEOS_MACOS_APP_NAME}")
  include(CPack)
endif()
