#CMakeLists.txt file for use with the Google Test framework
#Place project headers in '/include', sources in '/src', and test sources in '/test'
#Build and run tests with 'make check'

cmake_minimum_required(VERSION 2.8.9)
set(PROJECT_NAME_STR DSM-Controller)
project(${PROJECT_NAME_STR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -std=c++11")

include_directories("${PROJECT_SOURCE_DIR}/include")
file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

find_package(GTest REQUIRED)
include_directories("${GTEST_INCLUDE_DIR}")

set(PROJECT_TEST_NAME ${PROJECT_NAME_STR}_test)
file(GLOB TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*.cpp")

add_executable(${PROJECT_TEST_NAME} ${SOURCES} ${TEST_SOURCES})
target_link_libraries(${PROJECT_TEST_NAME} ${GTEST_BOTH_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

enable_testing()
add_test(test "./${PROJECT_TEST_NAME}")
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS ${PROJECT_TEST_NAME})
