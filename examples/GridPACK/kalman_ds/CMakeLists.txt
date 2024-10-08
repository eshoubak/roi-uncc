# -*- mode: cmake -*-
#
#     Copyright (c) 2013 Battelle Memorial Institute
#     Licensed under modified BSD License. A copy of this license can be
#     found
#     in the LICENSE file in the top level directory of this distribution.
#
# -------------------------------------------------------------
# file: CMakeLists.install.in
# -------------------------------------------------------------
# -------------------------------------------------------------
# Created January 28, 2014 by William A. Perkins
# Last Change: 2018-07-18 06:36:35 d3g096
# -------------------------------------------------------------

cmake_minimum_required(VERSION 2.6.4)

if (NOT GRIDPACK_DIR)
  set(GRIDPACK_DIR /usr/local/GridPACK
      CACHE PATH "GridPACK installation directory")
endif()

include("${GRIDPACK_DIR}/lib/GridPACK.cmake")

project(KalmanFilter)

enable_language(CXX)

gridpack_setup()

add_definitions(${GRIDPACK_DEFINITIONS})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(BEFORE ${GRIDPACK_INCLUDE_DIRS})

add_executable(kds.x
   kds_main.cpp
)

target_link_libraries(kds.x ${GRIDPACK_LIBS})

add_custom_target(kds.x.input
 
  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/input_14.xml
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_kds.raw
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_classicGen.dyr
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_Kalman_input_ang.csv
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_Kalman_input_mag.csv
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/input_145.xml
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145.raw
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_classicGen.dyr
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_Kalman_input_ang.csv
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_Kalman_input_mag.csv
  ${CMAKE_CURRENT_BINARY_DIR}

  DEPENDS 
  ${CMAKE_CURRENT_SOURCE_DIR}/input_14.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_kds.raw
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_classicGen.dyr
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_Kalman_input_ang.csv
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14_Kalman_input_mag.csv

  ${CMAKE_CURRENT_SOURCE_DIR}/input_145.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145.raw
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_classicGen.dyr
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_Kalman_input_ang.csv
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE145_Kalman_input_mag.csv
)

add_dependencies(kds.x kds.x.input)

