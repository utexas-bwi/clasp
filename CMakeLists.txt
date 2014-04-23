cmake_minimum_required(VERSION 2.8.3)

project(clasp)

find_package(catkin REQUIRED COMPONENTS cmake_modules)
find_package(TBB REQUIRED)

catkin_package()

message(STATUS "clasp: Found TBB_LIBRARIES: ${TBB_LIBRARIES}")
add_definitions(-DWITH_THREADS=1)
add_definitions(-DNDEBUG)
add_definitions(-O3)

## libraries
foreach(lib clasp program_opts)
  include_directories(lib${lib})
  file(GLOB lib_src lib${lib}/src/*.cpp)
  add_library(${lib} ${lib_src})
endforeach()

## binaries
file(GLOB APP_SRC app/*.cpp)
add_executable(clasp_app ${APP_SRC})
target_link_libraries(clasp_app 
  clasp 
  program_opts
  rt
  ${TBB_LIBRARIES}
)
set_target_properties(clasp_app PROPERTIES OUTPUT_NAME clasp)

## install targets
install(TARGETS clasp program_opts clasp_app
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## install headers
foreach(lib clasp program_opts)
  install(DIRECTORY lib${lib}/${lib}/
    DESTINATION ${CATKIN_GLOBAL_INCLUDE_DESTINATION}/${lib}
  )
endforeach()
