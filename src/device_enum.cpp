#include "device_enum.h"
#include "logger.h"
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace aegis {

std::vector<uint32_t> DeviceEnumerator::LoadBlocklistVenDevPairs(const std::wstring& blocklistPath) {
    // Verwacht formaat (simpel, één per regel, TODO: naar JSON zodra de
    // config-laag JSON-parsing heeft): "VEN_XXXX_DEV_YYYY"
    // Encodeer als één uint32: (vendorId << 16) | deviceId, voor snelle lookup.
    std::vector<uint32_t> pairs;
    std::wifstream file(blocklistPath);
    if (!file.good()) {
        Logger::Instance().Log(LogLevel::WARN, "device_enum",
            "DMA-blocklist niet gevonden - device-matching draait met lege lijst");
        return pairs;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        // TODO: echte parsing. Placeholder-structuur staat klaar zodat de
        // blocklist later gevuld kan worden zonder deze functie te herschrijven.
    }
    return pairs;
}

std::vector<PciDeviceInfo> DeviceEnumerator::EnumeratePciDevices(const std::wstring& blocklistPath) {
    std::vector<PciDeviceInfo> results;
    auto blocklist = LoadBlocklistVenDevPairs(blocklistPath);

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(nullptr, L"PCI", nullptr,
                                              DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE) return results;

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        wchar_t hwId[512] = {0};
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                                               nullptr, reinterpret_cast<PBYTE>(hwId),
                                               sizeof(hwId), nullptr)) {
            PciDeviceInfo info;
            info.hardwareId = hwId;

            // Verwacht formaat: PCI\VEN_1234&DEV_5678&SUBSYS_...
            std::wstring hwIdStr(hwId);
            auto venPos = hwIdStr.find(L"VEN_");
            auto devPos = hwIdStr.find(L"DEV_");
            if (venPos != std::wstring::npos && devPos != std::wstring::npos) {
                info.vendorId = static_cast<uint16_t>(
                    wcstoul(hwIdStr.substr(venPos + 4, 4).c_str(), nullptr, 16));
                info.deviceId = static_cast<uint16_t>(
                    wcstoul(hwIdStr.substr(devPos + 4, 4).c_str(), nullptr, 16));
            }

            wchar_t desc[256] = {0};
            SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
                                               nullptr, reinterpret_cast<PBYTE>(desc),
                                               sizeof(desc), nullptr);
            info.deviceDescription = desc;

            uint32_t key = (static_cast<uint32_t>(info.vendorId) << 16) | info.deviceId;
            info.matchedBlocklist = std::find(blocklist.begin(), blocklist.end(), key) != blocklist.end();

            if (info.matchedBlocklist) {
                std::ostringstream extra;
                extra << "\"vendor_id\":\"" << std::hex << info.vendorId
                      << "\",\"device_id\":\"" << info.deviceId << "\"";
                Logger::Instance().LogEvent(LogLevel::FLAG, "device_enum",
                    "PCI-device matcht DMA-blocklist", extra.str());
            }

            results.push_back(info);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return results;
}

SystemDmaProtection DeviceEnumerator::CheckDmaProtectionStatus() {
    SystemDmaProtection status;

    // Windows Kernel DMA Protection status staat in de registry onder
    // MSFT0201 device of via WMI (MSFT_PhysicalDisk niet relevant hier).
    // Simpelere/robuustere route: MSDM/registry-flag die Windows zelf
    // bijhoudt voor "Kernel DMA Protection".
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                       L"SYSTEM\\CurrentControlSet\\Control\\DmaSecurity\\Devices",
                       0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        status.kernelDmaProtectionOn = true; // Sleutel aanwezig = feature actief op dit systeem
        RegCloseKey(hKey);
    }

    // IOMMU-detectie robuust doen vereist WMI (Win32_DeviceGuardKeys of
    // MS_VirtualizationBasedSecurity) - TODO: koppelen aan de WMI-helper
    // die ook av_status.cpp gebruikt, om COM-init niet te dupliceren.
    status.iommuPresent = false; // Placeholder tot WMI-helper gedeeld is

    return status;
}

std::vector<PciDeviceInfo> DeviceEnumerator::EnumerateThunderboltDevices() {
    std::vector<PciDeviceInfo> results;
    // TODO: filteren op Thunderbolt-specifieke device class/GUID
    // (GUID_DEVCLASS_THUNDERBOLT_CONTROLLER-achtige aanpak) i.p.v. een
    // losse SetupDiGetClassDevs-call. Voor nu gedekt door de generieke
    // PCI-enumeratie hierboven, aparte Thunderbolt-security-level check
    // (WT_SECURITY_LEVEL) is een vervolgstap.
    return results;
}

} // namespace aegis
