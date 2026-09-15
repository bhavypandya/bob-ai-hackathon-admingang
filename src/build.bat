@echo off
REM Build script for Supply Chain Disruption Assistant (Windows with MinGW)
REM Must be run from the src\ directory or as: src\build.bat

REM Change to the directory containing this script
cd /d "%~dp0"

echo === Supply Chain Assistant - Build Script (Windows) ===
echo Working directory: %CD%
echo.

REM Check for g++
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: g++ not found. Install MinGW from https://www.mingw-w64.org/
    echo Or use MSYS2: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
    pause
    exit /b 1
)

REM Create output directories
if not exist "build\bin" mkdir "build\bin"
if not exist "build\obj" mkdir "build\obj"

echo Compiling SQLite3 (bundled)...
gcc -c third_party\sqlite\sqlite3.c -o build\obj\sqlite3.o -O2 -w

echo Compiling database layer...
g++ -c backend\database\database.cpp -o build\obj\database.o ^
    -std=c++17 -O2 -Ibackend -Ibackend\models -Ithird_party -Ithird_party\sqlite

echo Compiling services...
for %%F in (disruption_service shipment_service route_service carrier_service fleet_service cold_chain_service recommendation_service simulation_service) do (
    echo   %%F
    g++ -c backend\services\%%F.cpp -o build\obj\%%F.o ^
        -std=c++17 -O2 -Ibackend -Ibackend\models -Ibackend\services -Ibackend\database ^
        -Ithird_party -Ithird_party\sqlite
)

echo Compiling API handler...
g++ -c backend\api\api_handler.cpp -o build\obj\api_handler.o ^
    -std=c++17 -O2 -Ibackend -Ibackend\models -Ibackend\services -Ibackend\database ^
    -Ibackend\api -Ithird_party -Ithird_party\sqlite

echo Compiling main...
g++ -c backend\main.cpp -o build\obj\main.o ^
    -std=c++17 -O2 -Ibackend -Ibackend\models -Ibackend\services -Ibackend\database ^
    -Ibackend\api -Ithird_party -Ithird_party\sqlite

echo Linking backend...
g++ build\obj\main.o build\obj\database.o build\obj\sqlite3.o ^
    build\obj\disruption_service.o build\obj\shipment_service.o ^
    build\obj\route_service.o build\obj\carrier_service.o ^
    build\obj\fleet_service.o build\obj\cold_chain_service.o ^
    build\obj\recommendation_service.o build\obj\simulation_service.o ^
    build\obj\api_handler.o ^
    -o build\bin\supply_chain_backend.exe ^
    -std=c++17 -lws2_32 -lmswsock

echo.
echo Compiling tests...
for %%F in (test_disruption test_routing test_carrier test_fleet test_cold_chain) do (
    g++ -c tests\%%F.cpp -o build\obj\%%F.o ^
        -std=c++17 -O2 -Ibackend -Ibackend\models -Ibackend\services -Ibackend\database ^
        -Ibackend\api -Ithird_party -Ithird_party\sqlite -DTESTING_MODE
)
g++ -c tests\test_main.cpp -o build\obj\test_main.o ^
    -std=c++17 -O2 -Ibackend -Ithird_party -Ithird_party\sqlite -DTESTING_MODE

echo Linking tests...
g++ build\obj\test_main.o build\obj\test_disruption.o build\obj\test_routing.o ^
    build\obj\test_carrier.o build\obj\test_fleet.o build\obj\test_cold_chain.o ^
    build\obj\database.o build\obj\sqlite3.o ^
    build\obj\disruption_service.o build\obj\shipment_service.o ^
    build\obj\route_service.o build\obj\carrier_service.o ^
    build\obj\fleet_service.o build\obj\cold_chain_service.o ^
    build\obj\recommendation_service.o build\obj\simulation_service.o ^
    build\obj\api_handler.o ^
    -o build\bin\run_tests.exe ^
    -std=c++17 -lws2_32

echo Copying frontend...
xcopy /E /Y /I frontend build\bin\frontend >nul

echo.
echo === Build Complete ===
echo.
echo Run backend:  build\bin\supply_chain_backend.exe --frontend .\frontend
echo Run tests:    build\bin\run_tests.exe
echo Open:         http://localhost:8080
