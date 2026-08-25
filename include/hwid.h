#pragma once
#include <string>

namespace aegis {

// Genereert een salted, gehashte hardware-fingerprint. Bewust GEEN raw
// serials/MAC-adressen naar buiten: alleen de uiteindelijke hash wordt
// gelogd/gerapporteerd (zie privacy-afspraken in project-README).
class HwidFingerprint {
public:
    // salt: per-tournament salt zodat dezelfde pc niet cross-tournament
    // te correleren is via een vaste hash.
    static std::string Generate(const std::string& salt);

private:
    static std::wstring GetMachineGuid();
    static std::wstring GetCpuIdString();
    static std::wstring GetSystemVolumeSerial();
    static std::wstring GetBiosSerial();
    static std::string Sha256Hex(const std::wstring& input);
};

} // namespace aegis
