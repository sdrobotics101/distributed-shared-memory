#CMakeLists.txt file for use with the Google Test framework
#Place project headers in '/include', sources in '/src', and test sources in '/test'
#Build and run tests with 'make check'

cmake_minimum_required(VERSION 2.8.9)
set(PROJECT_NAME_STR DSM-Controller)
project(${PROJECT_NAME_STR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -std=c++11")
set(CMAKE_MACOSX_RPATH 1)

find_package(Boost REQUIRED system thread log python)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
add_definitions(-DBOOST_LOG_DYN_LINK)

find_package(PythonLibs 3.5 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# shared sources
file(GLOB SHARED_SOURCES "${PROJECT_SOURCE_DIR}/src/Shared/*.cpp")

# server sources
file(GLOB SERVER_SOURCES "${PROJECT_SOURCE_DIR}/src/Server/*.cpp")

# client sources
file(GLOB CLIENT_SOURCES "${PROJECT_SOURCE_DIR}/src/Client/*.cpp")

# server test sources
file(GLOB SERVER_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*server*.cpp")

# client test sources
file(GLOB CLIENT0_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client0*.cpp")
file(GLOB CLIENT1_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client1*.cpp")

# server test target
set(PROJECT_SERVER_TEST_NAME ${PROJECT_NAME_STR}_server_test)
add_executable(${PROJECT_SERVER_TEST_NAME} ${SHARED_SOURCES} ${SERVER_SOURCES} ${SERVER_TEST_SOURCES})
target_link_libraries(${PROJECT_SERVER_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

# client0 test target
set(PROJECT_CLIENT0_TEST_NAME ${PROJECT_NAME_STR}_client0_test)
add_executable(${PROJECT_CLIENT0_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT0_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT0_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

# client1 test target
set(PROJECT_CLIENT1_TEST_NAME ${PROJECT_NAME_STR}_client1_test)
add_executable(${PROJECT_CLIENT1_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT1_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT1_TEST_NAME} ${Boost_LIBRARIES})
# target_compile_definitions(${PROJECT_TEST_NAME})

set(PROJECT_PYTHON_MODULE_NAME dsmclient)
add_library(${PROJECT_PYTHON_MODULE_NAME} MODULE ${SHARED_SOURCES} ${CLIENT_SOURCES})
target_compile_definitions(${PROJECT_PYTHON_MODULE_NAME} PUBLIC BUILD_PYTHON_MODULE)
target_link_libraries(${PROJECT_PYTHON_MODULE_NAME} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})
set_target_properties(${PROJECT_PYTHON_MODULE_NAME} PROPERTIES PREFIX "" SUFFIX ".so")
