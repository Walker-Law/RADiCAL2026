cmake_minimum_required(VERSION 3.16)
project(H3generation)

find_package(Geant4 REQUIRED ui_all vis_all)
include(${Geant4_USE_FILE})

find_package(ROOT REQUIRED COMPONENTS RIO Tree Hist)
include(${ROOT_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(H3generation H3generation.cc ${sources} ${headers})
target_link_libraries(H3generation ${Geant4_LIBRARIES} ROOT::RIO ROOT::Tree ROOT::Hist)

configure_file(vis.mac vis.mac COPYONLY)
configure_file(run.mac run.mac COPYONLY)
