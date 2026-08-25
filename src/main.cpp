#include "logger.h"
#include "config.h"
#include "hwid.h"
#include "process_monitor.h"
#include "integrity.h"
#include "screenshot.h"
#include "input_monitor.h"
#include "device_enum.h"
#include "av_status.h"

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>

using namespace aegis;

static std::atomic<bool> g_shouldCapturePrtScn{false};

static void OnPrtScnPressed() {
    // Hook-callback blijft licht: alleen een flag zetten. De main-loop
    // pakt hem op en doet het eigenlijke (duurdere) capture-werk.
    g_shouldCapturePrtScn.store(true, std::memory_order_relaxed);
}

// Toont wat er verzameld wordt en vraagt expliciete toestemming.
// TODO: dit wordt in de echte tournament-client een fatsoenlijk UI-scherm
// i.p.v. console-input - console hier puur voor het skeleton/testen.
static bool ShowConsentScreenAndGetApproval() {
    std::wcout << L"=== Aegis Anti-Cheat - Toestemming ===\n"
                << L"Tijdens dit tournament verzamelt Aegis het volgende, ALLEEN gericht op\n"
                << L"het spelproces en de tournament-sessie:\n"
                << L"  - Willekeurige + PrtScn-screenshots (lokaal bewaard, tenzij geflagged)\n"
                << L"  - Gehashte hardware-fingerprint (geen raw serials)\n"
                << L"  - Processen actief bij gamestart (naam/hash/handtekening)\n"
                << L"  - Antivirus-status (naam + actief/inactief)\n"
                << L"  - PCIe-apparaten (alleen gematcht tegen bekende DMA-cheat-hardware)\n"
                << L"  - Input-timingpatronen (GEEN tekstinhoud, geen keylogging)\n"
                << L"Alles wordt lokaal gelogd. Niet-geflagde data wordt na de sessie verwijderd.\n\n"
                << L"Akkoord? (j/n): ";

    wchar_t answer = 0;
    std::wcin >> answer;
    return answer == L'j' || answer == L'J';
}

int wmain() {
    if (!ShowConsentScreenAndGetApproval()) {
        std::wcout << L"Geen toestemming gegeven - Aegis start niet.\n";
        return 0;
    }

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    AppConfig config = AppConfig::LoadFromFile(L"config\\aegis_config.json");
    config.consentGiven = true;

    SYSTEMTIME st;
    GetSystemTime(&st);
    std::wostringstream logFileName;
    logFileName << config.logDirectory << L"\\session_"
                << st.wYear << st.wMonth << st.wDay << L"_"
                << st.wHour << st.wMinute << st.wSecond << L".jsonl";

    if (!Logger::Instance().Init(logFileName.str())) {
        std::wcerr << L"Kon logbestand niet openen, stoppen.\n";
        return 1;
    }

    Logger::Instance().Log(LogLevel::INFO, "main", "Aegis Anti-Cheat gestart, consent gegeven");

    // --- HWID fingerprint (eenmalig bij start) ---
    // TODO: salt moet per-tournament vanuit config/backend komen zodra
    // reporting bestaat, i.p.v. hardcoded placeholder.
    std::string hwid = HwidFingerprint::Generate("aegis-tournament-2026-placeholder-salt");
    Logger::Instance().LogEvent(LogLevel::INFO, "hwid", "Hardware-fingerprint gegenereerd",
        "\"hwid_hash\":\"" + hwid + "\"");

    // --- Process/AV snapshot bij start ---
    auto processes = ProcessMonitor::SnapshotRunningProcesses();
    std::ostringstream procExtra;
    procExtra << "\"process_count\":" << processes.size();
    Logger::Instance().LogEvent(LogLevel::INFO, "process_monitor",
        "Process-snapshot bij start voltooid", procExtra.str());
    for (const auto& p : processes) {
        if (!p.isSigned) {
            std::string nameNarrow(p.name.begin(), p.name.end());
            Logger::Instance().Log(LogLevel::WARN, "process_monitor",
                "Ongesigneerd proces actief: " + nameNarrow);
        }
    }

    auto avProducts = AvStatus::QueryInstalledAntivirus();
    if (avProducts.empty()) {
        Logger::Instance().Log(LogLevel::WARN, "av_status", "Geen antivirus-product gedetecteerd");
    }

    // --- DMA device check ---
    auto pciDevices = DeviceEnumerator::EnumeratePciDevices(config.blocklistPath);
    auto dmaProtection = DeviceEnumerator::CheckDmaProtectionStatus();
    std::ostringstream dmaExtra;
    dmaExtra << "\"kernel_dma_protection_on\":" << (dmaProtection.kernelDmaProtectionOn ? "true" : "false");
    Logger::Instance().LogEvent(LogLevel::INFO, "device_enum", "DMA-protection status gecontroleerd", dmaExtra.str());

    // --- Input monitor + PrtScn watcher starten ---
    InputMonitor inputMonitor;
    inputMonitor.Start();

    PrtScnWatcher prtScnWatcher;
    prtScnWatcher.Start(OnPrtScnPressed);

    // --- Game-integriteit ---
    IntegrityMonitor integrity(config.targetGameExeSha256);
    DWORD gamePid = 0;
    bool gameRunning = ProcessMonitor::FindProcessByName(config.targetGameExeName, gamePid);
    if (gameRunning) {
        integrity.OnGameStart(config.targetGameExeName, gamePid);
    } else {
        Logger::Instance().Log(LogLevel::WARN, "main",
            "Target-game nog niet gevonden bij startup - wacht op main loop");
    }

    // --- Main loop ---
    // Bewust: alle zware taken (screenshots, PCI-enum, integrity-check)
    // draaien op eigen interval en op deze aparte thread, niet gekoppeld
    // aan de render/game-loop. Dit is de kern van de performance-eis.
    auto lastScreenshot = std::chrono::steady_clock::now();
    auto lastIntegrityCheck = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        auto now = std::chrono::steady_clock::now();

        if (g_shouldCapturePrtScn.exchange(false)) {
            ScreenshotCapture::CaptureToFile(config.screenshotDirectory, L"prtscn");
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScreenshot).count()
                >= config.screenshotIntervalMinMs) {
            ScreenshotCapture::CaptureToFile(config.screenshotDirectory, L"random");
            lastScreenshot = now;
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastIntegrityCheck).count()
                >= config.integrityCheckIntervalMs) {
            integrity.PeriodicCheck();
            lastIntegrityCheck = now;
        }

        // TODO: nette shutdown-trigger (bijv. game-exit detecteren i.p.v.
        // deze loop oneindig te laten draaien in het skeleton).
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Placeholder exit-conditie voor het skeleton:
        running = true; // vervang door echte shutdown-logica
        break; // TODO: verwijderen zodra shutdown-logica er is; voorkomt oneindige loop in skeleton-vorm
    }

    integrity.OnGameStop();
    inputMonitor.Stop();
    prtScnWatcher.Stop();
    Logger::Instance().Log(LogLevel::INFO, "main", "Aegis Anti-Cheat sessie afgesloten");
    Logger::Instance().Shutdown();

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
