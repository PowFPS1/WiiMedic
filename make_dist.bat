@echo off
REM WiiMedic - Distribution Package Builder
REM Run this AFTER building with 'make' to create a ready-to-use SD card package

echo ==========================================
echo  WiiMedic Distribution Package Builder
echo ==========================================
echo.

if not exist "boot.dol" (
    echo ERROR: boot.dol not found!
    echo Please run 'make' first to build the project.
    echo.
    echo To build:
    echo   1. Open devkitPro MSys2 terminal
    echo   2. cd to this directory
    echo   3. Run 'make'
    echo.
    pause
    exit /b 1
)

echo Creating distribution package...

REM Create distribution directory
if exist "dist" rmdir /s /q "dist"
mkdir "dist\apps\WiiMedic"

REM Copy files
copy "boot.dol" "dist\apps\WiiMedic\boot.dol"
copy "meta.xml" "dist\apps\WiiMedic\meta.xml"

REM Check for icon
if exist "icon.png" (
    copy "icon.png" "dist\apps\WiiMedic\icon.png"
    echo Icon included.
) else (
    echo NOTE: No icon.png found. App will show without icon in HBC.
    echo       Create a 128x48 PNG named icon.png in this directory.
)

echo.
echo ==========================================
echo  Package created in: dist\apps\WiiMedic\
echo ==========================================
echo.
echo To install on your Wii:
echo   1. Copy the 'apps' folder from 'dist\' to your SD card root
echo   2. Your SD card should have: SD:\apps\WiiMedic\boot.dol
echo   3. Insert SD card and launch from Homebrew Channel
echo.
pause
