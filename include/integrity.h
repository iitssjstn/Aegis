#pragma once
#include <string>
#include <chrono>

namespace aegis {

struct CpuSpeedReading {
    double measuredMHz = 0.0;
    double biosNominalMHz = 0.0;   // Uit registry (~MHz waarde)
    double boxNominalMHz = 0.0;    // Uit config (opgegeven CPU-model nominale speed)
    bool withinExpectedRange = true;
};

// Bewaakt de integriteit van het game-process: verwachte hash, of het
// process nog draait, en periodieke checks (niet elke frame - performance-eis).
class IntegrityMonitor {
public:
    explicit IntegrityMonitor(std::wstring expectedSha256);

    // Zet start-tijdstip en verifieert exe-hash tegen expectedSha256.
    // Retourneert false als de hash niet matcht (mogelijk gepatchte exe).
    bool OnGameStart(const std::wstring& exePath, DWORD pid);

    void OnGameStop();

    // Lichte periodieke check: leeft het proces nog, klopt de hash nog
    // (her-lezen van het bestand op disk detecteert geen in-memory
    // patches; dat vereist proces-memory reads, zie project-README
    // over de grenzen van user-mode detectie).
    void PeriodicCheck();

    static CpuSpeedReading ReadCpuSpeed(double boxNominalMHz);

private:
    std::wstring m_expectedSha256;
    std::wstring m_exePath;
    DWORD m_pid = 0;
    std::chrono::system_clock::time_point m_startTime;
    bool m_running = false;

    static double GetBiosNominalMHz();
    static double GetCurrentMeasuredMHz();
};

} // namespace aegis
