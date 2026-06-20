param([switch]$Revert)

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$WoofDir = "$env:ProgramData\WOOF"

function Run-Cmd {
    param([string]$Command)
    cmd /c $Command 2>&1 | Out-Null
    return $LASTEXITCODE
}

function Mount-ESP {
    Run-Cmd "mountvol S: /D" | Out-Null
    foreach ($letter in "S","M","N","O","P","Q","R","T","U","V","W","X","Y","Z") {
        $r = Run-Cmd "mountvol ${letter}: /S"
        if ($r -eq 0) { return $letter }
    }
    return $null
}

function Dismount-ESP {
    param($Drive)
    Run-Cmd "mountvol ${Drive}: /D" | Out-Null
}

function Write-Nsh {
    param($Path, $Tool, $SM, $SP, $BM, $BP, $BV, $IVN, $IV, $ID, $CM, $CV, $CA, $SS, $BS, $CS)
@"
echo -off
$Tool /SU AUTO
$Tool /BS "$BS"
$Tool /CM "$CM"
$Tool /BT "Default string"
$Tool /CA "$CA"
$Tool /CSK "Default string"
$Tool /SS "$SS"
$Tool /PSN "To Be Filled By O.E.M."
$Tool /SM "$SM"
$Tool /SP "$SP"
$Tool /ID "$ID"
$Tool /IVN "$IVN"
$Tool /IV "$IV"
$Tool /BM "$BM"
$Tool /BP "$BP"
$Tool /BV "$BV"
$Tool /CV "$CV"
$Tool /SK "Default String"
$Tool /SF "To be Filled By O.E.M."
$Tool /CS "$CS"
$Tool /SV "System Version"
$Tool /PPN "Unknown"

\EFI\Microsoft\Boot\bootmgfw.efi
"@ | Set-Content -Path $Path -Encoding Ascii
}

function Write-Randomizer {
    param($Path, $EspDrive, $EfiTool, $WoofDir)
@"
@echo off
setlocal enabledelayedexpansion
mountvol ${EspDrive}: /S >nul 2>&1
if !errorlevel! neq 0 exit /b 1
if not exist "${EspDrive}:\EFI\WOOF" exit /b 1
for /f "usebackq tokens=1,* delims==" %%%%a in ("$WoofDir\hardware.txt") do set "%%%%a=%%%%b"
set CHARS=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
set BS_SERIAL=
set CS_SERIAL=
for /l %%%%i in (1,1,16) do (
    set /a R=!random! %%%% 36
    for %%%%r in (!R!) do set BS_SERIAL=!BS_SERIAL!!CHARS:~%%%%r,1!
    set /a R=!random! %%%% 36
    for %%%%r in (!R!) do set CS_SERIAL=!CS_SERIAL!!CHARS:~%%%%r,1!
)
(
    echo echo -off
    echo $EfiTool /SU AUTO
    echo $EfiTool /BS "!BS_SERIAL!"
    echo $EfiTool /CM "!CM!"
    echo $EfiTool /BT "Default string"
    echo $EfiTool /CA "!CA!"
    echo $EfiTool /CSK "Default string"
    echo $EfiTool /SS "!SS!"
    echo $EfiTool /PSN "To Be Filled By O.E.M."
    echo $EfiTool /SM "!SM!"
    echo $EfiTool /SP "!SP!"
    echo $EfiTool /ID "!ID!"
    echo $EfiTool /IVN "!IVN!"
    echo $EfiTool /IV "!IV!"
    echo $EfiTool /BM "!BM!"
    echo $EfiTool /BP "!BP!"
    echo $EfiTool /BV "!BV!"
    echo $EfiTool /CV "!CV!"
    echo $EfiTool /SK "Default String"
    echo $EfiTool /SF "To be Filled By O.E.M."
    echo $EfiTool /CS "!CS_SERIAL!"
    echo $EfiTool /SV "System Version"
    echo $EfiTool /PPN "Unknown"
    echo.
    echo \EFI\Microsoft\Boot\bootmgfw.efi
) > "${EspDrive}:\EFI\WOOF\startup.nsh"
if exist "$WoofDir\guid.txt" (
    set /p GUID=<"$WoofDir\guid.txt"
    bcdedit /bootsequence !GUID! /addfirst >nul 2>&1
)
mountvol ${EspDrive}: /D >nul 2>&1
exit /b 0
"@ | Set-Content -Path $Path -Encoding Ascii
}

function Show-Banner {
    Write-Host "============================================"
    Write-Host "    WOOF SYSTEM SPOOFER"
    Write-Host "   Auto-Detect & Apply - Any Motherboard"
    Write-Host "============================================"
    Write-Host ""
}

# ==================== REVERT ====================
if ($Revert) {
    Show-Banner
    Write-Host " REVERTING all changes..."
    Write-Host ""
    Write-Host " [1/7] Deleting scheduled task..."
    Run-Cmd "schtasks /delete /tn ""WOOF Serial Randomizer"" /f"
    if ($LASTEXITCODE -eq 0) { Write-Host "   Removed." } else { Write-Host "   Not found." }
    Write-Host ""

    Write-Host " [2/7] Removing EFI boot files from ESP..."
    $esp = Mount-ESP
    if ($esp) {
        if (Test-Path "${esp}:\EFI\WOOF") {
            Remove-Item -Recurse -Force "${esp}:\EFI\WOOF"
            Write-Host "   ${esp}:\\EFI\\WOOF\\ deleted."
        } else {
            Write-Host "   No WOOF folder found."
        }
        Dismount-ESP $esp
    } else {
        Write-Host "   Could not mount ESP. Files may remain."
    }
    Write-Host ""

    Write-Host " [3/7] Removing UEFI boot entry..."
    $bcdOut = cmd /c "bcdedit /enum firmware"
    $guid = $null
    foreach ($line in $bcdOut) {
        if ($line -match '\{([0-9a-f\-]+)\}\s+WOOF Spoofer') {
            $guid = "{$($matches[1])}"
            break
        }
    }
    if ($guid) {
        Run-Cmd "bcdedit /delete $guid /f"
        Write-Host "   Boot entry deleted: $guid"
    } else {
        Write-Host "   No WOOF boot entry found."
    }
    Write-Host ""

    Write-Host " [4/7] Cleaning up ProgramData files..."
    if (Test-Path $WoofDir) {
        Remove-Item -Recurse -Force $WoofDir
        Write-Host "   $WoofDir deleted."
    } else {
        Write-Host "   Not found."
    }
    Write-Host ""

    Write-Host " [5/7] Restoring network adapters..."
    $nics = Get-WmiObject Win32_NetworkAdapter | Where-Object { $_.PhysicalAdapter }
    foreach ($nic in $nics) {
        $key = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4D36E972-E325-11CE-BFC1-08002bE10318}\$($nic.DeviceID.PadLeft(4,'0'))"
        if (Test-Path $key) {
            Remove-ItemProperty -Path $key -Name "NetworkAddress" -ErrorAction SilentlyContinue
            Remove-ItemProperty -Path $key -Name "PnPCapabilities" -ErrorAction SilentlyContinue
        }
    }
    foreach ($nic in $nics) {
        if ($nic.NetConnectionId) {
            Run-Cmd "netsh interface set interface name=""$($nic.NetConnectionId)"" disable" | Out-Null
            Run-Cmd "netsh interface set interface name=""$($nic.NetConnectionId)"" enable" | Out-Null
        }
    }
    Write-Host "   Network adapters restored."
    Write-Host ""

    Write-Host " [6/7] Removing Intel RST driver..."
    $drivers = cmd /c "pnputil /enum-drivers"
    $infName = $null
    for ($i = 0; $i -lt $drivers.Count; $i++) {
        if ($drivers[$i] -match 'iqvw64e') {
            if ($i -gt 0 -and $drivers[$i-1] -match ':\s+(\S+\.inf)') {
                $infName = $matches[1]
                break
            }
        }
    }
    if ($infName) {
        Run-Cmd "pnputil /delete-driver $infName /uninstall"
        Write-Host "   Intel RST driver removed."
    } else {
        Write-Host "   No Intel RST driver found."
    }
    Write-Host ""

    Write-Host " [7/7] Removing AMI utility driver..."
    Run-Cmd "sc delete AMIDrv"
    Run-Cmd "sc delete winxsrcsv64"
    Write-Host "   AMI driver service removed (if found)."
    Write-Host ""

    Write-Host "============================================"
    Write-Host "       REVERT COMPLETE"
    Write-Host "============================================"
    Write-Host ""
    Write-Host " Restart your PC to fully reset all settings."
    Write-Host ""
    [Console]::Beep(500,400)
    Start-Sleep -Milliseconds 200
    [Console]::Beep(400,600)
    return
}

# ==================== MAIN ====================
Show-Banner

# ---- DETECT DMI ----
Write-Host " [Detecting hardware DMI values...]"

$cs = Get-CimInstance Win32_ComputerSystem
$bb = Get-CimInstance Win32_BaseBoard
$bios = Get-CimInstance Win32_BIOS
$enc = Get-CimInstance Win32_SystemEnclosure
$prod = Get-CimInstance Win32_ComputerSystemProduct

function Safe {
    param($Value, $Fallback = "Default string")
    if ($Value) { $Value.Trim() } else { $Fallback }
}

$SM = Safe $cs.Manufacturer
$SP = Safe $cs.Model
$BM = Safe $bb.Manufacturer
$BP = Safe $bb.Product
$BV = Safe $bb.Version
$IVN = Safe $bios.Manufacturer
$IV = Safe $bios.SMBIOSBIOSVersion
$raw = if ($bios.ReleaseDate) { $bios.ReleaseDate -replace '\D','' } else { "" }
$ID = if ($raw.Length -ge 8) { "$($raw.Substring(0,4))/$($raw.Substring(4,2))/$($raw.Substring(6,2))" } else { "Default string" }
$CM = Safe $enc.Manufacturer ($SM)
$CV = Safe $enc.Version
$CA = Safe $enc.SMBIOSAssetTag
$SS = Safe $prod.IdentifyingNumber

Write-Host "   System: $SM $SP"
Write-Host "   BIOS:   $IVN $IV"
Write-Host "   Board:  $BM $BP v$BV"
Write-Host ""

# ---- GPU DETECT ----
$gpuName = try { (Get-CimInstance Win32_VideoController).Name.Trim() } catch { $null }
$configName = "Normal"
$efiDir = "NORMAL - CONFIG"
$uefiDir = "NORMAL - CONFIG"
if ($gpuName -match 'GeForce|Radeon|RX|RTX|GTX') {
    $configName = "GPU"
    $efiDir = "GPU - CONIFG"
    $uefiDir = "GPU - CONFIG"
}
Write-Host " Config: $configName (GPU: $(if ($gpuName) { $gpuName } else { 'None' }))"
Write-Host ""

# ---- TOOL SELECT ----
$efiTool = "AMIDEEFIx64.efi"
$choice = Read-Host " Use AFUEFI instead of AMIDEEFI? (Y/N) [N]"
if ($choice -eq 'Y' -or $choice -eq 'y') { $efiTool = "AFUEFIx64.efi" }
Write-Host ""

# ---- CONFIRM ----
$choice = Read-Host " Start? (Y/N)"
if ($choice -ne 'Y' -and $choice -ne 'y') { Write-Host " Aborted."; return }
Write-Host ""

# ---- RANDOMIZE BS/CS ----
Write-Host " [1] Generating random serials..."
$chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
$rng = New-Object System.Random
$bsSerial = -join (1..16 | ForEach-Object { $chars[$rng.Next(36)] })
$csSerial = -join (1..16 | ForEach-Object { $chars[$rng.Next(36)] })
Write-Host "   BS: $bsSerial"
Write-Host "   CS: $csSerial"
Write-Host ""

# ---- DEPLOY TO ESP ----
Write-Host " [2] Deploying to System Partition..."
$espDrive = Mount-ESP
if (-not $espDrive) {
    Write-Host "   ERROR: Cannot mount ESP. Are you on UEFI and running as Admin?"
    Write-Host "   Press Enter to exit."; Read-Host; return
}
Write-Host "   Mounted at ${espDrive}:"

if (Test-Path "${espDrive}:\EFI\WOOF") { Remove-Item -Recurse -Force "${espDrive}:\EFI\WOOF" -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path "${espDrive}:\EFI\WOOF" | Out-Null

$sourceEfi = "$ScriptRoot\EFI DUMP\$efiDir"
$sourceUefi = "$ScriptRoot\UEFI DUMP\$uefiDir"

$files = @(
    "$sourceEfi\amideefix64.efi",
    "$sourceEfi\efi\boot\BOOTX64.efi",
    "$sourceEfi\iqvw64e.sys"
)
foreach ($f in $files) {
    if (Test-Path $f) {
        Copy-Item $f "${espDrive}:\EFI\WOOF\" -Force
        Write-Host "   Copied $(Split-Path -Leaf $f)"
    } else {
        Write-Host "   WARNING: $f not found"
    }
}

Write-Nsh -Path "${espDrive}:\EFI\WOOF\startup.nsh" -Tool $efiTool -SM $SM -SP $SP -BM $BM -BP $BP -BV $BV -IVN $IVN -IV $IV -ID $ID -CM $CM -CV $CV -CA $CA -SS $SS -BS $bsSerial -CS $csSerial
Write-Host "   startup.nsh written"

New-Item -ItemType Directory -Force -Path $WoofDir | Out-Null
@"
SM=$SM
SP=$SP
BM=$BM
BP=$BP
BV=$BV
IVN=$IVN
IV=$IV
ID=$ID
CM=$CM
CV=$CV
CA=$CA
SS=$SS
"@ | Set-Content -Path "$WoofDir\hardware.txt" -Encoding Ascii
Write-Host ""

# ---- BOOT ENTRY ----
Write-Host " [3] Creating UEFI boot entry..."
$bcdOut = cmd /c "bcdedit /create /d ""WOOF Spoofer"" /application osloader 2>&1"
$guid = $null
foreach ($line in @($bcdOut)) {
    if ($line -match '\{([0-9a-f\-]+)\}') {
        $guid = "{$($matches[1])}"
        break
    }
}
if ($guid) {
    Run-Cmd "bcdedit /set $guid device partition=system"
    Run-Cmd "bcdedit /set $guid path \EFI\WOOF\BOOTX64.efi"
    Run-Cmd "bcdedit /bootsequence $guid /addfirst"
    Set-Content -Path "$WoofDir\guid.txt" -Value $guid
    Write-Host "   Boot entry created."
} else {
    Write-Host "   WARNING: Could not create boot entry."
    Write-Host "   $bcdOut"
}
Write-Host ""

# ---- SCHEDULED TASK ----
Write-Host " [4] Installing auto-randomizer..."
Write-Randomizer -Path "$WoofDir\randomizer.cmd" -EspDrive $espDrive -EfiTool $efiTool -WoofDir $WoofDir
$r = Run-Cmd "schtasks /create /tn ""WOOF Serial Randomizer"" /tr ""cmd /c $WoofDir\randomizer.cmd"" /sc onstart /ru SYSTEM /rl highest /f"
if ($LASTEXITCODE -eq 0) {
    Write-Host "   Scheduled task installed."
} else {
    Write-Host "   WARNING: Could not create scheduled task."
}
Write-Host ""

# ---- MAC SPOOF ----
Write-Host " [5] Spoofing MAC addresses..."
$netFixer = "$ScriptRoot\Machanger DUMP\NetFixer.bat"
if (Test-Path $netFixer) {
    Push-Location "$ScriptRoot\Machanger DUMP"
    cmd /c "NetFixer.bat"
    Pop-Location
    Write-Host "   MAC spoofing complete."
} else {
    Write-Host "   WARNING: NetFixer.bat not found!"
}
Write-Host ""

# ---- RST DRIVER ----
Write-Host " [6] Installing Intel RST driver..."
$rstDriver = "$sourceUefi\iqvw64e.sys"
if (Test-Path $rstDriver) {
    Run-Cmd "pnputil /add-driver ""$rstDriver"" /install"
    Write-Host "   Intel RST driver installed."
} else {
    Write-Host "   WARNING: iqvw64e.sys not found."
}
Write-Host ""

# ---- AMI DRIVER ----
$amiLoaded = "No"
Write-Host " [7] Loading AMI utility driver..."
$amiExe = "$sourceUefi\winxsrcsv64.exe"
if (Test-Path $amiExe) {
    Start-Process -NoNewWindow -FilePath $amiExe
    Start-Sleep -Seconds 3
    $amiLoaded = "Yes"
    Write-Host "   AMI driver loaded."
} else {
    Write-Host "   Skipped."
}
Write-Host ""

# ---- UNMOUNT ----
Dismount-ESP $espDrive

# ---- DONE ----
Write-Host "============================================"
Write-Host "       ALL DONE - SPOOFING COMPLETE"
Write-Host "============================================"
Write-Host ""
Write-Host " MAC:  Spoofed"
Write-Host " RST:  $configName installed"
Write-Host " AMI:  $amiLoaded"
Write-Host ""
Write-Host " Boot cycle: random serials -> reboot -> auto-flash -> repeat"
Write-Host " Restart your PC to begin."
Write-Host ""
[Console]::Beep(800,200)
Start-Sleep -Milliseconds 100
[Console]::Beep(1000,300)
Start-Sleep -Milliseconds 200
[Console]::Beep(1200,500)
Read-Host "Press Enter"
