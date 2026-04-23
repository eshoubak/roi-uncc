cmake_minimum_required(VERSION 3.21)
project(PowerFlow)

enable_language(CXX)

find_package(MPI REQUIRED)

add_executable(powerflow_ex_A.x
  powerflow_ex_A.cpp
  pf_app.cpp
  pf_factory.cpp
)

add_executable(powerflow_ex_B.x
  powerflow_ex_B.cpp
  pf_app.cpp
  pf_factory.cpp
)

add_executable(powerflow_ex_C.x
  powerflow_ex_C.cpp
  pf_app.cpp
  pf_factory.cpp
)

set(COMMON_INCLUDE_DIRS
  /usr/local/ga-5.8/include
  /usr/local/GridPACK/include
  /usr/local/helics/include
  ${MPI_C_INCLUDE_DIRS}
  ${MPI_CXX_INCLUDE_DIRS}
)

target_include_directories(powerflow_ex_A.x PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(powerflow_ex_B.x PRIVATE ${COMMON_INCLUDE_DIRS})
target_include_directories(powerflow_ex_C.x PRIVATE ${COMMON_INCLUDE_DIRS})

set(LIBRARIES_NAMES
  petsc
  parmetis
  boost_random
  boost_mpi
  boost_serialization
  ga++
  ga
  armci
  gridpack_configuration
  gridpack_math
  gridpack_block_parsers
  gridpack_stream
  gridpack_components
  gridpack_ymatrix_components
  gridpack_pfmatrix_components
  gridpack_partition
  gridpack_parallel
  gridpack_dynamic_simulation_full_y_module
  gridpack_powerflow_module
  gridpack_environment
  gridpack_timer
  helicscpp
  helics
)

set(LIBRARIES_FOUND)

foreach(LIBRARY_NAME IN LISTS LIBRARIES_NAMES)
  find_library(LIBRARY_PATH NAMES ${LIBRARY_NAME}
    PATHS
      /usr/local/GridPACK/lib
      /usr/local/ga-5.8/lib
      /usr/local/petsc-3.16.4/lib
      /usr/local/boost-1.78.0/lib
      /usr/lib64/openmpi/lib
      /usr/local/helics/lib
    NO_CACHE
    NO_DEFAULT_PATH
  )

  if(NOT LIBRARY_PATH)
    message(FATAL_ERROR "Library '${LIBRARY_NAME}' could not be found.")
  endif()

  list(APPEND LIBRARIES_FOUND ${LIBRARY_PATH})
  unset(LIBRARY_PATH)
endforeach()

message(STATUS "All Libraries found: ${LIBRARIES_FOUND}")

set(COMMON_LINK_LIBS
  -Wl,--start-group
  ${LIBRARIES_FOUND}
  -Wl,--end-group
  MPI::MPI_C
  MPI::MPI_CXX
)

target_link_libraries(powerflow_ex_A.x PRIVATE ${COMMON_LINK_LIBS})
target_link_libraries(powerflow_ex_B.x PRIVATE ${COMMON_LINK_LIBS})
target_link_libraries(powerflow_ex_C.x PRIVATE ${COMMON_LINK_LIBS})

add_custom_command(
  TARGET powerflow_ex_A.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_3_bus_phase_A.xml" "${CMAKE_BINARY_DIR}/input_3_bus_phase_A.xml"
  COMMENT "Copying Phase A xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_A.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE3_phase_A.raw" "${CMAKE_BINARY_DIR}/IEEE3_phase_A.raw"
  COMMENT "Copying Phase A network file to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_B.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_3_bus_phase_B.xml" "${CMAKE_BINARY_DIR}/input_3_bus_phase_B.xml"
  COMMENT "Copying Phase B xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_B.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE3_phase_B.raw" "${CMAKE_BINARY_DIR}/IEEE3_phase_B.raw"
  COMMENT "Copying Phase B network file to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_C.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_3_bus_phase_C.xml" "${CMAKE_BINARY_DIR}/input_3_bus_phase_C.xml"
  COMMENT "Copying Phase C xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_C.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE3_phase_C.raw" "${CMAKE_BINARY_DIR}/IEEE3_phase_C.raw"
  COMMENT "Copying Phase C network file to the build directory"
)
