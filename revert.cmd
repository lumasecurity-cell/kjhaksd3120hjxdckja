@echo off
cd /d "%~dp0"
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    goto :EOF
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0woof.ps1" -Revert
pause
