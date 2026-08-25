#include "av_status.h"
#include "logger.h"
#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

namespace aegis {

std::vector<AvProductInfo> AvStatus::QueryInstalledAntivirus() {
    std::vector<AvProductInfo> results;

    HRESULT hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hres);
    if (FAILED(hres) && hres != RPC_E_CHANGED_MODE) {
        Logger::Instance().Log(LogLevel::WARN, "av_status", "COM-initialisatie mislukt");
        return results;
    }

    hres = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                                 RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                                 nullptr, EOAC_NONE, nullptr);
    // RPC_E_TOO_LATE is prima als security al eerder in het proces is ingesteld.

    IWbemLocator* pLoc = nullptr;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
    if (FAILED(hres)) {
        if (comInitializedHere) CoUninitialize();
        return results;
    }

    IWbemServices* pSvc = nullptr;
    // SecurityCenter2 is de namespace waar Windows zelf AV-producten registreert.
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\SecurityCenter2"), nullptr, nullptr,
                                nullptr, 0, nullptr, nullptr, &pSvc);
    if (FAILED(hres)) {
        pLoc->Release();
        if (comInitializedHere) CoUninitialize();
        return results;
    }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                       RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    IEnumWbemClassObject* pEnumerator = nullptr;
    hres = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT displayName, productState FROM AntiVirusProduct"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr, &pEnumerator);

    if (SUCCEEDED(hres) && pEnumerator) {
        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn == 0) break;

            AvProductInfo info;
            VARIANT vtName, vtState;
            VariantInit(&vtName);
            VariantInit(&vtState);

            pclsObj->Get(L"displayName", 0, &vtName, nullptr, nullptr);
            if (vtName.vt == VT_BSTR) info.displayName = vtName.bstrVal;

            pclsObj->Get(L"productState", 0, &vtState, nullptr, nullptr);
            if (vtState.vt == VT_I4) {
                // productState is een bitmask; de middelste byte draagt
                // doorgaans de "enabled" status. Dit is een bekend maar
                // ongedocumenteerd Microsoft-formaat.
                int state = vtState.intVal;
                info.enabled = ((state >> 12) & 0xF) != 0;
                info.upToDate = ((state) & 0xFF) == 0;
            }

            VariantClear(&vtName);
            VariantClear(&vtState);
            pclsObj->Release();

            results.push_back(info);
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    if (comInitializedHere) CoUninitialize();

    return results;
}

} // namespace aegis
