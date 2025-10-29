# Require CMake 3.15+ (matching scikit-build-core) Use new versions of all
# policies up to CMake 4.0
cmake_minimum_required(VERSION 3.15...4.0)

# Scikit-build-core sets these values for you, or you can just hard-code the
# name and version.
project(
  ${SKBUILD_PROJECT_NAME}
  VERSION ${SKBUILD_PROJECT_VERSION}
  LANGUAGES CXX)

# Find pybind11 and Python
find_package(pybind11 CONFIG REQUIRED)

if(NOT DET_SOFTWARE)
  set(DET_SOFTWARE $ENV{DET_SOFTWARE})  
endif()

message(STATUS "DET_SOFTWARE set: " ${DET_SOFTWARE})

find_path(XDMA_HEXITEC_INCLUDE_DIR
    NAMES
      xdma_hexitec.h
    PATHS
      ${DET_SOFTWARE}/libs/include
      ${DET_SOFTWARE}/none_vme/hexitec/include
      
    REQUIRED
)

find_path(HDF5_CPP_INCLUDE_DIR
    NAMES
      H5Cpp.h
    PATHS
      /usr/include/hdf5/serial
      ${DET_SOFTWARE}/libs/include
      ${DET_SOFTWARE}/libs/include-hdf5
)

find_library(XDMA_HEXITEC_LIB
    NAMES
      xdma_hexitec
    PATHS
      ${DET_SOFTWARE}/libs/libs.linux.x86_64/lib
      ${DET_SOFTWARE}/none_vme/hexitec/lib/objs.x86_64
    REQUIRED
)

find_library(HDF5_LIB
    NAMES
      hdf5_hl
    PATHS
      ${DET_SOFTWARE}/libs/libs.linux.x86_64/lib
      ${DET_SOFTWARE}/libs/src/HDF5/hdf5-1.14.4-3/hdf5/lib
)
get_filename_component(XDMA_HEXITEC_LIB_DIR ${XDMA_HEXITEC_LIB} PATH)
get_filename_component(HDF5_LIB_DIR ${HDF5_LIB} PATH)

set(INCLUDE_DIRS ${XDMA_HEXITEC_INCLUDE_DIR} ${HDF5_CPP_INCLUDE_DIR})
set(LINK_DIRS ${XDMA_HEXITEC_LIB_DIR} ${HDF5_LIB_DIR})

message(STATUS "INCLUDE DIRS: ${INCLUDE_DIRS}")
message(STATUS "LIB DIRS: ${LINK_DIRS}")

link_directories(${LINK_DIRS})

include_directories(${INCLUDE_DIRS})



# Add a library using FindPython's tooling (pybind11 also provides a helper like
# this)
python_add_library(_core MODULE src/main.cpp WITH_SOABI)
target_link_libraries(_core PRIVATE xdma_hexitec detfile hdf5_hl img_mod rt)
target_link_libraries(_core PRIVATE pybind11::headers)

# This is passing in the version as a define just as an example
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION xdma_hexitec)