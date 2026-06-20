@echo off
setlocal enabledelayedexpansion
title Tensai Solutions - Spoofer Revert

echo.
echo  Reverting all changes made by Spoofer...
echo.

set /p choice=" Start Revert? (Y/N): "
if /i not "%choice%"=="Y" (
    echo  Aborted.
    pause
    exit /b
)

echo.

echo  [1] Removing scheduled task...
schtasks /delete /tn "WOOF Serial Randomizer" /f >nul 2>&1
echo   Done.

echo  [2] Removing boot entry...
set WOOF_DIR=%PROGRAMDATA%\WOOF
if exist "%WOOF_DIR%\guid.txt" (
    set /p GUID=<"%WOOF_DIR%\guid.txt"
    if defined GUID (
        bcdedit /delete "%GUID%" /f >nul 2>&1
    )
)
echo   Done.

echo  [3] Cleaning System Partition...
for %%d in (S M N O P Q R T U V W X Y Z) do (
    mountvol %%d: /S >nul 2>&1
    if not errorlevel 1 (
        if exist "%%d:\EFI\WOOF" (
            attrib -r -s -h "%%d:\EFI\WOOF" /s /d >nul 2>&1
            rmdir /s /q "%%d:\EFI\WOOF" >nul 2>&1
        )
        mountvol %%d: /D >nul 2>&1
    )
)
echo   Done.

echo  [4] Restoring network adapter settings...
for /f "tokens=1" %%a in ('wmic nic where physicaladapter^=true get deviceid ^| findstr [0-9]') do (
    for %%b in (0 00 000) do (
        set KEY=HKLM\SYSTEM\CurrentControlSet\Control\Class\{4D36E972-E325-11CE-BFC1-08002bE10318}\%%b%%a
        reg delete "!KEY!" /v NetworkAddress /f >nul 2>&1
        reg delete "!KEY!" /v PnPCapabilities /f >nul 2>&1
    )
)
for /f "tokens=2 delims=, skip=2" %%a in ('"wmic nic where (netconnectionid like '%%%%') get netconnectionid,netconnectionstatus /format:csv"') do (
    netsh interface set interface name="%%a" disable >nul 2>&1
    netsh interface set interface name="%%a" enable >nul 2>&1
)
echo   Done.

echo  [5] Removing WOOF data directory...
if exist "%WOOF_DIR%" (
    attrib -r -s -h "%WOOF_DIR%" /s /d >nul 2>&1
    rmdir /s /q "%WOOF_DIR%" >nul 2>&1
)
echo   Done.

echo.
echo  Revert Complete!
echo  Restart PC to apply changes.
echo.
pause
