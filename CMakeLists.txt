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

find_package(Vulkan REQUIRED)

add_executable(cubeos_voxel
  src/main.cpp
  src/app.cpp
  src/vk_context.cpp
)

target_include_directories(cubeos_voxel PRIVATE src)
target_link_libraries(cubeos_voxel PRIVATE glfw Vulkan::Vulkan)
target_compile_definitions(cubeos_voxel PRIVATE GLFW_INCLUDE_NONE)

if (CUBEOS_ENABLE_VALIDATION)
  target_compile_definitions(cubeos_voxel PRIVATE CUBEOS_ENABLE_VALIDATION=1)
endif()

if (MSVC)
  target_compile_options(cubeos_voxel PRIVATE /W4 /permissive-)
else()
  target_compile_options(cubeos_voxel PRIVATE -Wall -Wextra -Wpedantic)
endif()
