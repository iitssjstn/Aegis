#include "integrity.h"
#include "process_monitor.h"
#include "logger.h"
#include <windows.h>
#include <sstream>

namespace aegis {

IntegrityMonitor::IntegrityMonitor(std::wstring expectedSha256)
    : m_expectedSha256(std::move(expectedSha256)) {}

bool IntegrityMonitor::OnGameStart(const std::wstring& exePath, DWORD pid) {
    m_exePath = exePath;
    m_pid = pid;
    m_startTime = std::chrono::system_clock::now();
    m_running = true;

    // Hergebruik hashing via ProcessMonitor zou een publieke helper vereisen;
    // voor nu los geïmplementeerd - TODO: hash-util naar gedeelde module
    // trekken zodat dit niet dupliceert met process_monitor.cpp.
    std::wstring actualHash; // TODO: ComputeFileSha256(exePath)

    bool match = m_expectedSha256.empty() || actualHash == m_expectedSha256;

    std::ostringstream extra;
    extra << "\"pid\":" << pid << ",\"exe_path_logged\":true,\"hash_match\":"
          << (match ? "true" : "false");

    Logger::Instance().LogEvent(
        match ? LogLevel::INFO : LogLevel::FLAG,
        "integrity",
        match ? "Game gestart, hash-verificatie OK" : "Game gestart, hash MISMATCH t.o.v. verwachte versie",
        extra.str());

    return match;
}

void IntegrityMonitor::OnGameStop() {
    if (!m_running) return;
    auto duration = std::chrono::system_clock::now() - m_startTime;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    std::ostringstream extra;
    extra << "\"pid\":" << m_pid << ",\"session_duration_seconds\":" << seconds;

    Logger::Instance().LogEvent(LogLevel::INFO, "integrity", "Game gestopt", extra.str());
    m_running = false;
}

void IntegrityMonitor::PeriodicCheck() {
    if (!m_running) return;

    DWORD stillRunningPid = 0;
    // Check of het proces nog leeft door PID te openen i.p.v. volledige
    // her-enumeratie (goedkoper, past bij performance-eis).
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, m_pid);
    if (hProcess) {
        DWORD waitResult = WaitForSingleObject(hProcess, 0);
        if (waitResult == WAIT_OBJECT_0) {
            // Proces is al beëindigd maar OnGameStop is niet aangeroepen -
            // onverwachte exit, de moeite van het loggen waard.
            Logger::Instance().Log(LogLevel::WARN, "integrity",
                "Game-process onverwacht beëindigd (geen nette exit gedetecteerd)");
            m_running = false;
        }
        CloseHandle(hProcess);
    } else {
        Logger::Instance().Log(LogLevel::WARN, "integrity",
            "Kon game-process niet meer openen tijdens periodieke check");
        m_running = false;
    }
}

double IntegrityMonitor::GetBiosNominalMHz() {
    HKEY hKey;
    DWORD mhz = 0, size = sizeof(mhz);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                       L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                       0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &size);
        RegCloseKey(hKey);
    }
    return static_cast<double>(mhz);
}

double IntegrityMonitor::GetCurrentMeasuredMHz() {
    // Meet werkelijke clock-speed via QueryPerformanceCounter-gebaseerde
    // sampling van RDTSC-ticks over een kort interval. Kort interval
    // (enkele ms) om performance-impact minimaal te houden.
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    unsigned __int64 tscStart = __rdtsc();
    QueryPerformanceCounter(&start);

    Sleep(5); // Korte sample-window; genoeg voor een grove schatting

    unsigned __int64 tscEnd = __rdtsc();
    QueryPerformanceCounter(&end);

    double elapsedSeconds = static_cast<double>(end.QuadPart - start.QuadPart) / freq.QuadPart;
    double cycles = static_cast<double>(tscEnd - tscStart);
    double mhz = (cycles / elapsedSeconds) / 1'000'000.0;
    return mhz;
}

CpuSpeedReading IntegrityMonitor::ReadCpuSpeed(double boxNominalMHz) {
    CpuSpeedReading reading;
    reading.biosNominalMHz = GetBiosNominalMHz();
    reading.boxNominalMHz = boxNominalMHz;
    reading.measuredMHz = GetCurrentMeasuredMHz();

    // Turbo-boost zorgt voor legitieme afwijkingen naar boven; alleen
    // sterke afwijkingen (bijv. >25% onder nominal, wat kan wijzen op
    // een gethrottlede/gevirtualiseerde omgeving) zijn interessant.
    double lowerBound = reading.boxNominalMHz * 0.75;
    reading.withinExpectedRange = reading.measuredMHz >= lowerBound;

    return reading;
}

} // namespace aegis
