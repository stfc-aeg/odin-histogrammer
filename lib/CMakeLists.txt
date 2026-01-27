# Require CMake 3.15+ (matching scikit-build-core) Use new versions of all
# policies up to CMake 4.0
cmake_minimum_required(VERSION 3.15...4.0)

# Scikit-build-core sets these values for you, or you can just hard-code the
# name and version.
project(
  ${SKBUILD_PROJECT_NAME}
  VERSION ${SKBUILD_PROJECT_VERSION}
  LANGUAGES CXX C)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(HDF5 COMPONENTS C HL CXX REQUIRED)

set(HEXITEC_SRC_FILES
  cpp/hexitec/xdma.cpp
  cpp/hexitec/qdma.cpp
  cpp/hexitec/xdma_hbm_hist.cpp
  cpp/hexitec/udp_core.cpp

  cpp/hexitec/xdma_hexitec.cpp
  cpp/hexitec/xdma_hexitec_c.cpp
  cpp/hexitec/xdma_hexitec_dma.cpp

  cpp/hexitec/baseline_and_trigger.cpp
  cpp/hexitec/cshare_and_format.cpp
  cpp/hexitec/circular_hdf_writer.cpp
  cpp/hexitec/event_list_writer.cpp
  cpp/hexitec/linearity_hdf5.cpp
  cpp/hexitec/save_restore_settings_hdf5.cpp
  cpp/hexitec/save_spectra_hdf5.cpp
  cpp/hexitec/save_spectra.cpp

  cpp/hexitec/hexitec_hbm.cpp
  cpp/hexitec/hexitec_udp.cpp
  cpp/hexitec/hexitec_itfg.cpp
)

# QDMA UTIL OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(qdma OBJECT
  cpp/qdma/dmautils.c
  cpp/qdma/dmactl.c
  cpp/qdma/dmactl_reg.c
)

target_include_directories(qdma PRIVATE cpp/include)

target_compile_options(qdma PRIVATE
  -D_FILE_OFFSET_BITS=64
  -D_GNU_SOURCE
  -D_LARGE_FILE_SOURCE
  -D_AIO_AIX_SOURCE
)

# DETFILE OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(detfile OBJECT cpp/detfile/detfile.c)
target_include_directories(detfile PRIVATE cpp/include)

# IMG_MOD OBJECTS LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(img_mod OBJECT cpp/img_mod/img_mod.c cpp/img_mod/img_mod_linux.c)
target_include_directories(img_mod PRIVATE cpp/include)
target_compile_options(img_mod PRIVATE -D__LINUX__=1)


# HEXITEC STATIC LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library(hexitec STATIC
  $<TARGET_OBJECTS:qdma>
  $<TARGET_OBJECTS:detfile>
  $<TARGET_OBJECTS:img_mod>
  ${HEXITEC_SRC_FILES}
)

message(STATUS "Adding include dirs to hexitec")
# add directories containing header files to xdma_hexitec
target_include_directories(hexitec PUBLIC 
  cpp/include
  ${HDF5_INCLUDE_DIRS}
)


message(STATUS "Adding Linked Libs to hexitec")

# link pre-existing objects to xdma_hexitec
target_link_libraries(hexitec PUBLIC
  ${HDF5_LIBRARIES}
  ${HDF5_HL_LIBRARIES}
)

target_compile_options(hexitec PUBLIC -fPIC -O2 -fno-omit-frame-pointer)

install(TARGETS hexitec DESTINATION xdma_hexitec)


# PYTHON MODULE LIBRARY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Find pybind11 and Python
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG)

# Add a library using FindPython's tooling (pybind11 also provides a helper like this)
pybind11_add_module(_core MODULE src/main.cpp)
target_include_directories(_core PUBLIC cpp/include)
target_link_libraries(_core PRIVATE pybind11::headers)
target_link_libraries(_core PUBLIC hexitec)


# This is passing in the version as a define just as an example
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION xdma_hexitec)
