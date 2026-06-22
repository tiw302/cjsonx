@echo off
setlocal EnableDelayedExpansion

:: -----------------------------------------------------------------------------
:: build.bat - Windows Build script for cjsonx engine
:: Optimized with parallel builds and consistent naming
:: -----------------------------------------------------------------------------

:: Check if command line arguments are provided
if not "%~1"=="" (
    if /i "%~1"=="lib" goto build_lib
    if /i "%~1"=="examples" goto build_examples
    if /i "%~1"=="test" goto run_tests
    if /i "%~1"=="bench" goto run_benchmarks
    if /i "%~1"=="all" goto build_all
    if /i "%~1"=="clean" goto clean
    echo error: unknown option '%~1'. usage: %0 {lib^|examples^|test^|bench^|all^|clean}
    exit /b 1
)

:menu
echo ====================================================================================
echo cjsonx engine build!! (Windows)
echo ====================================================================================
echo   1^) build library
echo   2^) build examples
echo   3^) run tests
echo   4^) run benchmarks
echo   5^) build all
echo   6^) clean
echo   q^) quit
echo ====================================================================================
echo.
set /p choice=">> "

if /i "%choice%"=="1" goto build_lib
if /i "%choice%"=="2" goto build_examples
if /i "%choice%"=="3" goto run_tests
if /i "%choice%"=="4" goto run_benchmarks
if /i "%choice%"=="5" goto build_all
if /i "%choice%"=="6" goto clean
if /i "%choice%"=="q" exit /b 0

echo error: invalid choice '%choice%'. please enter a number between 1-6, or 'q' to quit.
echo.
goto menu

:build_lib
echo.
echo Configuring library build...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building library...
cmake --build build --target cjsonx --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! library is in build\
echo ====================================================================================
echo.
goto end

:build_examples
echo.
echo Configuring examples build...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building examples...
cmake --build build --target example_dom_access example_float128_precision example_error_handling example_simple_parse example_embedded_noalloc example_builder_api --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! to run examples:
echo   * .\build\Release\example_dom_access.exe
echo   * .\build\Release\example_float128_precision.exe
echo   * .\build\Release\example_error_handling.exe
echo   * .\build\Release\example_simple_parse.exe
echo   * .\build\Release\example_embedded_noalloc.exe
echo   * .\build\Release\example_builder_api.exe
echo ====================================================================================
echo.
goto end

:run_tests
echo.
echo Configuring tests...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building tests...
cmake --build build --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Running tests...
ctest --test-dir build -C Release --output-on-failure
echo ====================================================================================
echo  tests complete!
echo ====================================================================================
echo.
goto end

:run_benchmarks
echo.
echo Configuring benchmarks...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building benchmarks...
cmake --build build --target bench_compare --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Running benchmarks...
set BENCH_EXE=.\build\Release\bench_compare.exe
if not exist "%BENCH_EXE%" set BENCH_EXE=.\build\bench_compare.exe

if exist "%BENCH_EXE%" (
    if exist "benchmarks\datasets\twitter.json" (
        "%BENCH_EXE%" benchmarks\datasets\twitter.json
    ) else (
        echo note: twitter.json not found in benchmarks\datasets\
        echo you can download datasets using the scripts in the benchmarks folder.
    )
    
    if exist "benchmarks\datasets\canada.json" (
        "%BENCH_EXE%" benchmarks\datasets\canada.json
    )
) else (
    echo error: benchmark executable not built.
    echo make sure third_party dependencies are downloaded ^(see benchmarks\ folder^).
)
echo.
echo ====================================================================================
echo  benchmarks complete!
echo ====================================================================================
echo.
goto end

:build_all
call :build_lib
call :build_examples
call :run_tests
call :run_benchmarks
goto end

:clean
echo.
echo Cleaning build directory...
if exist build rmdir /s /q build
echo clean complete!
echo.
goto end

:end
if "%~1"=="" goto menu
exit /b 0
