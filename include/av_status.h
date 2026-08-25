#pragma once
#include <string>
#include <vector>

namespace aegis {

struct AvProductInfo {
    std::wstring displayName;
    bool enabled = false;
    bool upToDate = false;
};

// Vraagt geregistreerde AV-producten op via het Security Center WMI-
// namespace (root\SecurityCenter2). Alleen naam + status - geen
// scanresultaten of logs van het AV-product zelf (privacy-scoping).
class AvStatus {
public:
    static std::vector<AvProductInfo> QueryInstalledAntivirus();
};

} // namespace aegis
