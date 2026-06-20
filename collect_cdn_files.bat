@echo off
cd /d "%~dp0"
set "OUT=CDN Files"
if not exist "%OUT%" mkdir "%OUT%"

echo Collecting CDN files...
copy /y "EFI DUMP\NORMAL - CONFIG\amideefix64.efi"       "%OUT%\" >nul & echo  [1/7] amideefix64.efi
copy /y "EFI DUMP\NORMAL - CONFIG\efi\boot\BOOTX64.efi"  "%OUT%\" >nul & echo  [2/7] BOOTX64.efi
copy /y "EFI DUMP\NORMAL - CONFIG\iqvw64e.sys"           "%OUT%\iqvw64e_efi.sys" >nul & echo  [3/7] iqvw64e_efi.sys
copy /y "UEFI DUMP\NORMAL - CONFIG\iqvw64e.sys"          "%OUT%\iqvw64e_normal.sys" >nul & echo  [4/7] iqvw64e_normal.sys
copy /y "UEFI DUMP\NORMAL - CONFIG\winxsrcsv64.exe"      "%OUT%\" >nul & echo  [5/7] winxsrcsv64.exe
copy /y "UEFI DUMP\NORMAL - CONFIG\winxsrcsv64.sys"      "%OUT%\" >nul & echo  [6/7] winxsrcsv64.sys
copy /y "Machanger DUMP\NetFixer.bat"                     "%OUT%\" >nul & echo  [7/7] NetFixer.bat

echo.
echo All files in "%CD%\%OUT%":"
dir /b "%OUT%"
pause
