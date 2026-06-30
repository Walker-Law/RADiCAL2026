cmake_minimum_required(VERSION 3.16)
project(radical)

find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(radical radical.cc ${sources} ${headers})
target_link_libraries(radical ${Geant4_LIBRARIES})

# Copy macro files to build directory so ./radical works without arguments
file(GLOB macros ${PROJECT_SOURCE_DIR}/*.mac)
foreach(macro ${macros})
    configure_file(${macro} ${PROJECT_BINARY_DIR} COPYONLY)
endforeach()
