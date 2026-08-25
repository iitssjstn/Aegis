# Aegis Anti-Cheat — user-mode client (skeleton)

C++20, Windows-only, user-mode (geen kernel-driver — bewuste keuze, zie
projectgeschiedenis). Lokale JSON-lines logging, geen netwerkcomponent nog.

## Structuur

| Module | Verantwoordelijkheid | Status |
|---|---|---|
| `logger` | Lokale JSON-lines logging | Werkend |
| `config` | Instellingen laden | Skeleton — JSON-parsing nog te integreren (nlohmann/json aanraden) |
| `hwid` | Gehashte hardware-fingerprint | Werkend (BIOS-serial nog TODO) |
| `process_monitor` | Process/AV/signature snapshot | Werkend |
| `integrity` | Exe-hash, start/stop tracking, CPU-speed check | Grotendeels werkend, hash-hergebruik nog te ontdubbelen met process_monitor |
| `screenshot` | Random + PrtScn screenshot capture | Werkend |
| `input_monitor` | Input-timing statistieken | Werkend, detectie-thresholds zijn placeholders |
| `device_enum` | PCIe-enumeratie + DMA-blocklist match | Skeleton — blocklist-parsing en IOMMU-check nog te doen |
| `av_status` | AV-productstatus via WMI | Werkend |

## Bouwen

Vereist: Visual Studio 2022 (of vergelijkbare MSVC-toolchain), CMake 3.20+.

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Belangrijke openstaande punten (niet overslaan)

1. **JSON-library integreren** (config + blocklist parsing zijn nu stubs).
2. **Consent-scherm** is nu console-based als placeholder — moet een
   echte UI worden vóórdat dit bij spelers draait, en de AVG-vereisten
   (rechtsgrond, bewaartermijn, inzage/verwijdering) moeten juridisch
   afgekaderd zijn, niet alleen technisch.
3. **Retention-logica ontbreekt nog**: niet-geflagde screenshots/logs
   moeten na de afgesproken termijn automatisch verwijderd worden — nu
   worden ze alleen weggeschreven, niet opgeruimd.
4. **DMA-blocklist is leeg.** Dit moet gevuld en onderhouden worden;
   zie de discussie over waarom dit sowieso nooit 100% sluitend is
   (spoofbare vendor/device-ID's, externe DMA-setups die geen hardware
   in het host-systeem achterlaten).
5. **Macro/aim-detectie thresholds zijn placeholders**, gebaseerd op
   geen echte speler-data. Eerst kalibreren op een testgroep voor je
   dit gebruikt om spelers te flaggen — anders krijg je false positives
   op mensen met snelle, consistente reflexen.
6. **Geen reporting-laag.** Zoals afgesproken: lokale logging voor nu.
   Wanneer je een backend toevoegt, hou de privacy-scoping aan die
   eerder besproken is (verdichte events versturen, niet ruwe dumps;
   screenshots pas opvragen bij een actieve flag/review).
7. **Menselijke review vóór consequenties** — dit skeleton flag't alleen
   in de logs; er zit bewust geen auto-ban-logica in.

## Wat dit NIET doet (bewust)

- Geen kernel-mode monitoring — mist daardoor in-memory patches, cheats
  die na gamestart via manual mapping laden zonder file-on-disk, en
  externe DMA-setups zonder host-side hardware-signature.
- Geen aim-gedragsanalyse (menselijk vs. onnatuurlijk aim-patroon) —
  dat vereist game-side/server-side data die deze client niet heeft.

