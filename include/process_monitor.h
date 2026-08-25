#pragma once
#include <string>
#include <vector>

namespace aegis {

struct ProcessRecord {
    DWORD pid = 0;
    std::wstring name;
    std::wstring fullPath;
    std::wstring sha256;          // Hash van het exe-bestand op disk
    bool isSigned = false;        // Authenticode-signature geldig?
    std::wstring signerName;      // Bijv. "Microsoft Windows", leeg indien ongesigneerd
};

class ProcessMonitor {
public:
    // Snapshot van alle lopende processen op het moment van aanroep.
    // Bedoeld om bij gamestart één keer volledig te loggen (zoals in de
    // featurelijst), NIET continu te pollen (performance-eis).
    static std::vector<ProcessRecord> SnapshotRunningProcesses();

    // Voor los gebruik: alleen het target-game-process opzoeken op naam.
    static bool FindProcessByName(const std::wstring& exeName, DWORD& outPid);

private:
    static std::wstring GetProcessFullPath(HANDLE hProcess);
    static std::wstring ComputeFileSha256(const std::wstring& filePath);
    static bool VerifyAuthenticode(const std::wstring& filePath, std::wstring& outSignerName);
};

} // namespace aegis
