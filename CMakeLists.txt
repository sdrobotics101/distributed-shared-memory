cmake_minimum_required(VERSION 2.8.9)
set(PROJECT_NAME_STR DSM-Controller)
project(${PROJECT_NAME_STR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -std=c++11")
set(CMAKE_MACOSX_RPATH 1)

find_package(Boost COMPONENTS system thread log python program_options REQUIRED)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

find_package(PythonLibs 3.5 REQUIRED)

# shared sources
file(GLOB SHARED_SOURCES "${PROJECT_SOURCE_DIR}/src/Shared/*.cpp")

# server sources
file(GLOB SERVER_SOURCES "${PROJECT_SOURCE_DIR}/src/Server/*.cpp")

# client sources
file(GLOB CLIENT_SOURCES "${PROJECT_SOURCE_DIR}/src/Client/*.cpp")

# client test sources
file(GLOB CLIENT0_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client0*.cpp")
file(GLOB CLIENT1_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*client1*.cpp")

# server target
set(PROJECT_SERVER_NAME DSMServer)
add_executable(${PROJECT_SERVER_NAME} ${SHARED_SOURCES} ${SERVER_SOURCES})
target_compile_definitions(${PROJECT_SERVER_NAME} PUBLIC BOOST_LOG_DYN_LINK)
target_link_libraries(${PROJECT_SERVER_NAME} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})

# client0 test target
set(PROJECT_CLIENT0_TEST_NAME ${PROJECT_NAME_STR}_client0_test)
add_executable(${PROJECT_CLIENT0_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT0_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT0_TEST_NAME} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})

# client1 test target
set(PROJECT_CLIENT1_TEST_NAME ${PROJECT_NAME_STR}_client1_test)
add_executable(${PROJECT_CLIENT1_TEST_NAME} ${SHARED_SOURCES} ${CLIENT_SOURCES} ${CLIENT1_TEST_SOURCES})
target_link_libraries(${PROJECT_CLIENT1_TEST_NAME} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})

set(PROJECT_PYTHON_MODULE_NAME pydsm)
add_library(${PROJECT_PYTHON_MODULE_NAME} MODULE ${SHARED_SOURCES} ${CLIENT_SOURCES})
target_compile_definitions(${PROJECT_PYTHON_MODULE_NAME} PUBLIC BUILD_PYTHON_MODULE)
target_include_directories(${PROJECT_PYTHON_MODULE_NAME} PUBLIC ${PYTHON_INCLUDE_DIRS})
target_link_libraries(${PROJECT_PYTHON_MODULE_NAME} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_PYTHON_LIBRARY} ${PYTHON_LIBRARIES})
set_target_properties(${PROJECT_PYTHON_MODULE_NAME} PROPERTIES PREFIX "" SUFFIX ".so")
