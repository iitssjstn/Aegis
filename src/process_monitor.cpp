#include "process_monitor.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "bcrypt.lib")

namespace aegis {

std::wstring ProcessMonitor::GetProcessFullPath(HANDLE hProcess) {
    wchar_t path[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        return std::wstring(path);
    }
    return L"";
}

std::wstring ProcessMonitor::ComputeFileSha256(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.good()) return L"";

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::wstring result;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return result;
    }

    DWORD hashObjLen = 0, cbData = 0, hashLen = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjLen), sizeof(DWORD), &cbData, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(DWORD), &cbData, 0);
    std::vector<UCHAR> hashObj(hashObjLen);
    std::vector<UCHAR> hash(hashLen);

    if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjLen, nullptr, 0, 0) == 0) {
        // Bestand in chunks lezen i.p.v. volledig in memory - ook grote
        // exe's mogen geen noemenswaardige memory-spike geven.
        std::vector<char> buffer(1 << 16);
        while (file.good()) {
            file.read(buffer.data(), buffer.size());
            std::streamsize read = file.gcount();
            if (read > 0) {
                BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buffer.data()),
                                static_cast<ULONG>(read), 0);
            }
        }
        BCryptFinishHash(hHash, hash.data(), hashLen, 0);
        std::wostringstream oss;
        for (UCHAR b : hash) {
            oss << std::hex << std::setw(2) << std::setfill(L'0') << static_cast<int>(b);
        }
        result = oss.str();
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

bool ProcessMonitor::VerifyAuthenticode(const std::wstring& filePath, std::wstring& outSignerName) {
    WINTRUST_FILE_INFO fileInfo = {0};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = filePath.c_str();

    GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trustData = {0};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE; // Offline-vriendelijk; geen netwerk-call per proces
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    LONG status = WinVerifyTrust(nullptr, &policyGuid, &trustData);

    // Cleanup state (vereist door WinVerifyTrust contract)
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGuid, &trustData);

    if (status == ERROR_SUCCESS) {
        // TODO: signer common name extraheren via CertGetNameString op de
        // cert-chain (CryptQueryObject). Voor nu: alleen geldig/ongeldig.
        outSignerName = L"(verified, signer-name extraction TODO)";
        return true;
    }
    outSignerName = L"";
    return false;
}

std::vector<ProcessRecord> ProcessMonitor::SnapshotRunningProcesses() {
    std::vector<ProcessRecord> results;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return results;

    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &entry)) {
        do {
            ProcessRecord record;
            record.pid = entry.th32ProcessID;
            record.name = entry.szExeFile;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (hProcess) {
                record.fullPath = GetProcessFullPath(hProcess);
                CloseHandle(hProcess);

                if (!record.fullPath.empty()) {
                    record.sha256 = ComputeFileSha256(record.fullPath);
                    record.isSigned = VerifyAuthenticode(record.fullPath, record.signerName);
                }
            }
            results.push_back(record);
        } while (Process32NextW(hSnapshot, &entry));
    }

    CloseHandle(hSnapshot);
    return results;
}

bool ProcessMonitor::FindProcessByName(const std::wstring& exeName, DWORD& outPid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(PROCESSENTRY32W);
    bool found = false;

    if (Process32FirstW(hSnapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, exeName.c_str()) == 0) {
                outPid = entry.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &entry));
    }

    CloseHandle(hSnapshot);
    return found;
}

} // namespace aegis
