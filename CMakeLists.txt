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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(DevIL REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

# imnodes (no tiene port en vcpkg)
include(FetchContent)
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG        2d0c280b07b2f62b120034811cc6e41251673961
    SOURCE_SUBDIR  _no_cmake_
)
FetchContent_MakeAvailable(imnodes)
add_library(imnodes STATIC ${imnodes_SOURCE_DIR}/imnodes.cpp)
target_include_directories(imnodes PUBLIC ${imnodes_SOURCE_DIR})
target_link_libraries(imnodes PUBLIC imgui::imgui)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "Source/*.cpp"
    "Source/*.h"
    "Source/*.hpp"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${SOURCES})

add_executable(MotorVideojuego ${SOURCES})


target_link_libraries(MotorVideojuego PRIVATE 
SDL3::SDL3 
glad::glad 
glm::glm 
imgui::imgui
DevIL::ILU
DevIL::ILUT
assimp::assimp
Tracy::TracyClient
imguizmo::imguizmo
imnodes
)

# Copy required DLLs (TracyClient.dll, SDL3.dll, assimp.dll, etc.) to executable directory
if(WIN32)
    add_custom_command(TARGET MotorVideojuego POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:MotorVideojuego>
            $<TARGET_FILE_DIR:MotorVideojuego>
        COMMAND_EXPAND_LISTS
    )

    # Asegura la ruta para la depuración en Visual Studio
    set_target_properties(MotorVideojuego PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:MotorVideojuego>;%PATH%"
    )
endif()