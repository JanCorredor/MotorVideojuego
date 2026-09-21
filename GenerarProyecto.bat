@echo off
title Configuracion del Motor
echo Configurando el motor de videojuegos con CMake y Vcpkg...
cmake --preset default
if %errorlevel% neq 0 (
    echo.
    echo Error: No se pudo generar el proyecto. Revisa los logs de CMake.
    pause
    exit /b %errorlevel%
)
echo.
echo ¡Proyecto generado con exito en la carpeta build!
pause