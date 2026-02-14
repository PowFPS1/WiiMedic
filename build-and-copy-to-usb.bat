@echo off
REM Build WiiMedic and copy to E:\apps\WiiMedic
REM Requires DEVKITPPC in PATH (devkitPro)

set "SRC=%~dp0"
set "BUILD=C:\WiiMedic_build"
set "USB=E:\apps\WiiMedic"
set "LOG=%SRC%build_log.txt"

echo Build started %date% %time% > "%LOG%"
echo Copying project to %BUILD% ... >> "%LOG%"
if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%"
robocopy "%SRC%" "%BUILD%" /E /XD .git build /NFL /NDL /NJH /NJS /NC /NS >nul 2>&1
if not exist "%BUILD%\Makefile" (
    echo Copy failed. >> "%LOG%"
    exit /b 1
)

echo Building... >> "%LOG%"
cd /d "%BUILD%"
make clean >> "%LOG%" 2>&1
make >> "%LOG%" 2>&1
if errorlevel 1 (
    echo Build failed. >> "%LOG%"
    exit /b 1
)

if not exist "%BUILD%\boot.dol" (
    echo boot.dol not found after build. >> "%LOG%"
    exit /b 1
)

echo Copying to %USB% ... >> "%LOG%"
if not exist "%USB%" mkdir "%USB%"
if exist "%BUILD%\boot.dol" copy /Y "%BUILD%\boot.dol" "%USB%\boot.dol"
copy /Y "%SRC%meta.xml" "%USB%\meta.xml" 2>nul
if exist "%SRC%icon.png" copy /Y "%SRC%icon.png" "%USB%\icon.png" 2>nul
echo Done. WiiMedic at E:\apps\WiiMedic >> "%LOG%"
