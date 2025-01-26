cmake_minimum_required(VERSION 3.10)

# Project name and version
project(MinecraftClone VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Specify executable output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/renderer.cpp
    src/block.cpp
    src/player.cpp
    src/world.cpp
    src/perlin_noise.cpp
    src/inventory.cpp
    src/tree_generator.cpp
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/glad
    ${CMAKE_SOURCE_DIR}/include/glfw
)

# Add glad as a static library
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/include/glad/glad.c)

# Find and link GLFW
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Add the executable
add_executable(MinecraftClone ${SOURCE_FILES})

# Link libraries
target_link_libraries(MinecraftClone glad glfw OpenGL::GL)

# Copy shaders and textures to the output directory
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(COPY ${CMAKE_SOURCE_DIR}/textures DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Output information
message(STATUS "Executable will be generated in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
