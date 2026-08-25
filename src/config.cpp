#include "config.h"
#include <fstream>

namespace aegis {

// TODO: vervang door echte JSON-parsing (bijv. nlohmann/json via vcpkg)
// zodra het config-formaat vaststaat. Voor nu: defaults + consent-flag
// wordt in main.cpp expliciet gezet na het tonen van het consent-scherm.
AppConfig AppConfig::LoadFromFile(const std::wstring& path) {
    AppConfig cfg;
    std::ifstream f(path);
    if (!f.good()) {
        // Geen config gevonden -> defaults, consent blijft false tot
        // de gebruiker die expliciet geeft via de UI-flow.
        return cfg;
    }
    // TODO: parse JSON velden hierboven in AppConfig.
    return cfg;
}

bool AppConfig::SaveToFile(const std::wstring& /*path*/) const {
    // TODO: implementeer zodra JSON-lib geïntegreerd is.
    return false;
}

} // namespace aegis
