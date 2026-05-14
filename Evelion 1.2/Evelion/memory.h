#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string_view>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

class Memory
{
private:
    std::uintptr_t processId = 0;
    HANDLE processHandle = nullptr;
    std::ofstream logFile;

    // تابع داخلی برای نوشتن لاگ‌ها با زمان دقیق
    void Log(const std::string& message) const {
        if (logFile.is_open()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timeBuf;
            sprintf_s(timeBuf, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            const_cast<std::ofstream&>(logFile) << timeBuf << message << std::endl;
            const_cast<std::ofstream&>(logFile).flush();
        }
    }

public:
    Memory(std::string_view processName) noexcept
    {
        // ساخت فایل لاگ در کنار چیت
        logFile.open("Evelion_DeepDebug.log", std::ios::out | std::ios::trunc);
        Log("=== EVELION DEEP MEMORY DIAGNOSTIC INITIATED ===");

        ::PROCESSENTRY32 entry = { };
        entry.dwSize = sizeof(::PROCESSENTRY32);
        const HANDLE snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        bool found = false;
        while (::Process32Next(snapShot, &entry))
        {
            if (!processName.compare(entry.szExeFile))
            {
                processId = entry.th32ProcessID;
                processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
                
                if (processHandle) {
                    Log("SUCCESS: Attached to " + std::string(processName) + " (PID: " + std::to_string(processId) + ")");
                } else {
                    Log("FATAL: Found process, but OpenProcess failed! Anti-Cheat is actively blocking handle creation. Error Code: " + std::to_string(GetLastError()));
                }
                found = true;
                break;
            }
        }

        if (!found) Log("ERROR: Process " + std::string(processName) + " not found in Windows Process List.");
        if (snapShot) ::CloseHandle(snapShot);
    }

    ~Memory()
    {
        Log("=== DIAGNOSTIC SESSION ENDED ===");
        if (logFile.is_open()) logFile.close();
        if (processHandle) ::CloseHandle(processHandle);
    }

    // این موتور جستجوی جدید و قدرتمند ماست
    std::uintptr_t PatternScan(const char* signature, const char* name_for_log) const {
        Log(std::string("--- Starting Deep Scan for: ") + name_for_log + " ---");
        
        if (!processHandle) {
            Log("ABORT: No process handle available.");
            return 0;
        }

        // تبدیل پترن به فرمت قابل جستجو
        std::vector<int> patternBytes;
        auto bytes = const_cast<char*>(signature);
        auto start = bytes;
        auto end = bytes + strlen(signature);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') ++current;
                patternBytes.push_back(-1); // -1 یعنی هر بایتی بود قبول کن (Wildcard)
            } else {
                patternBytes.push_back(strtoul(current, &current, 16));
            }
        }

        // جستجو در کل فضای رم بازی (از آدرس 0 تا 2 گیگابایت)
        MEMORY_BASIC_INFORMATION mbi;
        std::uintptr_t currentAddress = 0;
        size_t bytesRead;
        std::vector<uint8_t> buffer;

        while (VirtualQueryEx(processHandle, (LPCVOID)currentAddress, &mbi, sizeof(mbi))) {
            // فقط صفحاتی که مربوط به خود بازی هستن و قابل خوندن هستن رو چک کن
            if (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS && !(mbi.Protect & PAGE_GUARD)) {
                
                buffer.resize(mbi.RegionSize);
                
                if (ReadProcessMemory(processHandle, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead)) {
                    
                    if (bytesRead == 0) {
                        Log("SUSPICIOUS: ReadProcessMemory succeeded but returned 0 bytes at region: " + std::to_string((std::uintptr_t)mbi.BaseAddress));
                    } else {
                        // جستجوی الگو داخل این صفحه از رم
                        for (size_t i = 0; i < bytesRead - patternBytes.size(); ++i) {
                            bool found = true;
                            for (size_t j = 0; j < patternBytes.size(); ++j) {
                                if (patternBytes[j] != -1 && buffer[i + j] != patternBytes[j]) {
                                    found = false;
                                    break;
                                }
                            }
                            if (found) {
                                std::uintptr_t resultAddr = (std::uintptr_t)mbi.BaseAddress + i;
                                std::stringstream ss;
                                ss << std::hex << resultAddr;
                                Log("BINGO: Pattern [" + std::string(name_for_log) + "] found at Address: 0x" + ss.str());
                                return resultAddr;
                            }
                        }
                    }
                } else {
                    // اینجا مچ آنتی‌چیت گرفته میشه!
                    DWORD err = GetLastError();
                    if (err == 299) { // ERROR_PARTIAL_COPY
                        // این ارور یعنی آنتی چیت یک بایت خاص رو قفل کرده
                        // Log("INFO: Partial read at region: " + std::to_string((std::uintptr_t)mbi.BaseAddress) + " (Likely Anti-Cheat Page Guarding)");
                    } else {
                        Log("WARNING: RPM Failed at region: " + std::to_string((std::uintptr_t)mbi.BaseAddress) + ". Error Code: " + std::to_string(err));
                    }
                }
            }
            currentAddress = (std::uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }

        Log("FAILED: Pattern [" + std::string(name_for_log) + "] could not be found anywhere in memory.");
        return 0;
    }

    // متدهای استاندارد خوندن رم
    template <typename T>
    const T Read(const std::uintptr_t address) const noexcept
    {
        T value = { };
        ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
        return value;
    }

    // بازنویسی تابع قدیمی برای سازگاری موقت و لاگ‌گیری
    std::uintptr_t GetModuleAddress(std::string_view moduleName) const noexcept
    {
        Log("ATTEMPT: main.cpp requested Module Address for: " + std::string(moduleName));
        Log("INFO: Returning 0 to force bypass of Windows PEB and move to Pattern Scanning.");
        return 0; // ما عمداً 0 برمی‌گردونیم تا روش قدیمی غیرفعال بشه
    }
};
