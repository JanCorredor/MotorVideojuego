@echo off
title Creating Build
echo Creating build with CMake & Vcpkg...
cmake --preset default
if %errorlevel% neq 0 (
    echo.
    echo Error: Couldn't build the project. Check CMake's logs.
    pause
    exit /b %errorlevel%
)
echo.
echo ¡Project Built Successfully!
pause