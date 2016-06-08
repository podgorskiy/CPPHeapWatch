cmake_minimum_required (VERSION 2.8)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CPPWATCH_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/CPPHeapWatch.h PARENT_SCOPE)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(CPPHeapWatch STATIC ${SOURCES})
