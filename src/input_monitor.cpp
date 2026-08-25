#include "input_monitor.h"
#include <cmath>
#include <numeric>

namespace aegis {

static uint64_t NowMs() {
    return GetTickCount64();
}

LRESULT CALLBACK InputMonitor::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        InputEvent evt{};
        evt.timestampMs = NowMs();
        evt.vkCode = static_cast<WORD>(kb->vkCode);
        evt.isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        evt.isMouseEvent = false;

        {
            // Kritieke sectie zo kort mogelijk houden - dit draait in de
            // systeembrede input-pipeline, elke ms telt voor input-lag.
            std::lock_guard<std::mutex> lock(s_mutex);
            s_events.push_back(evt);
            if (s_events.size() > kMaxBufferedEvents) s_events.pop_front();
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK InputMonitor::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE) {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        InputEvent evt{};
        evt.timestampMs = NowMs();
        evt.isMouseEvent = true;
        // Alleen delta-achtige info nodig voor patroon-analyse, geen
        // absolute schermposities loggen (niet relevant + onnodig precies).
        static LONG lastX = ms->pt.x, lastY = ms->pt.y;
        evt.mouseDx = ms->pt.x - lastX;
        evt.mouseDy = ms->pt.y - lastY;
        lastX = ms->pt.x;
        lastY = ms->pt.y;

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_events.push_back(evt);
            if (s_events.size() > kMaxBufferedEvents) s_events.pop_front();
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool InputMonitor::Start() {
    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                        GetModuleHandleW(nullptr), 0);
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc,
                                     GetModuleHandleW(nullptr), 0);
    return m_keyboardHook && m_mouseHook;
}

void InputMonitor::Stop() {
    if (m_keyboardHook) { UnhookWindowsHookEx(m_keyboardHook); m_keyboardHook = nullptr; }
    if (m_mouseHook) { UnhookWindowsHookEx(m_mouseHook); m_mouseHook = nullptr; }
    std::lock_guard<std::mutex> lock(s_mutex);
    s_events.clear();
}

MacroSuspicionStats InputMonitor::AnalyzeKey(WORD vkCode, size_t windowSize) {
    MacroSuspicionStats stats;
    std::vector<uint64_t> timestamps;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (auto it = s_events.rbegin(); it != s_events.rend() && timestamps.size() < windowSize; ++it) {
            if (!it->isMouseEvent && it->vkCode == vkCode && it->isKeyDown) {
                timestamps.push_back(it->timestampMs);
            }
        }
    }

    if (timestamps.size() < 3) return stats; // Te weinig data om iets te zeggen

    std::vector<double> intervals;
    for (size_t i = 0; i + 1 < timestamps.size(); ++i) {
        intervals.push_back(static_cast<double>(timestamps[i] - timestamps[i + 1]));
    }

    double sum = std::accumulate(intervals.begin(), intervals.end(), 0.0);
    stats.avgIntervalMs = sum / intervals.size();

    double sqSum = 0.0;
    for (double v : intervals) sqSum += (v - stats.avgIntervalMs) * (v - stats.avgIntervalMs);
    stats.stddevIntervalMs = std::sqrt(sqSum / intervals.size());
    stats.repeatCount = static_cast<int>(timestamps.size());

    // TODO: threshold voor "verdacht consistent" moet gekalibreerd worden
    // op echte speler-data (menselijke variatie verschilt sterk per
    // persoon/actie). Placeholder: stddev < 3ms bij >20 herhalingen.
    stats.suspiciouslyConsistent = (stats.stddevIntervalMs < 3.0 && stats.repeatCount > 20);

    return stats;
}

MacroSuspicionStats InputMonitor::AnalyzeMouseMovement(size_t windowSize) {
    MacroSuspicionStats stats;
    // TODO: implementeer verticale-compensatie-patroon-detectie voor
    // no-recoil (vereist correlatie met in-game recoil-timing, wat
    // buiten pure input-monitoring valt - zie project-README voor de
    // discussie over wat user-mode wel/niet kan zien).
    (void)windowSize;
    return stats;
}

} // namespace aegis
