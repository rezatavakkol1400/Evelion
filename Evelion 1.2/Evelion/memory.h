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

    // Use std::ostringstream to safely format the time, avoiding sprintf_s compiler errors
    void Log(const std::string& message) const {
        if (logFile.is_open()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            
            std::ostringstream timeStream;
            timeStream << "[" 
                       << std::setfill('0') << std::setw(2) << st.wHour << ":"
                       << std::setfill('0') << std::setw(2) << st.wMinute << ":"
                       << std::setfill('0') << std::setw(2) << st.wSecond << "."
                       << std::setfill('0') << std::setw(3) << st.wMilliseconds << "] ";
                       
            const_cast<std::ofstream&>(logFile) << timeStream.str() << message << std::endl;
            const_cast<std::ofstream&>(logFile).flush();
        }
    }

public:
    Memory(std::string_view processName) noexcept
    {
        logFile.open("Evelion_DeepDebug.log", std::ios::out | std::ios::trunc);
        Log("=== EVELION DEEP MEMORY DIAGNOSTIC INITIATED ===");

        ::PROCESSENTRY32 entry = { };
        entry.dwSize = sizeof(::PROCESSENTRY32);
        const HANDLE snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        bool found = false;
        if (snapShot != INVALID_HANDLE_VALUE) {
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
            ::CloseHandle(snapShot);
        }

        if (!found) Log("ERROR: Process " + std::string(processName) + " not found in Windows Process List.");
    }

    ~Memory()
    {
        Log("=== DIAGNOSTIC SESSION ENDED ===");
        if (logFile.is_open()) logFile.close();
        if (processHandle) ::CloseHandle(processHandle);
    }

    std::uintptr_t PatternScan(const char* signature, const char* name_for_log) const {
        Log(std::string("--- Starting Deep Scan for: ") + name_for_log + " ---");
        
        if (!processHandle) {
            Log("ABORT: No process handle available.");
            return 0;
        }

        std::vector<int> patternBytes;
        auto bytes = const_cast<char*>(signature);
        auto start = bytes;
        auto end = bytes + strlen(signature);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') ++current;
                patternBytes.push_back(-1); 
            } else {
                patternBytes.push_back(strtoul(current, &current, 16));
            }
        }

        MEMORY_BASIC_INFORMATION mbi;
        std::uintptr_t currentAddress = 0;
        SIZE_T bytesRead; 
        std::vector<uint8_t> buffer;

        while (VirtualQueryEx(processHandle, (LPCVOID)currentAddress, &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS && !(mbi.Protect & PAGE_GUARD)) {
                
                buffer.resize(mbi.RegionSize);
                
                if (ReadProcessMemory(processHandle, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead)) {
                    
                    if (bytesRead == 0) {
                        Log("SUSPICIOUS: ReadProcessMemory succeeded but returned 0 bytes at region: " + std::to_string((std::uintptr_t)mbi.BaseAddress));
                    } else {
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
                    DWORD err = GetLastError();
                    if (err != 299) { 
                        Log("WARNING: RPM Failed at region: " + std::to_string((std::uintptr_t)mbi.BaseAddress) + ". Error Code: " + std::to_string(err));
                    }
                }
            }
            currentAddress = (std::uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }

        Log("FAILED: Pattern [" + std::string(name_for_log) + "] could not be found anywhere in memory.");
        return 0;
    }

    template <typename T>
    const T Read(const std::uintptr_t address) const noexcept
    {
        T value = { };
        ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
        return value;
    }

    // ==========================================
    // Core Functions needed by main.cpp
    // ==========================================

    bool ReadModuleMemory(std::string_view moduleName, void* buffer, size_t size) const noexcept
    {
        std::uintptr_t moduleBase = GetModuleAddress(moduleName);
        if (moduleBase == 0) return false;
        return ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(moduleBase), buffer, size, nullptr) != 0;
    }

    bool ReadHugeMemory(std::uintptr_t moduleBase, void* buffer, size_t size) const noexcept
    {
        if (moduleBase == 0) return false;
        return ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(moduleBase), buffer, size, nullptr) != 0;
    }

    // Generic ReadModuleBuffer matching the expected signature in Evelion
    template <typename T>
    const T ReadModuleBuffer(std::uintptr_t address) const noexcept
    {
        T value = { };
        ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
        return value;
    }

    std::uintptr_t GetModuleAddress(std::string_view moduleName) const noexcept
    {
        Log("ATTEMPT: main.cpp requested Module Address for: " + std::string(moduleName));
        Log("INFO: Returning 0 to force bypass of Windows PEB and move to Pattern Scanning.");
        return 0; 
    }
};
