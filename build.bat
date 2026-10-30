@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist logs mkdir logs
set "LOG=logs\build.log"
where cmake >nul 2>&1
if errorlevel 1 (echo CMake was not found in PATH.&pause&exit /b 1)
where g++ >nul 2>&1
if errorlevel 1 goto msvc
echo [1/3] Configuring MinGW...
cmake -S . -B build -G "MinGW Makefiles" -DRISHA_BUILD_TESTS=ON >"%LOG%" 2>&1
if errorlevel 1 goto failed
goto build
:msvc
echo [1/3] Configuring Visual Studio...
cmake -S . -B build -DRISHA_BUILD_TESTS=ON >"%LOG%" 2>&1
if errorlevel 1 goto failed
:build
echo [2/3] Building...
cmake --build build --config Release --parallel >>"%LOG%" 2>&1
if errorlevel 1 goto failed
echo [3/3] Testing...
ctest --test-dir build -C Release --output-on-failure >>"%LOG%" 2>&1
if errorlevel 1 goto failed
echo Build and tests OK.
if exist build\risha_editor.exe (start "" build\risha_editor.exe) else if exist build\Release\risha_editor.exe (start "" build\Release\risha_editor.exe)
exit /b 0
:failed
echo BUILD FAILED. Log: %LOG%
powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG%' -Tail 60"
pause
exit /b 1
