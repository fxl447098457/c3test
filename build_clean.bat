@echo off
setlocal
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
set PROJECT=D:\vb6pro

echo [1/3] Stripping zero-width chars from CMakeLists.txt...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT%\.temp\strip_zw.ps1" -Files "%PROJECT%\CMakeLists.txt"

echo [2/3] Loading MSVC environment...
call "%VCVARS%" x64 >nul 2>&1

cd /d %PROJECT%

if "%1"=="full" (
    echo [3/3] Full rebuild...
    if exist ".build" rmdir /s /q ".build"
    cmake -G Ninja -B .build -DCMAKE_BUILD_TYPE=Release
    cmake --build .build --config Release
    goto :done
)

if "%1"=="configure" (
    echo [3/3] Reconfiguring...
    cmake -G Ninja -B .build -DCMAKE_BUILD_TYPE=Release
    goto :done
)

echo [3/3] Building...
cmake --build .build --config Release

:done
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
echo.
echo === Build complete ===
echo c3.exe: %PROJECT%\.build\c3.exe
endlocal