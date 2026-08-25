#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace aegis {

struct PciDeviceInfo {
    uint16_t vendorId = 0;
    uint16_t deviceId = 0;
    std::wstring deviceDescription;
    std::wstring hardwareId; // Volledige PCI\VEN_xxxx&DEV_xxxx string
    bool matchedBlocklist = false;
};

struct SystemDmaProtection {
    bool iommuPresent = false;       // VT-d / AMD-Vi aanwezig
    bool kernelDmaProtectionOn = false; // Windows Kernel DMA Protection status
};

class DeviceEnumerator {
public:
    // Enumereert alle PCI(e) devices en matcht tegen de blocklist.
    // Rapporteert bewust alleen vendor/device-ID + matched-status naar
    // logging, niet een volledige devicedump (privacy-scoping).
    static std::vector<PciDeviceInfo> EnumeratePciDevices(const std::wstring& blocklistPath);

    static SystemDmaProtection CheckDmaProtectionStatus();

    // Thunderbolt-devices apart, omdat PCIe-over-Thunderbolt een bekend
    // aanvalspad is voor externe DMA-cheats.
    static std::vector<PciDeviceInfo> EnumerateThunderboltDevices();

private:
    static std::vector<uint32_t> LoadBlocklistVenDevPairs(const std::wstring& blocklistPath);
};

} // namespace aegis
