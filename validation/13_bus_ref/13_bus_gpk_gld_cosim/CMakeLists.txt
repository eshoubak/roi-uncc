cmake_minimum_required(VERSION 3.21)
project(PowerFlow)

enable_language(CXX)

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

target_include_directories(powerflow_ex_A.x
  PRIVATE
  /usr/lib/x86_64-linux-gnu/openmpi/include
  /usr/local/ga-5.8/include
  /usr/local/GridPACK/include
  /usr/local/helics/include
)

target_include_directories(powerflow_ex_B.x
  PRIVATE
  /usr/lib/x86_64-linux-gnu/openmpi/include
  /usr/local/ga-5.8/include
  /usr/local/GridPACK/include
  /usr/local/helics/include
)

target_include_directories(powerflow_ex_C.x
  PRIVATE
  /usr/lib/x86_64-linux-gnu/openmpi/include
  /usr/local/ga-5.8/include
  /usr/local/GridPACK/include
  /usr/local/helics/include
)

# List of library names (FIXED: added "helics")
set(LIBRARIES_NAMES
  "mpi"
  "mpi_cxx"
  "petsc"
  "parmetis"
  "boost_random"
  "boost_serialization"
  "boost_mpi"
  "armci"
  "ga"
  "ga++"
  "gridpack_configuration"
  "gridpack_math"
  "gridpack_block_parsers"
  "gridpack_stream"
  "gridpack_components"
  "gridpack_ymatrix_components"
  "gridpack_pfmatrix_components"
  "gridpack_partition"
  "gridpack_parallel"
  "gridpack_dynamic_simulation_full_y_module"
  "gridpack_powerflow_module"
  "gridpack_environment"
  "gridpack_timer"
  "helicscpp"
  "helics"        # <-- ADDED THIS LINE (HELICS C API)
)

# Variable to hold the paths of found libraries
set(LIBRARIES_FOUND)

# Loop to find each library in the specified directories
foreach(LIBRARY_NAME IN LISTS LIBRARIES_NAMES)
  find_library(LIBRARY_PATH NAMES ${LIBRARY_NAME}
    PATHS
      /usr/local/GridPACK/lib
      /usr/local/ga-5.8/lib
      /usr/local/petsc-3.16.4/lib
      /usr/local/boost-1.78.0/lib
      /usr/lib/x86_64-linux-gnu/openmpi/lib
      /usr/local/helics/lib          # <-- this already contains libhelics.so
    NO_CACHE
    NO_DEFAULT_PATH
  )

  if(NOT LIBRARY_PATH)
    message(FATAL_ERROR "Library '${LIBRARY_NAME}' could not be found.")
  endif()

  list(INSERT LIBRARIES_FOUND 0 ${LIBRARY_PATH})
  unset(LIBRARY_PATH)
endforeach()

message(STATUS "All Libraries found: ${LIBRARIES_FOUND}")

# Link all found libs
target_link_libraries(powerflow_ex_A.x PRIVATE ${LIBRARIES_FOUND})
target_link_libraries(powerflow_ex_B.x PRIVATE ${LIBRARIES_FOUND})
target_link_libraries(powerflow_ex_C.x PRIVATE ${LIBRARIES_FOUND})

# Copy example files
add_custom_command(
  TARGET powerflow_ex_A.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_13_bus_phase_A.xml" "${CMAKE_BINARY_DIR}/input_13_bus_phase_A.xml"
  COMMENT "Copying Phase A xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_A.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE13_phase_A.raw" "${CMAKE_BINARY_DIR}/IEEE13_phase_A.raw"
  COMMENT "Copying Phase A network file to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_B.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_13_bus_phase_B.xml" "${CMAKE_BINARY_DIR}/input_13_bus_phase_B.xml"
  COMMENT "Copying Phase B xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_B.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE13_phase_B.raw" "${CMAKE_BINARY_DIR}/IEEE13_phase_B.raw"
  COMMENT "Copying Phase B network file to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_C.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/input_13_bus_phase_C.xml" "${CMAKE_BINARY_DIR}/input_13_bus_phase_C.xml"
  COMMENT "Copying Phase C xml configuration to the build directory"
)

add_custom_command(
  TARGET powerflow_ex_C.x POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/IEEE13_phase_C.raw" "${CMAKE_BINARY_DIR}/IEEE13_phase_C.raw"
  COMMENT "Copying Phase C network file to the build directory"
)
