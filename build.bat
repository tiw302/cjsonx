@echo off
rem cjsonx helper build script for windows
setlocal enabledelayedexpansion

rem define paths based on the script's location
set SCRIPT_DIR=%~dp0
rem remove trailing slash from SCRIPT_DIR
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set BUILD_DIR=%SCRIPT_DIR%\build
set RUN_TESTS=0
set CLEAN_FIRST=0

rem parse arguments
:loop
if "%~1" == "" goto endloop
if "%~1" == "--test" (
    set RUN_TESTS=1
)
if "%~1" == "--clean" (
    set CLEAN_FIRST=1
)
shift
goto loop
:endloop

rem handle clean build option
if %CLEAN_FIRST% == 1 (
    if exist "%BUILD_DIR%" (
        echo cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

rem create build directory if it doesn't exist
if not exist "%BUILD_DIR%" (
    echo creating build directory: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
)

rem run cmake configuration
echo configuring build with cmake...
cmake -B "%BUILD_DIR%" -S "%SCRIPT_DIR%" -DCMAKE_BUILD_TYPE=Release

rem build targets
echo building cjsonx target binaries...
cmake --build "%BUILD_DIR%" --config Release

rem run test suite if requested using --test-dir
if %RUN_TESTS% == 1 (
    echo running cjsonx unit tests...
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
)

echo build complete!
