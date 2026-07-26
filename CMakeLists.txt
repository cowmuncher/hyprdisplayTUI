cmake_minimum_required(VERSION 3.10)
project(hyprdisplayTUI CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(Curses REQUIRED)

add_executable(hyprdisplayTUI src/main.cpp)
target_include_directories(hyprdisplayTUI PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(hyprdisplayTUI PRIVATE nlohmann_json::nlohmann_json ${CURSES_LIBRARIES})
