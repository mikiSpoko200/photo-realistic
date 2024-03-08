@echo off
setlocal

CALL env.bat

SET "ASSEMBLY=ray-tracer-baby"

ECHO Building %ASSEMBLY%

SET C_FILE_NAMES=

SetLocal EnableDelayedExpansion
FOR /R %%f IN (*.c) DO (
    SET C_FILE_NAMES=!C_FILE_NAMES! %%f
)


REM Check if "--release" argument was passed
set IS_RELEASE=0
FOR %%i IN (%*) DO (
    IF /I "%%i"=="--release" (
        SET IS_RELEASE=1
    )
)

SET LIBS=embree3.lib glfw3dll.lib OpenGL32.lib glew32.lib cglm.lib obj.dll.lib

SET DEFINES=/D_CRT_SECURE_NO_WARNINGS
REM -fsycl -fsycl-allow-func-ptr -fsycl-targets=spir64  requires compile as c
SET COMPILER_FLAGS=-Qstd=c11 -Wunused-variable -Wall -Xclang
SET "INCLUDE=.\include;%INCLUDE%"

SET "BIN_PATH=.\bin"
SET "TARGET_PATH=.\target"

IF IS_RELEASE == 1 (
    ECHO Running Release build ...

    SET COMPILER_FLAGS=-O2 %COMPILER_FLAGS%
    SET BIN_PATH=%BIN_PATH%
    SET TARGET_PATH=%TARGET_PATH%\release

    icx main.cpp -o %TARGET_PATH%\%ASSEMBLY%.exe %DEFINES% %LIBS%
) ELSE (
    ECHO Running Debug build ...    

    SET COMPILER_FLAGS=!COMPILER_FLAGS!

    SET "BIN_PATH=!BIN_PATH!"
    SET "TARGET_PATH=!TARGET_PATH!\debug"
    SET "DEFINES=/D_DEBUG /D_LOGGING /DSTB_IMAGE_WRITE_IMPLEMENTATION !DEFINES!"
    
    SET "LIB=.\lib;!LIB!"

    icx /Fe"!TARGET_PATH!\%ASSEMBLY%" !DEFINES! !COMPILER_FLAGS! ./main.c !C_FILE_NAMES! %LIBS%
)
