# Simple CMake Example
# This shows how to create your own CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# Name your project
project(SimpleExample)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create an executable from main.cpp
add_executable(simple_example main.cpp)

# Add compiler warnings (helpful for learning)
if(MSVC)
    target_compile_options(simple_example PRIVATE /W4)
else()
    target_compile_options(simple_example PRIVATE -Wall -Wextra)
endif()

