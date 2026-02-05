cmake_minimum_required(VERSION 3.20)

project(cubeos_voxel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CUBEOS_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

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

find_package(Vulkan REQUIRED)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

if (DEFINED Vulkan_GLSLC_EXECUTABLE)
  set(GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
elseif(DEFINED ENV{VULKAN_SDK})
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
  ${SHADER_SOURCE_DIR}/cube.frag
)

set(SHADER_BINARIES
  ${SHADER_BINARY_DIR}/cube.vert.spv
  ${SHADER_BINARY_DIR}/cube.frag.spv
)

add_custom_command(
  OUTPUT ${SHADER_BINARY_DIR}/cube.vert.spv
  COMMAND "${GLSLC}" -o ${SHADER_BINARY_DIR}/cube.vert.spv ${SHADER_SOURCE_DIR}/cube.vert
  DEPENDS ${SHADER_SOURCE_DIR}/cube.vert
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
)

target_include_directories(cubeos_voxel PRIVATE src)
target_link_libraries(cubeos_voxel PRIVATE glfw Vulkan::Vulkan glm::glm)
target_compile_definitions(cubeos_voxel PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(cubeos_voxel PRIVATE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_dependencies(cubeos_voxel shaders)

add_custom_command(
  TARGET cubeos_voxel POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cubeos_voxel>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_BINARIES} $<TARGET_FILE_DIR:cubeos_voxel>/shaders
)

if (CUBEOS_ENABLE_VALIDATION)
  target_compile_definitions(cubeos_voxel PRIVATE CUBEOS_ENABLE_VALIDATION=1)
endif()

if (MSVC)
  target_compile_options(cubeos_voxel PRIVATE /W4 /permissive-)
else()
  target_compile_options(cubeos_voxel PRIVATE -Wall -Wextra -Wpedantic)
endif()
