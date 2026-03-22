cmake_minimum_required(VERSION 3.20)
# CMake 4.x dropped compatibility with <3.5. Keep deps like older GLM working.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CUBEOS_VERSION "0.3.0")
set(CUBEOS_DISPLAY_VERSION "${CUBEOS_VERSION} Snapshot 1")
set(CUBEOS_RELEASE_TAG "v0.3.0-snapshot.1")
set(CUBEOS_MACOS_APP_NAME "CubeOS v${CUBEOS_DISPLAY_VERSION}")

project(cubeos_voxel VERSION ${CUBEOS_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CUBEOS_ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)
option(CUBEOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CUBEOS_BUILD_SHADERS "Compile GLSL shaders during build" ON)
option(CUBEOS_BUILD_WORLDGEN_REGRESSION "Build worldgen regression suite" ON)

include(FetchContent)

set(CUBEOS_GLFW_GIT_TAG "3.3.8" CACHE STRING "glfw git tag to fetch")
set(CUBEOS_GLM_GIT_TAG "0.9.9.8" CACHE STRING "glm git tag to fetch")
set(CUBEOS_VOLK_GIT_TAG "1.4.304" CACHE STRING "volk git tag to fetch")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG ${CUBEOS_GLFW_GIT_TAG}
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG ${CUBEOS_GLM_GIT_TAG}
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
  GIT_TAG ${CUBEOS_VOLK_GIT_TAG}
)
FetchContent_MakeAvailable(volk)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_BINARIES)
if (CUBEOS_BUILD_SHADERS)
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

  function(cubeos_add_shader SOURCE_PATH OUTPUT_PATH)
    add_custom_command(
      OUTPUT ${OUTPUT_PATH}
      COMMAND "${GLSLC}" -o ${OUTPUT_PATH} ${SOURCE_PATH}
      DEPENDS ${SOURCE_PATH}
      VERBATIM
    )
  endfunction()

  cubeos_add_shader(${SHADER_SOURCE_DIR}/cube.vert ${SHADER_BINARY_DIR}/cube.vert.spv)
  cubeos_add_shader(${SHADER_SOURCE_DIR}/ui.vert ${SHADER_BINARY_DIR}/ui.vert.spv)
  cubeos_add_shader(${SHADER_SOURCE_DIR}/cube.frag ${SHADER_BINARY_DIR}/cube.frag.spv)

  add_custom_target(shaders ALL DEPENDS ${SHADER_BINARIES})
endif()

add_executable(cubeos_voxel
  src/main.cpp
  src/app.cpp
  src/audio.cpp
  src/vk_context.cpp
  src/world.cpp
)

target_include_directories(cubeos_voxel PRIVATE src)
target_include_directories(cubeos_voxel PRIVATE ${CUBEOS_VULKAN_INCLUDE_DIR})
target_include_directories(cubeos_voxel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)
target_link_libraries(cubeos_voxel PRIVATE glfw volk::volk glm::glm Threads::Threads)
target_compile_definitions(cubeos_voxel PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(cubeos_voxel PRIVATE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)
if (CUBEOS_BUILD_SHADERS)
  add_dependencies(cubeos_voxel shaders)
endif()

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
  target_link_libraries(cubeos_voxel PRIVATE
    "-framework AudioToolbox"
    "-framework AudioUnit"
    "-framework CoreAudio"
    "-framework CoreGraphics"
    "-framework CoreText"
    "-framework CoreFoundation"
    "-framework ImageIO"
  )
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

if (UNIX AND NOT APPLE)
  target_link_libraries(cubeos_voxel PRIVATE dl m)
endif()

if (CUBEOS_BUILD_SHADERS)
  add_custom_command(
    TARGET cubeos_voxel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cubeos_voxel>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_BINARIES} $<TARGET_FILE_DIR:cubeos_voxel>/shaders
  )
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures")
  add_custom_command(
    TARGET cubeos_voxel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cubeos_voxel>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
      "$<TARGET_FILE_DIR:cubeos_voxel>/textures"
  )
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio")
  add_custom_command(
    TARGET cubeos_voxel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cubeos_voxel>/audio
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio"
      "$<TARGET_FILE_DIR:cubeos_voxel>/audio"
  )
endif()

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

if (CUBEOS_BUILD_WORLDGEN_REGRESSION)
  enable_testing()

  add_executable(cubeos_worldgen_regression
    src/worldgen_regression.cpp
    src/world.cpp
  )
  target_include_directories(cubeos_worldgen_regression PRIVATE src)
  target_include_directories(cubeos_worldgen_regression PRIVATE ${CUBEOS_VULKAN_INCLUDE_DIR})
  target_link_libraries(cubeos_worldgen_regression PRIVATE volk::volk glm::glm Threads::Threads)
  target_compile_definitions(cubeos_worldgen_regression PRIVATE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

  add_test(
    NAME cubeos_worldgen_regression
    COMMAND cubeos_worldgen_regression
  )
endif()

if (CUBEOS_ENABLE_VALIDATION)
  target_compile_definitions(cubeos_voxel PRIVATE CUBEOS_ENABLE_VALIDATION=1)
endif()

if (MSVC)
  target_compile_options(cubeos_voxel PRIVATE /W4 /permissive-)
  if (CUBEOS_WARNINGS_AS_ERRORS)
    target_compile_options(cubeos_voxel PRIVATE /WX)
  endif()
else()
  target_compile_options(cubeos_voxel PRIVATE -Wall -Wextra -Wpedantic)
  if (CUBEOS_WARNINGS_AS_ERRORS)
    target_compile_options(cubeos_voxel PRIVATE -Werror)
  endif()
endif()

if (APPLE)
  install(TARGETS cubeos_voxel BUNDLE DESTINATION . COMPONENT Runtime)

  set(CPACK_PACKAGE_NAME "CubeOS")
  set(CPACK_PACKAGE_VENDOR "CubeOS")
  set(CPACK_PACKAGE_VERSION "${CUBEOS_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "CubeOS-${CUBEOS_RELEASE_TAG}-macOS")
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "${CUBEOS_MACOS_APP_NAME}")
  include(CPack)
endif()
