cmake_minimum_required(VERSION 3.16)
project(logos-native-generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(logos-native-generator main.cpp)
