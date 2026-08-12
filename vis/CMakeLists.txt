cmake_minimum_required(VERSION 3.12)
project(Deepfluoro_3D_rendering)

set(CMAKE_CXX_STANDARD 14) # HighFive requires C++14 or newer
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Set Build Type to Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# 2. Add HighFive Subdirectory
add_subdirectory(third_party/HighFive)

# 3. Find VTK and include RenderingAnnotation
find_package(VTK REQUIRED COMPONENTS
  CommonCore
  CommonDataModel
  InteractionStyle
  RenderingCore
  RenderingOpenGL2
  RenderingVolume
  RenderingVolumeOpenGL2
  RenderingAnnotation      # <--- ADDED THIS MODULE FOR vtkCubeAxesActor
)

# 4. Define your executable
add_executable(render3d vis.cpp)

# 5. Link libraries
target_link_libraries(render3d PRIVATE 
  HighFive
  ${VTK_LIBRARIES}
)

# 6. Auto-configure flags and plumbing for VTK 
vtk_module_autoinit(
  TARGETS render3d
  MODULES ${VTK_LIBRARIES}
)
