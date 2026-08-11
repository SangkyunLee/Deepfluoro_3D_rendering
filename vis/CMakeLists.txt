cmake_minimum_required(VERSION 3.12)
project(RENDER_3D)

set(CMAKE_CXX_STANDARD 17)

# Find Dependencies
find_package(VTK REQUIRED COMPONENTS CommonCore CommonMath CommonExecutionModel ImagingCore IOImage)
# 2. Disable HighFive's built-in unit tests/examples to keep your build clean
set(HIGHFIVE_UNIT_TESTS OFF CACHE INTERNAL "")
set(HIGHFIVE_EXAMPLES OFF CACHE INTERNAL "")

# find_package(HighFive REQUIRED)
add_subdirectory(third_party/HighFive)

add_executable(render3d vis.cpp)

# Link Libraries
target_link_libraries(render3d PRIVATE VTK::CommonCore VTK::CommonMath VTK::CommonExecutionModel VTK::ImagingCore VTK::IOImage HighFive)

# VTK Auto-init configuration
vtk_module_autoinit(TARGETS render3d MODULES VTK::CommonCore VTK::CommonMath VTK::CommonExecutionModel VTK::ImagingCore VTK::IOImage)
