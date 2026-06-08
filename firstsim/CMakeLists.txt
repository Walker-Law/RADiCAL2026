cmake_minimum_required(VERSION 3.16)
project(firstsim)

find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(firstsim firstsim.cc ${sources} ${headers})
target_link_libraries(firstsim ${Geant4_LIBRARIES})
