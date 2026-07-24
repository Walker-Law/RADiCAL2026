cmake_minimum_required(VERSION 3.16)
project(radsimple)

find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(radsimple radsimple.cc ${sources} ${headers})
target_link_libraries(radsimple ${Geant4_LIBRARIES})

# Copy macros next to the binary so `./radsimple run.mac` works from build/.
file(GLOB macros ${PROJECT_SOURCE_DIR}/*.mac)
foreach(macro ${macros})
    configure_file(${macro} ${PROJECT_BINARY_DIR} COPYONLY)
endforeach()
