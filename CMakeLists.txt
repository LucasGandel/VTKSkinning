cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0053 NEW)
cmake_policy(SET CMP0071 NEW)

project(SkinnedMeshViewer)

# Find VTK
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

# Find Qt5
find_package(Qt5Widgets)

# Find assimp
# (Set assimp_DIR to the "lib/cmake/assimp-**" folder of your assimp install tree)
find_package(assimp REQUIRED)
#Assimp does not export a proper library target.
find_library(ASSIMP_LIBRARY assimp${ASSIMP_LIBRARY_SUFFIX} HINTS ${ASSIMP_LIBRARY_DIRS})

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${VTK_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS} ${FBXSDK_INCLUDE_DIR})

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

qt5_wrap_ui(SMV_UI smvMainWindow.ui)

# Source files
set(SkinnedMeshViewer_SRCS
  ${SMV_UI}
  smvMainWindow.cxx
  smvRenderManager.cxx
  vtkAssimpImporter.cxx
  vtkMaterial.cxx
  vtkSkeletonAnimation.cxx
  vtkSkeletonAnimationStack.cxx
  vtkSkeletonAnimationKeys.cxx
  vtkSkeletonHierarchy.cxx
  vtkSkeletonPose.cxx
  vtkSkeletonPolyDataMapper.cxx)

set(SkinnedMeshViewer_HDRS
  smvMainWindow.h
  smvRenderManager.h
  vtkAssimpImporter.h
  vtkMaterial.h
  vtkSkeletonAnimation.h
  vtkSkeletonAnimationStack.h
  vtkSkeletonAnimationKeys.h
  vtkSkeletonHierarchy.h
  vtkSkeletonPose.h
  vtkSkeletonPolyDataMapper.h)

# Create target
add_executable(SkinnedMeshViewer MACOSX_BUNDLE smvMain.cxx ${SkinnedMeshViewer_SRCS} ${SkinnedMeshViewer_HDRS})
target_link_libraries(SkinnedMeshViewer ${VTK_LIBRARIES} Qt5::Widgets ${ASSIMP_LIBRARY})
