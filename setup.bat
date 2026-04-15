@echo off
setlocal enabledelayedexpansion
title ChessMind Setup

echo.
echo  ===================================
echo   ChessMind - Chess Engine Setup
echo  ===================================
echo.

set SCRIPT_DIR=%~dp0
set ENGINE_DIR=%SCRIPT_DIR%engine
set BRIDGE_DIR=%SCRIPT_DIR%bridge

:: Check g++
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] g++ not found.
    echo Install MinGW from https://winlibs.com/ and add to PATH
    echo OR use WSL2 and run setup.sh instead
    pause & exit /b 1
)
echo [OK] g++ found

:: Check Python
where python >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found. Install from python.org
    pause & exit /b 1
)
echo [OK] Python found

:: Install Python packages
echo Installing Python packages...
python -m pip install fastapi uvicorn python-chess websockets --quiet
echo [OK] Python packages installed

:: Compile engine
echo.
echo Compiling C++ engine...
cd /d "%ENGINE_DIR%"
g++ -std=c++17 -O2 -o chessmind.exe main.cpp board.cpp movegen.cpp eval.cpp search.cpp
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed
    pause & exit /b 1
)
echo [OK] Engine compiled

:: Copy to bridge
copy /Y chessmind.exe "%BRIDGE_DIR%\chessmind.exe" >nul
echo [OK] Engine copied to bridge

:: Find stockfish
set STOCKFISH=stockfish
where stockfish >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Stockfish found
) else (
    echo [WARN] Stockfish not found - hints disabled
    echo Download from https://stockfishchess.org/download/
    set STOCKFISH=
)

:: Write env file
echo CHESSMIND_BIN=%BRIDGE_DIR%\chessmind.exe> "%BRIDGE_DIR%\.env"
echo STOCKFISH_PATH=%STOCKFISH%>> "%BRIDGE_DIR%\.env"

echo.
echo  ===================================
echo   Setup complete!
echo  ===================================
echo.
echo  Run the game: double-click run.bat
echo.
pause
