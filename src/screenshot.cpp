#include "screenshot.h"
#include "logger.h"
#include <windows.h>
#include <gdiplus.h>
#include <filesystem>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

namespace aegis {

// Helper om de PNG-encoder CLSID te vinden (GDI+ vereist dit expliciet)
static bool GetPngEncoderClsid(CLSID& clsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;

    std::vector<BYTE> buffer(size);
    auto* imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(num, size, imageCodecInfo);

    for (UINT i = 0; i < num; ++i) {
        if (std::wstring(imageCodecInfo[i].MimeType) == L"image/png") {
            clsid = imageCodecInfo[i].Clsid;
            return true;
        }
    }
    return false;
}

bool ScreenshotCapture::SaveHBitmapAsPng(HBITMAP hBitmap, const std::wstring& filePath) {
    Gdiplus::Bitmap bitmap(hBitmap, nullptr);
    CLSID pngClsid;
    if (!GetPngEncoderClsid(pngClsid)) return false;

    Gdiplus::Status status = bitmap.Save(filePath.c_str(), &pngClsid, nullptr);
    return status == Gdiplus::Ok;
}

std::wstring ScreenshotCapture::CaptureToFile(const std::wstring& outputDirectory,
                                               const std::wstring& reason) {
    std::error_code ec;
    std::filesystem::create_directories(outputDirectory, ec);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

    BitBlt(hMemDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY | CAPTUREBLT);

    SYSTEMTIME st;
    GetSystemTime(&st);
    std::wostringstream fileName;
    fileName << outputDirectory << L"\\" << reason << L"_"
             << st.wYear << st.wMonth << st.wDay << L"_"
             << st.wHour << st.wMinute << st.wSecond << st.wMilliseconds << L".png";

    bool saved = SaveHBitmapAsPng(hBitmap, fileName.str());

    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    if (!saved) return L"";

    std::string reasonNarrow(reason.begin(), reason.end());
    Logger::Instance().Log(LogLevel::INFO, "screenshot",
        "Screenshot vastgelegd (reden: " + reasonNarrow + ")");

    return fileName.str();
}

LRESULT CALLBACK PrtScnWatcher::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb->vkCode == VK_SNAPSHOT && s_callback) {
            // Bewust: callback direct aanroepen maar de callback zelf moet
            // licht blijven (bijv. alleen een event posten naar een queue)
            // om de systeembrede input-pipeline niet te vertragen.
            s_callback();
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool PrtScnWatcher::Start(CaptureCallback onPrtScnPressed) {
    s_callback = onPrtScnPressed;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                GetModuleHandleW(nullptr), 0);
    return m_hook != nullptr;
}

void PrtScnWatcher::Stop() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    s_callback = nullptr;
}

} // namespace aegis
