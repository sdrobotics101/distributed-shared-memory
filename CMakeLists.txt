#CMakeLists.txt file for use with the Google Test framework
#Place project headers in '/include', sources in '/src', and test sources in '/test'
#Build and run tests with 'make check'

cmake_minimum_required(VERSION 2.8.9)
set(PROJECT_NAME_STR DSM-Controller)
project(${PROJECT_NAME_STR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -std=c++11")

find_package(Boost REQUIRED system thread log)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
add_definitions(-DBOOST_LOG_DYN_LINK)

# shared sources
include_directories("${PROJECT_SOURCE_DIR}/src/Shared")
file(GLOB SHARED_SOURCES "${PROJECT_SOURCE_DIR}/src/Shared/*.cpp")

# server sources
include_directories("${PROJECT_SOURCE_DIR}/src/Server")
file(GLOB SERVER_SOURCES "${PROJECT_SOURCE_DIR}/src/Server/*.cpp")

# client sources
include_directories("${PROJECT_SOURCE_DIR}/src/Client")
file(GLOB CLIENT_SOURCES "${PROJECT_SOURCE_DIR}/src/Client/*.cpp")

# server test sources
set(PROJECT_SERVER_TEST_NAME ${PROJECT_NAME_STR}_server_test)
file(GLOB SERVER_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*server*.cpp")

# client test sources
set(PROJECT_CLIENT0_TEST_NAME ${PROJECT_NAME_STR}_client0_test)
file(GLOB CLIENT0_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client0*.cpp")

set(PROJECT_CLIENT1_TEST_NAME ${PROJECT_NAME_STR}_client1_test)
file(GLOB CLIENT1_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client1*.cpp")

# server0 test target
add_executable(${PROJECT_SERVER_TEST_NAME} ${SHARED_SOURCES} ${SERVER_SOURCES} ${SERVER_TEST_SOURCES})
target_link_libraries(${PROJECT_SERVER_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

# client0 test target
add_executable(${PROJECT_CLIENT0_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT0_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT0_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

# client1 test target
add_executable(${PROJECT_CLIENT1_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT1_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT1_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})
