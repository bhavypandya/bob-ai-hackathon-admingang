@echo off
REM SupplyGuard — Launch Server
REM Double-click this file from anywhere to start the backend correctly

cd /d "%~dp0src\build\bin"

if not exist "supply_chain_backend.exe" (
    echo.
    echo ERROR: supply_chain_backend.exe not found.
    echo Please build first by running:  src\build.bat
    echo.
    pause
    exit /b 1
)

if not exist "frontend\index.html" (
    echo.
    echo ERROR: frontend\index.html not found.
    echo Please build first by running:  src\build.bat
    echo.
    pause
    exit /b 1
)

echo.
echo  ===========================================
echo   SupplyGuard - Supply Chain Command Centre
echo  ===========================================
echo.
echo  Starting backend server...
echo  Open browser at:  http://localhost:8080
echo  Press Ctrl+C to stop.
echo.

supply_chain_backend.exe --frontend .\frontend
pause
