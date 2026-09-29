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
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(DevIL REQUIRED)
find_package(assimp CONFIG REQUIRED)


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
)