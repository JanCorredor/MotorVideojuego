cmake_minimum_required(VERSION 3.14)

# Dynamically download vcpkg if toolchain file is not defined/found
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(FetchContent)
    FetchContent_Declare(
        vcpkg
        GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
        GIT_TAG        a1cae005c39be7b18ba319fced856b68d7276271
    )
    FetchContent_MakeAvailable(vcpkg)
    set(CMAKE_TOOLCHAIN_FILE "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

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

target_link_libraries(MotorVideojuego PRIVATE SDL3::SDL3 glad::glad)