@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title Metro Simulation - Build & Deploy

echo ========================================
echo   Metro Simulation - One-Click Build
echo ========================================
echo.

REM ----- 配置路径（如 Qt 安装在其他位置请修改下面三行）-----
set "QT_DIR=D:\yingyong\qt\6.10.3\msvc2022_64"
set "CMAKE_DIR=D:\yingyong\qt\Tools\CMake_64\bin"
set "VS_DIR=D:\Program Files\Microsoft Visual Studio\2022\Community"
REM ----------------------------------------------------------

set "PROJECT_DIR=%~dp0metro-simulation"
set "BUILD_DIR=%PROJECT_DIR%\build_release"
set "OUTPUT_DIR=%PROJECT_DIR%\output"

REM Step 1: Check Qt
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [ERROR] Qt not found at %QT_DIR%
    echo Please edit this file and set QT_DIR to your Qt installation path.
    pause
    exit /b 1
)

REM Step 2: Setup MSVC
call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MSVC not found. Please install Visual Studio 2022 Community.
    pause
    exit /b 1
)
echo [OK] MSVC environment ready

REM Step 3: Clean old build
if exist "%BUILD_DIR%" (
    echo [INFO] Cleaning old build...
    rmdir /s /q "%BUILD_DIR%"
)

REM Step 4: CMake Configure
echo [INFO] Configuring CMake...
"%CMAKE_DIR%\cmake.exe" -G "Visual Studio 17 2022" -A x64 ^
    -S "%PROJECT_DIR%" ^
    -B "%BUILD_DIR%" ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)
echo [OK] CMake configured

REM Step 5: Build
echo [INFO] Building Release (this may take a few minutes)...
"%CMAKE_DIR%\cmake.exe" --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed! Check the error messages above.
    pause
    exit /b 1
)
echo [OK] Build complete!

REM Step 6: Create output directory
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"

copy "%BUILD_DIR%\bin\Release\metro_sim.exe" "%OUTPUT_DIR%\" >nul
copy "%BUILD_DIR%\bin\Release\QtNodes.dll" "%OUTPUT_DIR%\" >nul

REM Step 7: windeployqt
echo [INFO] Running windeployqt (collecting Qt DLLs)...
"%QT_DIR%\bin\windeployqt.exe" "%OUTPUT_DIR%\metro_sim.exe" --no-translations
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] windeployqt reported issues.
)

REM Step 8: Copy data and resources
echo [INFO] Copying data and resources...
xcopy "%PROJECT_DIR%\data" "%OUTPUT_DIR%\data\" /E /I /Y >nul
xcopy "%PROJECT_DIR%\resources" "%OUTPUT_DIR%\resources\" /E /I /Y >nul
mkdir "%OUTPUT_DIR%\data\results" 2>nul

REM Step 9: Clean build directory (optional)
choice /C YN /M "Delete build directory to save space"
if errorlevel 2 goto :keep_build
if errorlevel 1 rmdir /s /q "%BUILD_DIR%"
:keep_build

echo.
echo ========================================
echo   BUILD SUCCESS!
echo.
echo   Output folder: %OUTPUT_DIR%
echo   Run: %OUTPUT_DIR%\metro_sim.exe
echo.
echo   To distribute: zip the "output" folder
echo   and send it to anyone (no Qt required).
echo ========================================
pause
endlocal
