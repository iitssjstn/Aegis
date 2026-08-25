#pragma once
#include <string>

namespace aegis {

class ScreenshotCapture {
public:
    // Maakt een schermafbeelding en slaat lokaal op als PNG.
    // Retourneert het volledige pad, of leeg bij falen.
    // reason: "random" of "prtscn" - voor logging/retention-logica.
    static std::wstring CaptureToFile(const std::wstring& outputDirectory,
                                       const std::wstring& reason);

private:
    static bool SaveHBitmapAsPng(HBITMAP hBitmap, const std::wstring& filePath);
};

// Luistert op systeem-niveau naar PrtScn via een low-level keyboard hook.
// Draait op een eigen lichte thread; de hook-callback zelf doet zo min
// mogelijk werk (alleen een flag zetten) om geen input-latency te
// introduceren - vereist voor de performance-eis.
class PrtScnWatcher {
public:
    using CaptureCallback = void(*)();

    bool Start(CaptureCallback onPrtScnPressed);
    void Stop();

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static inline CaptureCallback s_callback = nullptr;
    HHOOK m_hook = nullptr;
};

} // namespace aegis
