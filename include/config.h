#pragma once
#include <string>
#include <vector>

namespace aegis {

// Config bepaalt WAT er verzameld wordt en HOE VAAK. Bewust apart van de
// modules zelf zodat je scope/intervals kan aanpassen zonder code-changes,
// en zodat het consent-scherm exact kan tonen wat er aan staat.
struct AppConfig {
    // --- Scope / consent ---
    bool consentGiven = false;          // Moet expliciet true zijn voor start
    std::wstring targetGameExeName;     // bijv. L"game.exe" - alleen dit proces wordt gemonitord
    std::wstring targetGameExeSha256;   // Verwachte hash, voor integrity-check

    // --- Intervallen (ms) - bewust ruim ingesteld i.v.m. performance-eis ---
    int screenshotIntervalMinMs = 60'000;
    int screenshotIntervalMaxMs = 300'000;
    int integrityCheckIntervalMs = 15'000;
    int deviceEnumIntervalMs = 30'000;
    int cpuSpeedCheckIntervalMs = 20'000;

    // --- Data retention (lokaal) ---
    int retainNonFlaggedScreenshotsMinutes = 10; // Zie privacy-notitie in README
    int retainLogsDays = 30;

    // --- Paden ---
    std::wstring logDirectory = L"logs";
    std::wstring screenshotDirectory = L"logs\\screenshots";
    std::wstring blocklistPath = L"config\\dma_blocklist.json";

    static AppConfig LoadFromFile(const std::wstring& path);
    bool SaveToFile(const std::wstring& path) const;
};

} // namespace aegis
