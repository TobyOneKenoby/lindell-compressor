# SA-670 / Vibealizer: rör-EQ + 670-kompressor (VST3/AU för macOS)

Ett JUCE-plugin (C++17, CMake) med två delar:

1. **Program-EQ.** Kretsmodeller av Pultec EQP-1A och MEQ-5 med omkopplingsbart utsteg: **TUBE** (EQP-1A:s push-pull-förstärkare med 12AX7/12AU7) eller **SS** (diskret op-amp och transformator i API-stil).
2. **670-kompressor/limiter.** Variabel mu med L/R- eller M/S-drift (Lat/Vert).

## Bygga på Mac

Krav: Xcode (eller Command Line Tools) och CMake ≥ 3.22 (`brew install cmake`). JUCE 8.0.4 hämtas automatiskt.

```bash
./build_mac.sh
```

Skriptet bygger en universal binary (Apple Silicon + Intel) och kopierar pluginen till
`~/Library/Audio/Plug-Ins/VST3/` och `~/Library/Audio/Plug-Ins/Components/` (AU).
Om din DAW inte hittar pluginen kan du behöva ad hoc-signera den:

```bash
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/SA-670.vst3
```

`.github/workflows/build-macos.yml` bygger samma sak i GitHub Actions om projektet pushas till GitHub.

## Signalkedja

```
in → [4x översampling: EQP-1A-nät → MEQ-5-nät → LOW CUT 24 dB/okt → HIGH CUT 6 dB/okt → TUBE/SS-förstärkare]
   → 670-kompressor (L/R eller M/S) → SOLO M/S → ut
```

### EQ: kretsmodell

De passiva näten simuleras komponent för komponent med nodanalys (MNA) och trapetsdiskretisering. Det motsvarar en bilinjär transform av den analoga kretsen. Värdena kommer från schemana:

- **EQP-1A** (Tubetec PE-1a): BOOST HI 10K-A med serieresonant L/C (175 mH/15 nF … 19 mH/3,3 nF), HI BOOST Q 2K2, CUT HI 1K-C med 75R och 47n/94n/150n, BOOST LO 10K-B med 2,2 µ/1 µ/470 n/330 n, CUT LO 100K-B med 100 n/47 n/22 n/15 n, 570R-terminering och mellantrafo som last.
- **MEQ-5** (Gyraf): LOW BOOST 2K5 med spoluttag 120–520 mH, MID CUT 2K5-C med 270R och spoluttag 34–420 mH, HIGH BOOST 10K med 477/625 mH, samt 510R, 1K och 5K1.

Samspelet mellan boost och atten (Pultec-tricket) och de belastningsberoende Q-värdena uppstår därför av kretsen själv. Här är det som inte framgår av schemana:

| Okänt i schemat | Hur det hanteras |
|---|---|
| Mellantrafons reflekterade last | Kalibrerad till 14 kΩ så att max LOW BOOST/ATTEN blir +12,8/−17,4 dB (EQP-1A-spec: +13,5/−17,5) |
| Spolarnas DC-resistans | Uppskattad (5 Ω + 0,25 Ω/mH) |
| MEQ-kondensatorer för panelfrekvenser som saknar egen kondensator | Beräknade för resonans med schemats spoluttag |
| Kontaktresistans i en pot "på noll" | 0,5 Ω |

### Kompressor

Tidskonstanterna 1–6 följer 670:s. Läge 5 och 6 är programberoende med flera releasesteg, och MANUAL har egna attack- och release-rattar.

| Reglage | Funktion |
|---|---|
| AGC | INDEPENDENT / LINK / MID-SIDE / MID-SIDE LINK. I M/S-läge styr kanal 1 (LAT) mitten och kanal 2 (VERT) sidan. |
| AGC FILTER | Högpass på sidokedjan (90 / 180 Hz) eller MID-bandpass |
| DC THRESHOLD | Ratio och knä: från 2:1 mjukt till 20:1 hårt (limiter) |
| DRY/WET | Parallellkompression per kanal |

### Brytare och knappar

| Reglage | Funktion |
|---|---|
| IN | EQ-nätet in/ur |
| TUBE/SS | Väljer utsteg |
| ON (med lampa) | Utstegets färgning på/av |
| LINK | Höger kanals reglage följer vänster |
| IN/BYPASS | Kompressorn in/ur |
| Mätarväljaren | BAL = in, ZERO = GR, BAL = ut |
| ZERO-skruven | Kalibrerar mätarens nolla |
| BAL-skruven | Trimmar nivån ±2 dB |
| TOLERANCE (bottenraden) | Ger varje kanal egna komponenttoleranser (R ±1 %, C ±5 %, L ±3 %, rörbias). RANDOM och pilarna byter "exemplar". |
| SOLO M / SOLO S | Avlyssning av mitt eller sida |
| DISPLAY | Visar värden när du rör ett reglage |

VU-mätarna läser medelvärdet av likriktad signal (0 VU = −18 dBFS). Visaren har standardens ballistik: 300 ms och cirka 1,5 % översläng.

## GUI

- **Panelen ritas helt som vektor** (`Source/UI/PanelArt.cpp`): pulverlack, rackfästen, skruvar, alla texter och skalor, VU-hus och lampa. Den cachas i skärmens upplösning, så den är skarp på Retina. Typsnitt: Jost (SIL OFL, `Resources/fonts`).
- **Rattar och spakar** är fotografiska sprites ur din panelbild (`Resources/panel_source.webp`). `tools/clean_panel.py` och `tools/make_assets.py` skär ut dem och mäter skalvinklarna till `Source/UI/Layout.h`:

  ```bash
  python3 tools/clean_panel.py && python3 tools/make_assets.py
  ```

- **VU-mätarna** är vektorritade med standardens ballistik.
- **Levande detaljer:** lampan värms upp och lyser på panelen, VU-belysningen följer ON, spakarna svänger och rattarna glider.
- **DISPLAY** öppnar en spektrumanalysator med EQ-kurvan, beräknad från kretsmodellen.

## Test

```bash
cmake -B build -DSA670_BUILD_TESTS=ON && cmake --build build --target SA670Test
```

`SA670Test` kör ljud genom pluginet och kontrollerar EQ-kurvor, filter, M/S och CPU-last. Den sparar också en skärmbild av GUI:t.
