# Tries to find xdma_hexitec headers and libraries
#
# Variables used by this module which may need to be set before calling find_package:
#
# DET_SOFTWARE  Set this variable to the root of William's det-software svn repo
#
# Variables defined by this module:
#
# XDMA_HEXITEC_FOUND          System has the xdma_hexitec libs/headers
# XDMA_HEXITEC_LIBRARIES      The xdma_hexitec libraries
# XDMA_HEXITEC_INCLUDE_DIRS   The location of the Hexitec Headers

message("\nLooking for xdma_hexitec headers and libraries")

if (DET_SOFTWARE)
    message(STATUS "det-software root dir: ${DET_SOFTWARE}")
    string(TOLOWER ${CMAKE_SYSTEM_NAME} DET_SYSTEM_NAME)
    set(DET_SOFTWARE_INCLUDE_DIR ${DET_SOFTWARE}/libs/include)
    set(DET_SOFTWARE_LIBS_DIR ${DET_SOFTWARE}/libs/libs.${DET_SYSTEM_NAME}.${CMAKE_HOST_SYSTEM_PROCESSOR}/lib)
endif()



find_package(PkgConfig)
if(PkgConfig_FOUND)
    message("using Pkgconfig")
    set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR} ${PKG_CONFIG_PATH})
    pkg_check_modules(PC_XDMA_HEXITEC xdma_hexitec)
    message("LIB DIRS: ${PC_XDMA_HEXITEC_LIBDIR} OR ${PC_XDMA_HEXITEC_LIBRARY_DIRS}")
    message("INC DIRS: ${PC_XDMA_HEXITEC_INCLUDEDIR} OR ${PC_XDMA_HEXITEC_INCLUDE_DIRS}")
endif(PkgConfig_FOUND)


find_path(XDMA_HEXITEC_INCLUDE_DIRS
    NAMES
        xdma_hexitec.h
    PATHS
        ${PC_XDMA_HEXITEC_INCLUDEDIR}
        ${PC_XDMA_HEXITEC_INCLUDE_DIRS}
        ${DET_SOFTWARE_INCLUDE_DIR}
)

find_library(XDMA_HEXITEC_LIBRARY
    NAMES
        xdma_hexitec
    PATHS
        ${PC_XDMA_HEXITEC_LIBDIR}
        ${PC_XDMA_HEXITEC_LIBRARY_DIRS}
        ${DET_SOFTWARE_LIBS_DIR}
)

find_library(DETFILE_LIBRARY
    NAMES
        detfile
    PATHS
        ${PC_XDMA_HEXITEC_LIBDIR}
        ${PC_XDMA_HEXITEC_LIBRARY_DIRS}
        ${DET_SOFTWARE_LIBS_DIR}
)

set(XDMA_HEXITEC_LIBRARIES ${XDMA_HEXITEC_LIBRARY} ${DETFILE_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set XDMA_HEXITEC_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(xdma_hexitec
    DEFAULT_MSG
    XDMA_HEXITEC_LIBRARIES
    XDMA_HEXITEC_INCLUDE_DIRS
)

mark_as_advanced(XDMA_HEXITEC_LIBRARIES XDMA_HEXITEC_INCLUDE_DIRS XDMA_HEXITEC_LIBRARY DETFILE_LIBRARY)

if(XDMA_HEXITEC_FOUND)
    message("xdma_hexitec found")
    message(STATUS "Include directories: ${XDMA_HEXITEC_INCLUDE_DIRS}")
    message(STATUS "Libraries: ${XDMA_HEXITEC_LIBRARIES}")
endif()

