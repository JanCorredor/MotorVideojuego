cmake_minimum_required(VERSION 3.10)

project(MotorVideojuego)

find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(MotorVideojuego 
    Source/main.cpp
    Source/Audio.cpp
    Source/Audio.h
    Source/Engine.cpp
    Source/Engine.h
    Source/Log.cpp
    Source/Log.h
    Source/Module.h
    Source/PerfTimer.cpp
    Source/PerfTimer.h
    Source/Timer.cpp
    Source/Timer.h
    Source/Window.cpp
    Source/Window.h
)

target_link_libraries(MotorVideojuego PRIVATE SDL3::SDL3)
target_link_libraries(MotorVideojuego PRIVATE glad::glad)