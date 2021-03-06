set(MAIN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/")
set(MAIN_BINARY_NAME "coldice")

SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY_BACK ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

FILE(GLOB HSources *.h)
FILE(GLOB HXXSources *.hxx)

SET( MAIN_BINARY_SRC
  coldice.cpp #${CPPSources}
  ${HXXSources}
  ${HSources}
  )

message (STATUS "")
cmessage (STATUS_CYAN "--------------------------------")
cmessage (STATUS_CYAN "***   ColdICE solver setup   ***")
cmessage (STATUS_CYAN "--------------------------------")
message (STATUS "")

SET(SOLVER_COMPILE_PROPERTIES "")

if (NOT DEFINED PROJECTION_FLOAT_TYPE)
  SET(PROJECTION_FLOAT_TYPE "dice::DoubleDouble")
endif()
cmessage(STATUS_GREEN "   * Floating point type used for projection: ${PROJECTION_FLOAT_TYPE}")
message(STATUS "     Type can be defined with '-DPROJECTION_FLOAT_TYPE={double, long double (or longdouble), dice::DoubleDouble, dice::Float128OrMore, dice::QuadDouble}'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_PROJECTION_FLOAT_TYPE=${PROJECTION_FLOAT_TYPE}")

if (NOT DEFINED PROJECTION_HR_FLOAT_TYPE)
  SET(PROJECTION_HR_FLOAT_TYPE "dice::DoubleDouble")
endif()
cmessage(STATUS_GREEN "   * High resolution floating point type used for projection: ${PROJECTION_HR_FLOAT_TYPE}")
message(STATUS "     Type can be defined with '-DPROJECTION_HR_FLOAT_TYPE={double, long double (or longdouble), dice::DoubleDouble, dice::Float128OrMore, dice::QuadDouble}'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_PROJECTION_HR_FLOAT_TYPE=${PROJECTION_HR_FLOAT_TYPE}")

if (NOT DEFINED NDIMS)
  SET(NDIMS "2+3")
endif()
cmessage(STATUS_GREEN "   * Number of spatial dimensions: ${NDIMS}")
message(STATUS "     Number of spatial dimensions can be defined with '-DNDIMS={2,3,2+3}'")

if (NOT DEFINED BOUNDARY_TYPE)
  SET(BOUNDARY_TYPE "PERIODIC")
endif()
cmessage(STATUS_GREEN "   * Boundary conditions: ${BOUNDARY_TYPE}")
message(STATUS "     The type of boundary conditions can be set with '-DBOUNDARY_TYPE={BOXED,PERIODIC}'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_BOUNDARY_TYPE=${BOUNDARY_TYPE}")

if (NOT DEFINED AMR_ROOT_LEVEL)
  SET(AMR_ROOT_LEVEL "6")
endif()
cmessage(STATUS_GREEN "   * AMR grid root voxels level: ${AMR_ROOT_LEVEL}")
message(STATUS "     The level of the root voxels for AMR grid can be defined with '-DAMR_ROOT_LEVEL=N'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_AMR_ROOT_LEVEL=${AMR_ROOT_LEVEL}")

if (NOT DEFINED ENABLE_ACCURACY_CHECKING)
  SET(ENABLE_ACCURACY_CHECKING "false")
endif()
cmessage(STATUS_GREEN "   * Accuracy checking for projection: ${ENABLE_ACCURACY_CHECKING}")
message(STATUS "     Accuracy checking can be defined with '-DENABLE_ACCURACY_CHECKING=true/false'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_ENABLE_ACCURACY_CHECKING=${ENABLE_ACCURACY_CHECKING}")

if (NOT DEFINED DUMP_TRACERS)
  SET(DUMP_TRACERS "true")
endif()
cmessage(STATUS_GREEN "   * Enable dumping tracers coordinates: ${DUMP_TRACERS}")
message(STATUS "     Enable dumping tracers coordinates with '-DDUMP_TRACERS=true/false'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_DUMP_TRACERS=${DUMP_TRACERS}")

if (NOT DEFINED PER_SIMPLEX_INVARIANT)
  SET(PER_SIMPLEX_INVARIANT "false")
endif()
cmessage(STATUS_GREEN "   * Enable using per-simplex invariant threshold: ${PER_SIMPLEX_INVARIANT}")
message(STATUS "     Enable using per-simplex invariant threshold with '-DPER_SIMPLEX_INVARIANT=true/false'")
SET(SOLVER_COMPILE_PROPERTIES "${SOLVER_COMPILE_PROPERTIES};D_PER_SIMPLEX_INVARIANT=${PER_SIMPLEX_INVARIANT}")

message (STATUS "")
cmessage (STATUS_CYAN "---------------------------------------")

SET(MAIN_BINARY_NAME_SUFFIX "")
if (BOUNDARY_TYPE MATCHES "PERIODIC")
SET(MAIN_BINARY_NAME_SUFFIX "p")
endif()

SET(MAIN_BINARY_NAME_DEPENDENCE "")
if (NDIMS MATCHES "2" OR NDIMS MATCHES "2+3")
  add_executable("${MAIN_BINARY_NAME}_2D${MAIN_BINARY_NAME_SUFFIX}" ${MAIN_BINARY_SRC})
  set_target_properties("${MAIN_BINARY_NAME}_2D${MAIN_BINARY_NAME_SUFFIX}" 
    PROPERTIES COMPILE_DEFINITIONS "D_DIMS_COUNT=2;${SOLVER_COMPILE_PROPERTIES}")
  SET(MAIN_BINARY_NAME_DEPENDENCE "${MAIN_BINARY_NAME}_2D${MAIN_BINARY_NAME_SUFFIX}")
endif()

if (NDIMS MATCHES "3" OR NDIMS MATCHES "2+3")
  add_executable("${MAIN_BINARY_NAME}_3D${MAIN_BINARY_NAME_SUFFIX}" ${MAIN_BINARY_SRC})
  set_target_properties("${MAIN_BINARY_NAME}_3D${MAIN_BINARY_NAME_SUFFIX}" 
    PROPERTIES COMPILE_DEFINITIONS "D_DIMS_COUNT=3;${SOLVER_COMPILE_PROPERTIES}")
  SET(MAIN_BINARY_NAME_DEPENDENCE "${MAIN_BINARY_NAME_DEPENDENCE}" "${MAIN_BINARY_NAME}_3D${MAIN_BINARY_NAME_SUFFIX}")
endif()

add_custom_target(${MAIN_BINARY_NAME} 
  DEPENDS "${MAIN_BINARY_NAME_DEPENDENCE}")
set(ALL_TARGETS ${MAIN_BINARY_NAME_DEPENDENCE})

install (TARGETS ${ALL_TARGETS} DESTINATION bin)

SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_subdirectory ("post")
add_subdirectory ("static")

SET(TARGETS_LIST_LOC ${TARGETS_LIST_LOC} ${ALL_TARGETS})

SET(TARGETS_LIST ${TARGETS_LIST} ${TARGETS_LIST_LOC} PARENT_SCOPE)
SET(NOT_BUILT_TARGETS_LIST ${NOT_BUILT_TARGETS_LIST} ${NOT_BUILT_TARGETS_LIST_LOC} PARENT_SCOPE)
SET(DISABLED_FEATURES_LIST ${DISABLED_FEATURES_LIST} ${DISABLED_FEATURES_LIST_LOC} PARENT_SCOPE)

