@echo off
setlocal enabledelayedexpansion
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
set C3=C:\Users\vi\Desktop\c3.vb6.pro\.build\C3.exe
set OUT=C:\Users\vi\Desktop\c3.vb6.pro\publish\demos\out

call "%VCVARS%" x64 >nul 2>&1
if errorlevel 1 ( echo [ERROR] vcvarsall failed & exit /b 1 )
if not exist "%OUT%" mkdir "%OUT%"

cd /d "%~dp0"

echo ========================================
echo [1/8] hello.bas
echo ========================================
"%C3%" hello\hello.bas -o "%OUT%\hello.exe"
echo EXIT: !errorlevel!

echo ========================================
echo [2/8] activex_dll test_activex_dll.vbp
echo ========================================
"%C3%" --dll --arch x64 -o "%OUT%\test_activex_dll_x64.dll" activex_dll\test_activex_dll.vbp
echo EXIT: !errorlevel!

echo ========================================
echo [3/8] com_client.bas
echo ========================================
"%C3%" com_client\com_client.bas -o "%OUT%\com_client.exe"
echo EXIT: !errorlevel!

echo ========================================
echo [4/8] form 工程1.vbp
echo ========================================
"%C3%" form\工程1.vbp --output-dir "%OUT%\form"
echo EXIT: !errorlevel!

echo ========================================
echo [5/8] frxParse 工程1.vbp
echo ========================================
"%C3%" frxParse\工程1.vbp --output-dir "%OUT%\frxParse"
echo EXIT: !errorlevel!

echo ========================================
echo [6/8] multimodule TestVBP.vbp
echo ========================================
"%C3%" multimodule\TestVBP.vbp --output-dir "%OUT%\multimodule"
echo EXIT: !errorlevel!

echo ========================================
echo [7/8] realVBP 工程1.vbp
echo ========================================
"%C3%" realVBP\工程1.vbp --output-dir "%OUT%\realVBP"
echo EXIT: !errorlevel!

echo ========================================
echo [8/8] win32_form 工程1.vbp
echo ========================================
"%C3%" win32_form\工程1.vbp --output-dir "%OUT%\win32_form"
echo EXIT: !errorlevel!

echo.
echo ========================================
echo ALL DEMOS COMPILED
echo ========================================
