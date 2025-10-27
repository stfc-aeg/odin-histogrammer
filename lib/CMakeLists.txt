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

link_directories(
  $ENV{DET_SOFTWARE}/none_vme/hexitec/lib/objs.x86_64
  $ENV{DET_SOFTWARE}/libs/libs.linux.x86_64/lib
  $ENV{DET_SOFTWARE}/libs/src/HDF5/hdf5-1.14.4-3/hdf5/lib
)

include_directories(
  $ENV{DET_SOFTWARE}/none_vme/hexitec/include
  $ENV{DET_SOFTWARE}/libs/include
  $ENV{DET_SOFTWARE}/libs/include-hdf5
)



# Add a library using FindPython's tooling (pybind11 also provides a helper like
# this)
python_add_library(_core MODULE src/main.cpp WITH_SOABI)
target_link_libraries(_core PRIVATE xdma_hexitec detfile img_mod rt)
target_link_libraries(_core PRIVATE pybind11::headers)

# This is passing in the version as a define just as an example
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION xdma_hexitec)