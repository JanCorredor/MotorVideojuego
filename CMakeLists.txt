cmake_minimum_required(VERSION 3.10)

project(MotorVideojuego)

find_package(SDL3 CONFIG REQUIRED)

add_executable(MotorVideojuego main.cpp)

target_link_libraries(MotorVideojuego PRIVATE SDL3::SDL3)