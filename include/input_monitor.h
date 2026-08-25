#pragma once
#include <windows.h>
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>

namespace aegis {

// Eén input-event met timestamp, voor timing-analyse. Bewust GEEN
// tekstuele inhoud (geen keylogging van getypte tekst) - alleen
// virtual-key code + timing, wat genoeg is voor macro/timing-patronen.
struct InputEvent {
    uint64_t timestampMs;
    WORD vkCode;      // Voor toetsenbord-events
    bool isKeyDown;
    bool isMouseEvent;
    int mouseDx, mouseDy; // Voor mouse-move delta's (no-recoil/aim-analyse)
};

// Statistiek-samenvatting over een venster van events - dit is wat
// daadwerkelijk gelogd/gerapporteerd wordt, niet de raw event-stream
// (privacy: geen volledige input-capture bewaren).
struct MacroSuspicionStats {
    double avgIntervalMs = 0.0;
    double stddevIntervalMs = 0.0;   // Lage stddev = zeer consistente timing = verdacht
    int repeatCount = 0;
    bool suspiciouslyConsistent = false; // TODO: threshold afstemmen met echte data
};

// Houdt een rolling window van recente input-events bij via low-level
// hooks. De hooks zelf zijn minimaal (alleen event pushen naar een
// lock-protected deque) om geen input-lag te introduceren.
class InputMonitor {
public:
    bool Start();
    void Stop();

    // Analyseert het huidige venster op een specifieke virtual-key
    // (bijv. voor "bunny hop" op spatie, of een aim-key).
    MacroSuspicionStats AnalyzeKey(WORD vkCode, size_t windowSize = 200);

    // Analyseert mouse-movement patterns (voor no-recoil: verticale
    // compensatie-patches die te perfect anti-correleren met recoil-timing).
    MacroSuspicionStats AnalyzeMouseMovement(size_t windowSize = 200);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    static inline std::mutex s_mutex;
    static inline std::deque<InputEvent> s_events;
    static constexpr size_t kMaxBufferedEvents = 5000; // Cap: geen onbegrensde groei

    HHOOK m_keyboardHook = nullptr;
    HHOOK m_mouseHook = nullptr;
};

} // namespace aegis
