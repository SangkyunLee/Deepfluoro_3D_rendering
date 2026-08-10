cmake_minimum_required(VERSION 3.12)
project(RENDER_3D)

set(CMAKE_CXX_STANDARD 17)

# Find Dependencies
find_package(VTK REQUIRED COMPONENTS CommonCore CommonMath)
# find_package(HighFive REQUIRED)
add_subdirectory(third_party/HighFive)

add_executable(render3d vis.cpp)

# Link Libraries
target_link_libraries(render3d PRIVATE VTK::CommonCore VTK::CommonMath HighFive)

# VTK Auto-init configuration
vtk_module_autoinit(TARGETS render3d MODULES VTK::CommonCore VTK::CommonMath)
