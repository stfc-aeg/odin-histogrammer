# Require CMake 3.15+ (matching scikit-build-core) Use new versions of all
# policies up to CMake 4.0
cmake_minimum_required(VERSION 3.15...4.0)

# Scikit-build-core sets these values for you, or you can just hard-code the
# name and version.
project(
  ${SKBUILD_PROJECT_NAME}
  VERSION ${SKBUILD_PROJECT_VERSION}
  LANGUAGES CXX C)

# Appends the cmake modules path inside the MAKE_MODULE_PATH variable
# which stores the directories of additonal CMake modules (ie findxdma_hexitec.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(HDF5 COMPONENTS C HL CXX REQUIRED)

# ensure the env variable, if it exists, is set as a local CMAKE variable
if(DEFINED ENV{DET_SOFTWARE_ROOT})
  set(DET_SOFTWARE_ROOT $ENV{DET_SOFTWARE_ROOT})
endif()
if(DET_SOFTWARE_ROOT)
  message(STATUS "DET Software Root set: ${DET_SOFTWARE_ROOT}")
  set(HEXITEC_ROOT_DIR ${DET_SOFTWARE_ROOT}/none_vme/hexitec)
  set(HEXITEC_INCLUDE_DIRS
    ${HEXITEC_ROOT_DIR}/include
    ${HEXITEC_ROOT_DIR}/qdma
    ${DET_SOFTWARE_ROOT}/libs/include
  )
else()
  message(FATAL_ERROR "DET_SOFTWARE_ROOT must be set to install module")
endif()


set(HEXITEC_SRC_FILES
  ${HEXITEC_ROOT_DIR}/lib/xdma.cpp
  ${HEXITEC_ROOT_DIR}/lib/qdma.cpp
  ${HEXITEC_ROOT_DIR}/lib/xdma_hbm_hist.cpp
  ${HEXITEC_ROOT_DIR}/lib/udp_core.cpp

  ${HEXITEC_ROOT_DIR}/lib/xdma_hexitec.cpp
  ${HEXITEC_ROOT_DIR}/lib/xdma_hexitec_c.cpp
  ${HEXITEC_ROOT_DIR}/lib/xdma_hexitec_dma.cpp

  ${HEXITEC_ROOT_DIR}/lib/baseline_and_trigger.cpp
  ${HEXITEC_ROOT_DIR}/lib/cshare_and_format.cpp
  ${HEXITEC_ROOT_DIR}/lib/circular_hdf_writer.cpp
  ${HEXITEC_ROOT_DIR}/lib/event_list_writer.cpp
  ${HEXITEC_ROOT_DIR}/lib/linearity_hdf5.cpp
  ${HEXITEC_ROOT_DIR}/lib/save_restore_settings_hdf5.cpp
  ${HEXITEC_ROOT_DIR}/lib/save_spectra_hdf5.cpp
  ${HEXITEC_ROOT_DIR}/lib/save_spectra.cpp

  ${HEXITEC_ROOT_DIR}/lib/hexitec_hbm.cpp
  ${HEXITEC_ROOT_DIR}/lib/hexitec_udp.cpp
  ${HEXITEC_ROOT_DIR}/lib/hexitec_itfg.cpp
)

# QDMA UTIL OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(hexitec_qdma OBJECT
  ${HEXITEC_ROOT_DIR}/qdma/dmautils.c
  ${HEXITEC_ROOT_DIR}/qdma/dmactl.c
  ${HEXITEC_ROOT_DIR}/qdma/dmactl_reg.c
)

target_include_directories(hexitec_qdma PRIVATE
  ${HEXITEC_ROOT_DIR}/include
  ${HEXITEC_ROOT_DIR}/qdma
)

target_compile_options(hexitec_qdma PRIVATE
  -D_FILE_OFFSET_BITS=64
  -D_GNU_SOURCE
  -D_LARGE_FILE_SOURCE
  -D_AIO_AIX_SOURCE 
  -fPIC
)
# DETFILE OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(hexitec_detfile OBJECT
  ${DET_SOFTWARE_ROOT}/libs/src/detfile/detfile.c
)

target_include_directories(hexitec_detfile PRIVATE
  ${DET_SOFTWARE_ROOT}/libs/src/detfile
)

# IMG_MOD OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(img_mod OBJECT
  ${DET_SOFTWARE_ROOT}/display/id/img_mod_lib/img_mod.c
  ${DET_SOFTWARE_ROOT}/display/id/img_mod_lib/img_mod_linux.c
)
target_include_directories(img_mod PRIVATE
  ${DET_SOFTWARE_ROOT}/libs/include
)
target_compile_options(img_mod PRIVATE
  -D__LINUX__=1
)


# HEXITEC STATIC LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(hexitec STATIC
  $<TARGET_OBJECTS:hexitec_qdma>
  $<TARGET_OBJECTS:hexitec_detfile>
  $<TARGET_OBJECTS:img_mod>
  ${HEXITEC_SRC_FILES}
)

message(STATUS "Adding include dirs to hexitec")
# add directories containing header files to xdma_hexitec
target_include_directories(hexitec PUBLIC 
  ${HEXITEC_INCLUDE_DIRS}
  ${HDF5_INCLUDE_DIRS}
)


message(STATUS "Adding Linked Libs to hexitec")

# link pre-existing objects to xdma_hexitec
target_link_libraries(hexitec PUBLIC
  ${HDF5_LIBRARIES}
  ${HDF5_HL_LIBRARIES}
  ${DETFILE_LIBRARY}
)

target_compile_options(hexitec PUBLIC -fPIC -O2 -fno-omit-frame-pointer)

install(TARGETS hexitec DESTINATION xdma_hexitec)


# PYTHON MODULE LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Find pybind11 and Python
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG)

# Add a library using FindPython's tooling (pybind11 also provides a helper like this)
pybind11_add_module(_core MODULE src/main.cpp)
target_include_directories(_core PUBLIC ${XDMA_HEXITEC_INCLUDE_DIRS})
target_link_libraries(_core PRIVATE pybind11::headers)
target_link_libraries(_core PUBLIC
  hexitec
  /aeg_sw/work/users/wbd45595/develop/libraries/xdma_hexitec/libdetfile.a)


# This is passing in the version as a define just as an example
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION xdma_hexitec)
