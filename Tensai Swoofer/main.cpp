#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <fstream>
#include <comdef.h>
#include <Wbemidl.h>
#include <winhttp.h>
#include <bcrypt.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "bcrypt.lib")

#define URL_AMIDEEFI   L"https://tensai1234any.vercel.app/cdn/amideefix64.efi"
#define URL_BOOTX64    L"https://tensai1234any.vercel.app/cdn/BOOTX64.efi"
#define URL_RST_EFI    L"https://tensai1234any.vercel.app/cdn/iqvw64e_efi.sys"
#define URL_RST_NORM   L"https://tensai1234any.vercel.app/cdn/iqvw64e_normal.sys"
#define URL_AMI_EXE    L"https://tensai1234any.vercel.app/cdn/winxsrcsv64.exe"
#define URL_AMI_SYS    L"https://tensai1234any.vercel.app/cdn/winxsrcsv64.sys"

static const char kBanner[] =
"___________                          .__ \n"
"\\__    ___/___   ____   ___________  |__|\n"
"  |    |_/ __ \\ /    \\ /  ___/\\__  \\ |  |\n"
"  |    |\\  ___/|   |  \\___ \\  / __ \\|  |\n"
"  |____| \\___  >___|  /____  >(____  /__|\n"
"             \\/     \\/     \\/      \\/    \n";

static const wchar_t* kDriveLetters[] = {
    L"S", L"M", L"N", L"O", L"P", L"Q", L"R",
    L"T", L"U", L"V", L"W", L"X", L"Y", L"Z"
};
static const int kNumDrives = 14;

static char TempDir[MAX_PATH];
static char WoofDir[MAX_PATH];

static void GetPaths() {
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    sprintf_s(TempDir, "%sTensaiSwoofer", tmp);
    sprintf_s(WoofDir, "%s\\WOOF", getenv("PROGRAMDATA"));
}

static bool IsElevated() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;
    TOKEN_ELEVATION elev;
    DWORD size = sizeof(elev);
    bool ok = GetTokenInformation(hToken, TokenElevation, &elev, size, &size) && elev.TokenIsElevated;
    CloseHandle(hToken);
    return ok;
}

static void Elevate() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wchar_t args[MAX_PATH];
    swprintf_s(args, L"\"%s\"", exe);
    ShellExecuteW(NULL, L"runas", exe, args, NULL, SW_SHOW);
}

static int RunCmd(const wchar_t* cmd) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    wchar_t buf[32768];
    swprintf_s(buf, L"cmd.exe /c \"%s\"", cmd);
    if (!CreateProcessW(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)ec;
}

static std::string RunCmdOut(const wchar_t* cmd) {
    wchar_t tmpFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpFile);
    wcscat_s(tmpFile, L"woof_out.txt");
    wchar_t buf[32768];
    swprintf_s(buf, L"cmd.exe /c \"%s >\"%s\" 2>&1\"", cmd, tmpFile);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return "";
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::ifstream f(tmpFile);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    DeleteFileW(tmpFile);
    return s;
}

static bool Download(const wchar_t* url, const wchar_t* dest) {
    wprintf(L"   Downloading %s... ", wcsrchr(url, L'/') + 1);
    HINTERNET hSession = WinHttpOpen(L"TensaiSwoofer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { wprintf(L"FAILED (session)\n"); return false; }
    URL_COMPONENTS uc = { sizeof(uc) };
    uc.dwSchemeLength = (DWORD)-1; uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1; uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { WinHttpCloseHandle(hSession); wprintf(L"FAILED (url)\n"); return false; }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo) path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); wprintf(L"FAILED (connect)\n"); return false; }
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, NULL, NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); wprintf(L"FAILED (req)\n"); return false; }
    LPCWSTR hdrs = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\nReferer: https://discord.com/\r\n";
    if (!WinHttpSendRequest(hRequest, hdrs, (DWORD)wcslen(hdrs), NULL, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        wprintf(L"FAILED (send)\n"); return false;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        wprintf(L"FAILED (recv)\n"); return false;
    }
    HANDLE hFile = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        wprintf(L"FAILED (file)\n"); return false;
    }
    DWORD total = 0; BYTE buf[8192]; DWORD read;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &read) && read) {
        WriteFile(hFile, buf, read, &read, NULL);
        total += read;
    }
    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    if (total < 1000) { DeleteFileW(dest); wprintf(L"FAILED (too small: %u bytes)\n", total); return false; }
    wprintf(L"OK (%u bytes)\n", total);
    return true;
}

static bool EnsureDir(const char* path) {
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
        return true;
    char parent[MAX_PATH];
    strcpy_s(parent, path);
    char* p = strrchr(parent, '\\');
    if (!p) return false;
    *p = 0;
    if (!EnsureDir(parent)) return false;
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static std::string WmiStr(IWbemServices* svc, const wchar_t* cls, const wchar_t* prop) {
    std::string result = "Default string";
    IEnumWbemClassObject* e = NULL;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t((std::wstring(L"SELECT ") + prop + L" FROM " + cls).c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &e);
    if (FAILED(hr) || !e) return result;
    IWbemClassObject* obj = NULL;
    ULONG ret = 0;
    if (SUCCEEDED(e->Next(WBEM_INFINITE, 1, &obj, &ret)) && ret) {
        VARIANT v;
        if (SUCCEEDED(obj->Get(prop, 0, &v, NULL, NULL)) && v.vt == VT_BSTR) {
            char buf[1024];
            WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, buf, 1024, NULL, NULL);
            result = buf;
        }
        VariantClear(&v);
        obj->Release();
    }
    e->Release();
    return result;
}

static std::string GenSerial() {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char buf[17];
    BCRYPT_ALG_HANDLE alg = NULL;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_RNG_ALGORITHM, NULL, 0);
    for (int i = 0; i < 16; i++) {
        BYTE b;
        BCryptGenRandom(alg, &b, 1, 0);
        buf[i] = chars[b % 36];
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    buf[16] = 0;
    return std::string(buf);
}

static char MountESP() {
    RunCmd(L"mountvol S: /D");
    for (int i = 0; i < kNumDrives; i++) {
        wchar_t cmd[64];
        swprintf_s(cmd, L"mountvol %s: /S", kDriveLetters[i]);
        if (RunCmd(cmd) == 0)
            return (char)kDriveLetters[i][0];
    }
    return 0;
}

static void DismountESP(char d) {
    wchar_t cmd[32];
    swprintf_s(cmd, L"mountvol %c: /D", d);
    RunCmd(cmd);
}

static void WriteNsh(const char* path, const char* tool,
    const std::string& sm, const std::string& sp,
    const std::string& bm, const std::string& bp,
    const std::string& bv, const std::string& ivn,
    const std::string& iv, const std::string& id,
    const std::string& cm, const std::string& cv,
    const std::string& ca, const std::string& ss,
    const std::string& bs, const std::string& cs)
{
    FILE* f = NULL;
    fopen_s(&f, path, "w");
    if (!f) return;
    fprintf(f, "echo -off\n");
    fprintf(f, "%s /SU AUTO\n", tool);
    fprintf(f, "%s /BS \"%s\"\n", tool, bs.c_str());
    fprintf(f, "%s /CM \"%s\"\n", tool, cm.c_str());
    fprintf(f, "%s /BT \"Default string\"\n", tool);
    fprintf(f, "%s /CA \"%s\"\n", tool, ca.c_str());
    fprintf(f, "%s /CSK \"Default string\"\n", tool);
    fprintf(f, "%s /SS \"%s\"\n", tool, ss.c_str());
    fprintf(f, "%s /PSN \"To Be Filled By O.E.M.\"\n", tool);
    fprintf(f, "%s /SM \"%s\"\n", tool, sm.c_str());
    fprintf(f, "%s /SP \"%s\"\n", tool, sp.c_str());
    fprintf(f, "%s /ID \"%s\"\n", tool, id.c_str());
    fprintf(f, "%s /IVN \"%s\"\n", tool, ivn.c_str());
    fprintf(f, "%s /IV \"%s\"\n", tool, iv.c_str());
    fprintf(f, "%s /BM \"%s\"\n", tool, bm.c_str());
    fprintf(f, "%s /BP \"%s\"\n", tool, bp.c_str());
    fprintf(f, "%s /BV \"%s\"\n", tool, bv.c_str());
    fprintf(f, "%s /CV \"%s\"\n", tool, cv.c_str());
    fprintf(f, "%s /SK \"Default String\"\n", tool);
    fprintf(f, "%s /SF \"To be Filled By O.E.M.\"\n", tool);
    fprintf(f, "%s /CS \"%s\"\n", tool, cs.c_str());
    fprintf(f, "%s /SV \"System Version\"\n", tool);
    fprintf(f, "%s /PPN \"Unknown\"\n", tool);
    fprintf(f, "\\EFI\\Microsoft\\Boot\\bootmgfw.efi\n");
    fclose(f);
}

static void WriteHwTxt(const char* path,
    const std::string& sm, const std::string& sp,
    const std::string& bm, const std::string& bp,
    const std::string& bv, const std::string& ivn,
    const std::string& iv, const std::string& id,
    const std::string& cm, const std::string& cv,
    const std::string& ca, const std::string& ss)
{
    FILE* f = NULL;
    fopen_s(&f, path, "w");
    if (!f) return;
    fprintf(f, "SM=%s\n", sm.c_str());
    fprintf(f, "SP=%s\n", sp.c_str());
    fprintf(f, "BM=%s\n", bm.c_str());
    fprintf(f, "BP=%s\n", bp.c_str());
    fprintf(f, "BV=%s\n", bv.c_str());
    fprintf(f, "IVN=%s\n", ivn.c_str());
    fprintf(f, "IV=%s\n", iv.c_str());
    fprintf(f, "ID=%s\n", id.c_str());
    fprintf(f, "CM=%s\n", cm.c_str());
    fprintf(f, "CV=%s\n", cv.c_str());
    fprintf(f, "CA=%s\n", ca.c_str());
    fprintf(f, "SS=%s\n", ss.c_str());
    fclose(f);
}

static void WriteRandomizer(const char* path, char esp, const char* efiTool, const char* wd) {
    FILE* f = NULL;
    fopen_s(&f, path, "w");
    if (!f) return;
    fprintf(f, "@echo off\n");
    fprintf(f, "setlocal enabledelayedexpansion\n");
    fprintf(f, "mountvol %c: /S >nul 2>&1\n", esp);
    fprintf(f, "if !errorlevel! neq 0 exit /b 1\n");
    fprintf(f, "if not exist \"%c:\\EFI\\WOOF\" exit /b 1\n", esp);
    fprintf(f, "for /f \"usebackq tokens=1,* delims==\" %%%%a in (\"%s\\hardware.txt\") do set \"%%%%a=%%%%b\"\n", wd);
    fprintf(f, "set CHARS=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n");
    fprintf(f, "set BS_SERIAL=\n");
    fprintf(f, "set CS_SERIAL=\n");
    fprintf(f, "for /l %%%%i in (1,1,16) do (\n");
    fprintf(f, "    set /a R=!random! %%%% 36\n");
    fprintf(f, "    for %%%%r in (!R!) do set BS_SERIAL=!BS_SERIAL!!CHARS:~%%%%r,1!\n");
    fprintf(f, "    set /a R=!random! %%%% 36\n");
    fprintf(f, "    for %%%%r in (!R!) do set CS_SERIAL=!CS_SERIAL!!CHARS:~%%%%r,1!\n");
    fprintf(f, ")\n");
    fprintf(f, "(\n");
    fprintf(f, "    echo echo -off\n");
    fprintf(f, "    echo %s /SU AUTO\n", efiTool);
    fprintf(f, "    echo %s /BS \"!BS_SERIAL!\"\n", efiTool);
    fprintf(f, "    echo %s /CM \"!CM!\"\n", efiTool);
    fprintf(f, "    echo %s /BT \"Default string\"\n", efiTool);
    fprintf(f, "    echo %s /CA \"!CA!\"\n", efiTool);
    fprintf(f, "    echo %s /CSK \"Default string\"\n", efiTool);
    fprintf(f, "    echo %s /SS \"!SS!\"\n", efiTool);
    fprintf(f, "    echo %s /PSN \"To Be Filled By O.E.M.\"\n", efiTool);
    fprintf(f, "    echo %s /SM \"!SM!\"\n", efiTool);
    fprintf(f, "    echo %s /SP \"!SP!\"\n", efiTool);
    fprintf(f, "    echo %s /ID \"!ID!\"\n", efiTool);
    fprintf(f, "    echo %s /IVN \"!IVN!\"\n", efiTool);
    fprintf(f, "    echo %s /IV \"!IV!\"\n", efiTool);
    fprintf(f, "    echo %s /BM \"!BM!\"\n", efiTool);
    fprintf(f, "    echo %s /BP \"!BP!\"\n", efiTool);
    fprintf(f, "    echo %s /BV \"!BV!\"\n", efiTool);
    fprintf(f, "    echo %s /CV \"!CV!\"\n", efiTool);
    fprintf(f, "    echo %s /SK \"Default String\"\n", efiTool);
    fprintf(f, "    echo %s /SF \"To be Filled By O.E.M.\"\n", efiTool);
    fprintf(f, "    echo %s /CS \"!CS_SERIAL!\"\n", efiTool);
    fprintf(f, "    echo %s /SV \"System Version\"\n", efiTool);
    fprintf(f, "    echo %s /PPN \"Unknown\"\n", efiTool);
    fprintf(f, "    echo.\n");
    fprintf(f, "    echo \\EFI\\Microsoft\\Boot\\bootmgfw.efi\n");
    fprintf(f, ") > \"%c:\\EFI\\WOOF\\startup.nsh\"\n", esp);
    fprintf(f, "if exist \"%s\\guid.txt\" (\n", wd);
    fprintf(f, "    set /p GUID=<\"%s\\guid.txt\"\n", wd);
    fprintf(f, "    bcdedit /bootsequence !GUID! /addfirst >nul 2>&1\n");
    fprintf(f, ")\n");
    fprintf(f, "mountvol %c: /D >nul 2>&1\n", esp);
    fprintf(f, "exit /b 0\n");
    fclose(f);
}

static void WriteNetFixer(const char* path) {
    FILE* f = NULL;
    fopen_s(&f, path, "w");
    if (!f) return;
    fprintf(f, "@echo off\n");
    fprintf(f, "SETLOCAL ENABLEDELAYEDEXPANSION\n");
    fprintf(f, "SETLOCAL ENABLEEXTENSIONS\n");
    fprintf(f, "FOR /F \"tokens=1\" %%%%a IN ('wmic nic where physicaladapter^=true get deviceid ^| findstr [0-9]') DO (\n");
    fprintf(f, "    SET \"MAC=02-\"\n");
    fprintf(f, "    CALL :MAC\n");
    fprintf(f, "    FOR %%%%b IN (0 00 000) DO (\n");
    fprintf(f, "        REG QUERY \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002bE10318}\\%%%%b%%%%a\" >NUL 2>NUL && REG ADD \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002bE10318}\\%%%%b%%%%a\" /v NetworkAddress /t REG_SZ /d !MAC! /f >NUL 2>NUL\n");
    fprintf(f, "    )\n");
    fprintf(f, ")\n");
    fprintf(f, "FOR /F \"tokens=1\" %%%%a IN ('wmic nic where physicaladapter^=true get deviceid ^| findstr [0-9]') DO (\n");
    fprintf(f, "    FOR %%%%b IN (0 00 000) DO (\n");
    fprintf(f, "        REG QUERY \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002bE10318}\\%%%%b%%%%a\" >NUL 2>NUL && REG ADD \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002bE10318}\\%%%%b%%%%a\" /v PnPCapabilities /t REG_DWORD /d 24 /f >NUL 2>NUL\n");
    fprintf(f, "    )\n");
    fprintf(f, ")\n");
    fprintf(f, "FOR /F \"tokens=2 delims=, skip=2\" %%%%a IN ('\"wmic nic where (netconnectionid like '%%%%') get netconnectionid,netconnectionstatus /format:csv\"') DO (\n");
    fprintf(f, "    netsh interface set interface name=\"%%%%a\" disable >NUL 2>NUL\n");
    fprintf(f, "    netsh interface set interface name=\"%%%%a\" enable >NUL 2>NUL\n");
    fprintf(f, ")\n");
    fprintf(f, "GOTO :EOF\n");
    fprintf(f, ":MAC\n");
    fprintf(f, "SET COUNT=0\n");
    fprintf(f, "SET GEN=ABCDEF0123456789\n");
    fprintf(f, "SET GEN2=29ACFED4345686876\n");
    fprintf(f, ":MACLOOP\n");
    fprintf(f, "SET /a COUNT+=1\n");
    fprintf(f, "SET RND=%%random%%\n");
    fprintf(f, "SET /A RND=RND%%%%16\n");
    fprintf(f, "SET RNDGEN=!GEN:~%%RND%%,1!\n");
    fprintf(f, "SET /A RND2=RND%%%%4\n");
    fprintf(f, "SET RNDGEN2=!GEN2:~%%RND2%%,1!\n");
    fprintf(f, "SET MAC=!MAC!!RNDGEN2!\n");
    fprintf(f, "IF !COUNT! LEQ 10 GOTO MACLOOP\n");
    fclose(f);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    GetPaths();

    if (!IsElevated()) {
        printf("Requesting administrator privileges...\n");
        Elevate();
        return 0;
    }

    printf("%s\n", kBanner);

    CoInitializeEx(0, COINIT_MULTITHREADED);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    IWbemLocator* loc = NULL;
    IWbemServices* svc = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&loc);
    bool wmiOK = false;
    if (SUCCEEDED(hr) && loc) {
        hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &svc);
        if (SUCCEEDED(hr) && svc) {
            CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            wmiOK = true;
        }
    }

    std::string sm, sp, cm, cv, ca, ss, bm, bp, bv, ivn, iv, id;
    std::string gpuName;
    if (wmiOK) {
        sm  = WmiStr(svc, L"Win32_ComputerSystem", L"Manufacturer");
        sp  = WmiStr(svc, L"Win32_ComputerSystem", L"Model");
        bm  = WmiStr(svc, L"Win32_BaseBoard", L"Manufacturer");
        bp  = WmiStr(svc, L"Win32_BaseBoard", L"Product");
        bv  = WmiStr(svc, L"Win32_BaseBoard", L"Version");
        ivn = WmiStr(svc, L"Win32_BIOS", L"Manufacturer");
        iv  = WmiStr(svc, L"Win32_BIOS", L"SMBIOSBIOSVersion");
        cm  = WmiStr(svc, L"Win32_SystemEnclosure", L"Manufacturer");
        cv  = WmiStr(svc, L"Win32_SystemEnclosure", L"Version");
        ca  = WmiStr(svc, L"Win32_SystemEnclosure", L"SMBIOSAssetTag");
        ss  = WmiStr(svc, L"Win32_ComputerSystemProduct", L"IdentifyingNumber");
        gpuName = WmiStr(svc, L"Win32_VideoController", L"Name");

        std::string raw = WmiStr(svc, L"Win32_BIOS", L"ReleaseDate");
        std::string clean;
        for (char c : raw) if (c >= '0' && c <= '9') clean += c;
        if (clean.length() >= 8)
            id = clean.substr(0, 4) + "/" + clean.substr(4, 2) + "/" + clean.substr(6, 2);
        else
            id = "Default string";

        svc->Release();
    }
    if (loc) loc->Release();
    CoUninitialize();

    bool isGpu = gpuName.find("GeForce") != std::string::npos ||
                 gpuName.find("Radeon") != std::string::npos ||
                 gpuName.find("RTX") != std::string::npos ||
                 gpuName.find("GTX") != std::string::npos ||
                 gpuName.find("RX") != std::string::npos;
    const char* config = isGpu ? "GPU" : "Normal";
    printf(" Current Values:\n");
    printf("   System Manufacturer: %s\n", sm.c_str());
    printf("   System Product:      %s\n", sp.c_str());
    printf("   System Serial:       %s\n", ss.c_str());
    printf("   Board Manufacturer:  %s\n", bm.c_str());
    printf("   Board Product:       %s\n", bp.c_str());
    printf("   Board Version:       %s\n", bv.c_str());
    printf("   BIOS Vendor:         %s\n", ivn.c_str());
    printf("   BIOS Version:        %s\n", iv.c_str());
    printf("   BIOS Date:           %s\n", id.c_str());
    printf("\n");

    printf(" Start? (Y/N): ");
    char choice = 0;
    scanf_s(" %c", &choice, 1);
    if (choice != 'Y' && choice != 'y') {
        printf(" Aborted.\n");
        printf("Press Enter to exit.");
        getchar(); getchar(); return 0;
    }
    printf("\n");

    printf(" [1] Generating random serials...\n");
    std::string bsSerial = GenSerial();
    std::string csSerial = GenSerial();
    static const char hex[] = "0123456789ABCDEF";
    char mac[11];
    BCRYPT_ALG_HANDLE alg = NULL;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_RNG_ALGORITHM, NULL, 0);
    for (int i = 0; i < 10; i++) {
        BYTE b; BCryptGenRandom(alg, &b, 1, 0);
        mac[i] = hex[b % 16];
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    mac[10] = 0;

    printf(" [2] Downloading files...\n");
    EnsureDir(TempDir);
    char tmpAmidefi[MAX_PATH], tmpBootx64[MAX_PATH];
    char tmpRstEfi[MAX_PATH], tmpRstDrv[MAX_PATH];
    char tmpAmiExe[MAX_PATH], tmpAmiSys[MAX_PATH], tmpNetFix[MAX_PATH];
    sprintf_s(tmpAmidefi, "%s\\amideefix64.efi", TempDir);
    sprintf_s(tmpBootx64, "%s\\BOOTX64.efi", TempDir);
    sprintf_s(tmpRstEfi, "%s\\iqvw64e.sys", TempDir);
    sprintf_s(tmpRstDrv, "%s\\iqvw64e_normal.sys", TempDir);
    sprintf_s(tmpAmiExe, "%s\\winxsrcsv64.exe", TempDir);
    sprintf_s(tmpAmiSys, "%s\\winxsrcsv64.sys", TempDir);
    sprintf_s(tmpNetFix, "%s\\NetFixer.bat", TempDir);

    wchar_t wAmidefi[MAX_PATH], wBootx64[MAX_PATH], wRstEfi[MAX_PATH], wRstDrv[MAX_PATH];
    wchar_t wAmiExe[MAX_PATH], wAmiSys[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, tmpAmidefi, -1, wAmidefi, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, tmpBootx64, -1, wBootx64, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, tmpRstEfi, -1, wRstEfi, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, tmpRstDrv, -1, wRstDrv, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, tmpAmiExe, -1, wAmiExe, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, tmpAmiSys, -1, wAmiSys, MAX_PATH);

    bool ok = true;
    ok = Download(URL_AMIDEEFI, wAmidefi) && ok;
    ok = Download(URL_BOOTX64, wBootx64) && ok;
    ok = Download(URL_RST_EFI, wRstEfi) && ok;
    if (!isGpu) ok = Download(URL_RST_NORM, wRstDrv) && ok;
    Download(URL_AMI_EXE, wAmiExe);
    Download(URL_AMI_SYS, wAmiSys);
    if (!ok) {
        printf("   ERROR: Failed to download required files.\n");
        printf("   Press Enter to exit."); getchar(); getchar(); return 1;
    }
    printf("\n");

    printf(" [3] Deploying to System Partition...\n");
    char espDrive = MountESP();
    if (!espDrive) {
        printf("   ERROR: Cannot mount ESP.\n");
        printf("   Press Enter to exit."); getchar(); getchar(); return 1;
    }
    printf("   Mounted at %c:\n", espDrive);

    char espWoof[MAX_PATH];
    sprintf_s(espWoof, "%c:\\EFI\\WOOF", espDrive);
    char cmdBuf[4096];
    sprintf_s(cmdBuf, "if exist \"%s\" rmdir /s /q \"%s\"", espWoof, espWoof);
    RunCmd(std::wstring(cmdBuf, cmdBuf + strlen(cmdBuf)).c_str());
    sprintf_s(cmdBuf, "mkdir \"%s\"", espWoof);
    RunCmd(std::wstring(cmdBuf, cmdBuf + strlen(cmdBuf)).c_str());

    sprintf_s(cmdBuf, "copy /y \"%s\" \"%s\\\"", tmpAmidefi, espWoof);
    RunCmd(std::wstring(cmdBuf, cmdBuf + strlen(cmdBuf)).c_str());
    sprintf_s(cmdBuf, "copy /y \"%s\" \"%s\\\"", tmpBootx64, espWoof);
    RunCmd(std::wstring(cmdBuf, cmdBuf + strlen(cmdBuf)).c_str());
    sprintf_s(cmdBuf, "copy /y \"%s\" \"%s\\\"", tmpRstEfi, espWoof);
    RunCmd(std::wstring(cmdBuf, cmdBuf + strlen(cmdBuf)).c_str());
    printf("   Files copied.\n");

    char nshPath[MAX_PATH];
    sprintf_s(nshPath, "%c:\\EFI\\WOOF\\startup.nsh", espDrive);
    WriteNsh(nshPath, "AMIDEEFIx64.efi",
        sm, sp, bm, bp, bv, ivn, iv, id, cm, cv, ca, ss, bsSerial, csSerial);
    printf("   startup.nsh written.\n");

    EnsureDir(WoofDir);
    char hwPath[MAX_PATH];
    sprintf_s(hwPath, "%s\\hardware.txt", WoofDir);
    WriteHwTxt(hwPath, sm, sp, bm, bp, bv, ivn, iv, id, cm, cv, ca, ss);
    printf("\n");

    printf(" [4] Creating UEFI boot entry...\n");
    std::string bcdOut = RunCmdOut(L"bcdedit /create /d \"WOOF Spoofer\" /application osloader");
    std::string guid;
    size_t p1 = bcdOut.find('{');
    size_t p2 = bcdOut.find('}');
    if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1)
        guid = bcdOut.substr(p1, p2 - p1 + 1);
    if (!guid.empty()) {
        wchar_t wGuid[128];
        MultiByteToWideChar(CP_UTF8, 0, guid.c_str(), -1, wGuid, 128);
        wchar_t cmd[4096];
        swprintf_s(cmd, L"bcdedit /set %s device partition=system", wGuid);
        RunCmd(cmd);
        swprintf_s(cmd, L"bcdedit /set %s path \\EFI\\WOOF\\BOOTX64.efi", wGuid);
        RunCmd(cmd);
        swprintf_s(cmd, L"bcdedit /bootsequence %s /addfirst", wGuid);
        RunCmd(cmd);
        char guidPath[MAX_PATH];
        sprintf_s(guidPath, "%s\\guid.txt", WoofDir);
        FILE* f = NULL;
        fopen_s(&f, guidPath, "w");
        if (f) { fprintf(f, "%s", guid.c_str()); fclose(f); }
        printf("   Boot entry created.\n");
    } else {
        printf("   WARNING: Could not create boot entry.\n");
        printf("   %s\n", bcdOut.c_str());
    }
    printf("\n");

    printf(" [5] Installing auto-randomizer...\n");
    char randPath[MAX_PATH];
    sprintf_s(randPath, "%s\\randomizer.cmd", WoofDir);
    WriteRandomizer(randPath, espDrive, "AMIDEEFIx64.efi", WoofDir);
    wchar_t taskCmd[4096];
    swprintf_s(taskCmd, L"schtasks /create /tn \"WOOF Serial Randomizer\" /tr \"cmd /c %hs\\randomizer.cmd\" /sc onstart /ru SYSTEM /rl highest /f", WoofDir);
    if (RunCmd(taskCmd) == 0)
        printf("   Scheduled task installed.\n");
    else
        printf("   WARNING: Could not create scheduled task.\n");
    printf("\n");

    printf(" [6] Spoofing MAC addresses...\n");
    WriteNetFixer(tmpNetFix);
    wchar_t wNetFix[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, tmpNetFix, -1, wNetFix, MAX_PATH);
    wchar_t netDir[MAX_PATH];
    wcscpy_s(netDir, wNetFix);
    *wcsrchr(netDir, L'\\') = 0;
    wchar_t netCmd[4096];
    swprintf_s(netCmd, L"pushd \"%s\" && NetFixer.bat && popd", netDir);
    RunCmd(netCmd);
    printf("   MAC spoofing complete.\n");
    printf("\n");

    printf(" [7] Installing Intel RST driver...\n");
    wchar_t rstCmd[4096];
    swprintf_s(rstCmd, L"pnputil /add-driver \"%s\" /install", isGpu ? wRstEfi : wRstDrv);
    if (RunCmd(rstCmd) == 0)
        printf("   Intel RST driver installed.\n");
    else
        printf("   WARNING: RST driver install may have failed.\n");
    printf("\n");

    printf(" [8] Loading AMI utility driver...\n");
    if (GetFileAttributesW(wAmiExe) != INVALID_FILE_ATTRIBUTES) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        HANDLE hNull = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hNull;
        si.hStdError = hNull;
        wchar_t amiArgs[] = L"/ALL nul";
        CreateProcessW(wAmiExe, amiArgs, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (hNull) CloseHandle(hNull);
        printf("   AMI driver loaded.\n");
    } else {
        printf("   Skipped.\n");
    }
    printf("\n");

    DismountESP(espDrive);

    printf("Finished!\n");
    printf("Restart your PC to finish.\n\n");
    Beep(800, 200);
    Sleep(100);
    Beep(1000, 300);
    Sleep(200);
    Beep(1200, 500);
    printf("Press Enter to exit.");
    getchar(); getchar();

    printf("\nCleaning up temporary files...\n");
    char delCmd[MAX_PATH];
    sprintf_s(delCmd, "rmdir /s /q \"%s\"", TempDir);
    RunCmd(std::wstring(delCmd, delCmd + strlen(delCmd)).c_str());
    printf("Done.\n");
    return 0;
}
