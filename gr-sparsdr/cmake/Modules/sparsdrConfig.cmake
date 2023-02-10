find_package(PkgConfig)

PKG_CHECK_MODULES(PC_SPARSDR gnuradio-sparsdr)

FIND_PATH(
    SPARSDR_INCLUDE_DIRS
    NAMES gnuradio/sparsdr/api.h
    HINTS $ENV{SPARSDR_DIR}/include
        ${PC_SPARSDR_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    SPARSDR_LIBRARIES
    NAMES gnuradio-sparsdr
    HINTS $ENV{SPARSDR_DIR}/lib
        ${PC_SPARSDR_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-sparsdrTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPARSDR DEFAULT_MSG SPARSDR_LIBRARIES SPARSDR_INCLUDE_DIRS)
MARK_AS_ADVANCED(SPARSDR_LIBRARIES SPARSDR_INCLUDE_DIRS)
