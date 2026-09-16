@echo off
setlocal
cd /d "%~dp0"

set BUILD_DIR=build

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found in PATH.
    exit /b 1
)

where python >nul 2>nul
if errorlevel 1 (
    echo [ERROR] python was not found in PATH. Required by Kadoka rule checker.
    exit /b 1
)

cmake -S . -B %BUILD_DIR% -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1

cmake --build %BUILD_DIR% --config Release
if errorlevel 1 exit /b 1

ctest --test-dir %BUILD_DIR% -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Kadoka Shougi build and tests completed successfully.
endlocal
