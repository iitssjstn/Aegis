#include "hwid.h"
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")

namespace aegis {

std::wstring HwidFingerprint::GetMachineGuid() {
    HKEY hKey;
    wchar_t buffer[64] = {0};
    DWORD bufferSize = sizeof(buffer);
    std::wstring result;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
                       0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr,
                              reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS) {
            result = buffer;
        }
        RegCloseKey(hKey);
    }
    return result;
}

std::wstring HwidFingerprint::GetCpuIdString() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];

    std::wostringstream oss;
    oss << std::hex << nIds;

    __cpuid(cpuInfo, 1);
    oss << L"-" << cpuInfo[0] << L"-" << cpuInfo[3];

    // Brand string (niet uniek per chip, maar draagt bij aan de fingerprint
    // i.c.m. andere velden)
    int brand[12];
    __cpuid(brand, 0x80000002);
    __cpuid(brand + 4, 0x80000003);
    __cpuid(brand + 8, 0x80000004);
    oss << L"-" << reinterpret_cast<char*>(brand);

    return oss.str();
}

std::wstring HwidFingerprint::GetSystemVolumeSerial() {
    DWORD serial = 0;
    if (GetVolumeInformationW(L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
        std::wostringstream oss;
        oss << std::hex << serial;
        return oss.str();
    }
    return L"";
}

std::wstring HwidFingerprint::GetBiosSerial() {
    // TODO: BIOS serial via WMI (Win32_BIOS.SerialNumber) - vereist COM/WMI
    // init die we delen met av_status.cpp. Voor nu leeg; wordt in de WMI-
    // helper (zie av_status module) gecentraliseerd zodra die af is.
    return L"";
}

std::string HwidFingerprint::Sha256Hex(const std::wstring& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return result;
    }

    DWORD hashObjLen = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjLen),
                       sizeof(DWORD), &cbData, 0);
    std::vector<UCHAR> hashObj(hashObjLen);

    DWORD hashLen = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen),
                       sizeof(DWORD), &cbData, 0);
    std::vector<UCHAR> hash(hashLen);

    if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjLen, nullptr, 0, 0) == 0) {
        std::string narrow(input.begin(), input.end());
        BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(narrow.data())),
                        static_cast<ULONG>(narrow.size()), 0);
        BCryptFinishHash(hHash, hash.data(), hashLen, 0);

        std::ostringstream oss;
        for (UCHAR b : hash) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        result = oss.str();
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

std::string HwidFingerprint::Generate(const std::string& salt) {
    std::wstring combined = GetMachineGuid() + L"|" + GetCpuIdString() + L"|" +
                             GetSystemVolumeSerial() + L"|" + GetBiosSerial() + L"|" +
                             std::wstring(salt.begin(), salt.end());
    return Sha256Hex(combined);
}

} // namespace aegis
