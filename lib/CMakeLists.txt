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

# Appends the cmake modules path inside the MAKE_MODULE_PATH variable
# which stores the directories of additonal CMake modules (ie findxdma_hexitec.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# ensure the env variable, if it exists, is set as a local CMAKE variable
if(DEFINED ENV{DET_SOFTWARE})
  set(DET_SOFTWARE $ENV{DET_SOFTWARE})
  message(STATUS "DET_SOFTWARE set: " ${DET_SOFTWARE})
endif()

find_package(xdma_hexitec REQUIRED)
find_package(HDF5 COMPONENTS C HL CXX REQUIRED)

if (HDF5_FOUND)
  message(STATUS "HDF5 include files:  " ${HDF5_INCLUDE_DIRS} " AND " ${HDF5_C_INCLUDE_DIRS})
  message(STATUS "HDF5 libs:           " ${HDF5_LIBRARIES})
  message(STATUS "HDF5 HL Libs:        " ${HDF5_HL_LIBRARIES})
  message(STATUS "HDF5 C Libs:         " ${HDF5_C_LIBRARIES} " AND " ${HDF5_C_HL_LIBRARIES})
  message(STATUS "HDF5 CXX Libs        " ${HDF5_CXX_LIBRARIES} " AND " ${HDF5_CXX_HL_LIBRARIES})
  message(STATUS "HDF5 defs:           " ${HDF5_DEFINITIONS})
ENDIF(HDF5_FOUND)

include_directories(${XDMA_HEXITEC_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS})

# Add a library using FindPython's tooling (pybind11 also provides a helper like
# this)
python_add_library(_core MODULE src/main.cpp WITH_SOABI)
target_link_libraries(_core PRIVATE pybind11::headers)
target_link_libraries(_core PUBLIC ${XDMA_HEXITEC_LIBRARIES}
                                   ${HDF5_LIBRARIES} 
                                   ${HDF5_HL_LIBRARIES}
                                   ${HDF5_C_LIBRARIES}
                                   ${HDF5_C_HL_LIBRARIES}
                                   ${HDF5_CXX_LIBRARIES})


# This is passing in the version as a define just as an example
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION xdma_hexitec)