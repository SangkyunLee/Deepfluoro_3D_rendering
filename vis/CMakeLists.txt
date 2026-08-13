cmake_minimum_required(VERSION 3.12)
project(QtVtkImageApp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(VTK REQUIRED COMPONENTS 
  CommonCore
  CommonDataModel
  ImagingCore
  InteractionStyle
  RenderingCore
  RenderingOpenGL2
  GUISupportQt
)
set(PROJECT_SOURCES
    qtmain.cpp    
    include/qtmain.h      # <-- Explicitly include here
)
add_executable(QtVtkImageApp ${PROJECT_SOURCES})
target_link_libraries(QtVtkImageApp PRIVATE 
  Qt6::Widgets
  VTK::CommonCore
  VTK::CommonDataModel
  VTK::ImagingCore
  VTK::InteractionStyle
  VTK::RenderingCore
  VTK::RenderingOpenGL2
  VTK::GUISupportQt
)

# Necessary for VTK auto-initialization
vtk_module_autoinit(
  TARGETS QtVtkImageApp
  MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle
)
