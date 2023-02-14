# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT sparsdr_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT sparsdr_sources)

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND sparsdr_python_files
    python_bindings.cc
    band_spec_python.cc
    band_spec_vector_python.cc
    combined_pluto_receiver_python.cc
    combined_usrp_receiver_python.cc
    compressing_pluto_source_python.cc
    compressing_usrp_source_python.cc
    reconstruct_python.cc
    simple_combined_pluto_receiver_python.cc
    simple_combined_usrp_receiver_python.cc
    )



GR_PYBIND_MAKE_OOT(sparsdr
   ../../..
   gr::sparsdr
   "${sparsdr_python_files}")

# copy in bindings .so file for use in QA test module
add_custom_target(
  copy_bindings_for_tests ALL
  COMMAND
    ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/*.so"
    ${CMAKE_BINARY_DIR}/test_modules/gnuradio/sparsdr/
  DEPENDS sparsdr_python)

install(TARGETS sparsdr_python DESTINATION ${GR_PYTHON_DIR}/gnuradio/sparsdr COMPONENT pythonapi)
