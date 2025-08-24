#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPUI.h>
#include <Adafruit_Si4713.h>

// ===== Wi-Fi AP =====
const char *AP_SSID = "FM-TX";
const char *AP_PASS = "12345678"; // >=8 symbols

// ===== Si4713 wiring =====
#define RESETPIN 10
#define SDA_PIN 8
#define SCL_PIN 9
#define TX_POWER_DBuv 115 // 88..115

Adafruit_Si4713 radio(RESETPIN);
bool radioOn = false;
bool radioFound = false;

// Frequency in tenths of MHz (e.g. 1034 = 103.4 MHz)
uint16_t freq_tenths = 1034;
int16_t freqCorrection = 0; // kHz
uint8_t txPower = TX_POWER_DBuv;
bool rdsOn = true;

// ===== UI IDs =====
uint16_t tabRadio, tabRDS, tabAdvanced, tabScan;
uint16_t idSwitch, idLabelFreq, idSliderMHz, idSliderFrac, idSelect, idTxPower;
uint16_t idRdsSwitch, idRdsPS, idRdsRT, idRdsApply, idFreqCorr;
uint16_t idScanStart, idScanEnd, idScanStep, idScanBtn, idScanGraph, idScanOut;

// ===== Presets (Moscow) =====
struct Preset
{
    const char *name;
    uint16_t t;
};
Preset PRESETS[] = {
    {"Retro FM — 88.3", 883},
    {"Юмор FM — 88.7", 887},
    {"JAZZ — 89.1", 891},
    {"Авторадио — 90.3", 903},
    {"Relax FM — 90.8", 908},
    {"Радио Дача — 92.4", 924},
    {"Rock FM — 95.2", 952},
    {"Звезда — 95.6", 956},
    {"Дорожное Радио — 96.0", 960},
    {"Вести FM — 97.6", 976},
    {"Орфей — 99.2", 992},
    {"Радио России — 101.5", 1015},
    {"Наше Радио — 101.8", 1018},
    {"Монте-Карло — 102.1", 1021},
    {"Comedy Radio — 102.5", 1025},
    {"Шансон — 103.0", 1030},
    {"Маяк — 103.4", 1034},
    {"ENERGY — 104.2", 1042},
    {"Радио 7 — 104.7", 1047},
    {"Русское Радио — 105.7", 1057},
    {"Europa Plus — 106.2", 1062},
    {"Love Radio — 106.6", 1066},
    {"Хит FM — 107.4", 1074}};
const uint8_t PRESETS_N = sizeof(PRESETS) / sizeof(Preset);

// ===== helpers =====
static inline uint16_t clampTenths(uint16_t t)
{
    if (t < 820)
        t = 820; // 82.0 MHz
    if (t > 1080)
        t = 1080; // 108.0 MHz
    return t;
}
static String fmtMHz(uint16_t t, int16_t offset_khz = 0)
{
    int32_t cent = (int32_t)t * 10 + offset_khz / 10; // 0.01 MHz units
    char b[16];
    snprintf(b, sizeof(b), "%ld.%02ld", cent / 100, abs(cent % 100));
    return String(b);
}

static String fmtKHz(uint32_t khz)
{
    char b[16];
    snprintf(b, sizeof(b), "%lu.%03lu", khz / 1000, khz % 1000);
    return String(b);
}

void applyTxPower(uint8_t pwr)
{
    Serial.printf("[applyTxPower] %u\n", pwr);
    if (pwr < 88)
        pwr = 88;
    if (pwr > 115)
        pwr = 115;
    txPower = pwr;
    if (radioOn && radioFound)
        radio.setTXpower(txPower);
    ESPUI.updateControlValue(idTxPower, String(txPower));
}

void applyTune(uint16_t tenths)
{
    Serial.printf("[applyTune] %u\n", tenths);
    tenths = clampTenths(tenths);
    freq_tenths = tenths;
    ESPUI.updateControlValue(idLabelFreq, "Freq: " + fmtMHz(freq_tenths, freqCorrection) + " MHz");
    ESPUI.updateControlValue(idSliderMHz, String(freq_tenths / 10));
    ESPUI.updateControlValue(idSliderFrac, String(freq_tenths % 10));
    if (radioOn && radioFound)
    {
        radio.tuneFM(freq_tenths * 10 + freqCorrection / 10); // Si4713 expects 10 kHz units
    }
}

void applyFreqCorrection(int16_t corr)
{
    Serial.printf("[applyFreqCorrection] %d kHz\n", corr);
    freqCorrection = corr;
    ESPUI.updateControlValue(idFreqCorr, String(freqCorrection));
    applyTune(freq_tenths);
}

void radioPowerOn()
{
    Serial.println("[radioPowerOn]");
    digitalWrite(RESETPIN, HIGH);
    delay(15);                  // wakeup time
    radioFound = radio.begin(); // (re)initialize every ON
    if (!radioFound)
    {
        Serial.println("[Si4713] begin() failed");
        return;
    }
    radio.setTXpower(txPower);
    radio.tuneFM(freq_tenths * 10 + freqCorrection / 10);
    if (rdsOn)
        radio.beginRDS();
    Serial.printf("[Si4713] ON @ %s MHz\n", fmtMHz(freq_tenths, freqCorrection).c_str());
}

void radioPowerOff()
{
    Serial.println("[radioPowerOff]");
    digitalWrite(RESETPIN, LOW); // hold reset = OFF
    radioFound = false;
    Serial.println("[Si4713] OFF (reset)");
}

// ===== ESPUI callbacks (classic API) =====
// NOTE: 'type' is the event code; actual value lives in sender->value
void cbRadioSwitch(Control *sender, int type)
{
    Serial.printf("[cbRadioSwitch] type=%d\n", type);
    if (type == S_ACTIVE || type == S_INACTIVE)
    {
        radioOn = (type == S_ACTIVE);
        if (radioOn)
            radioPowerOn();
        else
            radioPowerOff();
    }
}

void cbFreqSliderMHz(Control *sender, int type)
{
    Serial.printf("[cbFreqSliderMHz] type=%d val=%s\n", type, sender->value.c_str());
    if (type == SL_VALUE)
    {
        uint16_t mhz = (uint16_t)sender->value.toInt();
        uint16_t t = mhz * 10 + (freq_tenths % 10);
        applyTune(t);
    }
}

void cbFreqSliderFrac(Control *sender, int type)
{
    Serial.printf("[cbFreqSliderFrac] type=%d val=%s\n", type, sender->value.c_str());
    if (type == SL_VALUE)
    {
        uint16_t frac = (uint16_t)sender->value.toInt();
        uint16_t t = (freq_tenths / 10) * 10 + frac;
        applyTune(t);
    }
}

void cbPresetSelect(Control *sender, int type)
{
    Serial.printf("[cbPresetSelect] type=%d val=%s\n", type, sender->value.c_str());
    if (type == S_VALUE)
    {
        uint16_t t = (uint16_t)sender->value.toInt(); // we store tenths in Option value
        applyTune(t);
    }
}

void cbTxPower(Control *sender, int type)
{
    Serial.printf("[cbTxPower] type=%d val=%s\n", type, sender->value.c_str());
    if (type == SL_VALUE)
    {
        applyTxPower((uint8_t)sender->value.toInt());
    }
}

void cbFreqCorr(Control *sender, int type)
{
    Serial.printf("[cbFreqCorr] type=%d val=%s\n", type, sender->value.c_str());
    if (type == S_VALUE)
    {
        applyFreqCorrection((int16_t)sender->value.toInt());
    }
}

static String trimPS8(const String &s)
{
    Serial.printf("[trimPS8] input='%s'\n", s.c_str());
    String t = s;
    t.trim();
    if (t.length() > 8)
        t.remove(8);
    return t;
}

void cbRdsSwitch(Control *sender, int type)
{
    Serial.printf("[cbRdsSwitch] type=%d\n", type);
    rdsOn = (type == S_ACTIVE);
    if (rdsOn && radioOn && radioFound)
        radio.beginRDS();
}

void cbRdsApply(Control *sender, int type)
{
    Serial.printf("[cbRdsApply] type=%d\n", type);
    if (type == B_UP && radioOn && radioFound && rdsOn)
    {
        Control *ps = ESPUI.getControl(idRdsPS);
        Control *rt = ESPUI.getControl(idRdsRT);
        String ps8 = ps ? trimPS8(ps->value) : "RADIO";
        String rt64 = rt ? rt->value : "Hello from ESP32";
        radio.setRDSstation(ps8.c_str());
        radio.setRDSbuffer(rt64.c_str());
    }
}

void cbScanBtn(Control *sender, int type)
{
    Serial.printf("[cbScanBtn] type=%d\n", type);
    if (type == B_UP)
    {
        Control *cs = ESPUI.getControl(idScanStart);
        Control *ce = ESPUI.getControl(idScanEnd);
        Control *ck = ESPUI.getControl(idScanStep);
        uint16_t startMHz = cs ? cs->value.toInt() : 87;
        uint16_t endMHz = ce ? ce->value.toInt() : 108;
        uint16_t stepKHz = ck ? ck->value.toInt() : 100;
        Serial.printf("[scan] start=%u end=%u step=%u\n", startMHz, endMHz, stepKHz);
        ESPUI.clearGraph(idScanGraph);
        String res;
        bool wasOn = radioOn;
        if (!radioOn)
            radioPowerOn();
        for (uint32_t kHz = startMHz * 1000UL; kHz <= endMHz * 1000UL; kHz += stepKHz)
        {
            radio.readTuneMeasure(kHz);
            radio.readTuneStatus();
            uint8_t noise = radio.currNoiseLevel;
            ESPUI.addGraphPoint(idScanGraph, noise);
            res += fmtKHz(kHz);
            res += " MHz : ";
            res += String(noise);
            res += "\n";
            Serial.printf("[scan] %s MHz noise=%u\n", fmtKHz(kHz).c_str(), noise);
        }
        ESPUI.updateControlValue(idScanOut, res);
        if (radioOn && radioFound)
            radio.tuneFM(freq_tenths * 10 + freqCorrection / 10);
        if (!wasOn)
            radioPowerOff();
    }
}

// ===== build UI (classic API) =====
void buildUI()
{
    Serial.println("[buildUI]");
    // Tabs
    tabRadio = ESPUI.addControl(ControlType::Tab, "Radio", "Radio");
    tabRDS = ESPUI.addControl(ControlType::Tab, "RDS", "RDS");
    tabAdvanced = ESPUI.addControl(ControlType::Tab, "Advanced", "Advanced");
    tabScan = ESPUI.addControl(ControlType::Tab, "Scan", "Scan");

    // --- Radio tab ---
    idSwitch = ESPUI.addControl(ControlType::Switcher, "Radio ON",
                                radioOn ? "1" : "0", ControlColor::Emerald, tabRadio, &cbRadioSwitch);

    idLabelFreq = ESPUI.addControl(ControlType::Label, "Frequency", "Freq: --.- MHz",
                                   ControlColor::Peterriver, tabRadio);

    // Sliders: MHz and tenths
    idSliderMHz = ESPUI.addControl(ControlType::Slider, "Tune MHz",
                                   String(freq_tenths / 10), ControlColor::Carrot, tabRadio, &cbFreqSliderMHz);
    ESPUI.addControl(ControlType::Min, "", "82", ControlColor::None, idSliderMHz);
    ESPUI.addControl(ControlType::Max, "", "108", ControlColor::None, idSliderMHz);
    ESPUI.addControl(ControlType::Step, "", "1", ControlColor::None, idSliderMHz);

    idSliderFrac = ESPUI.addControl(ControlType::Slider, "Decimals",
                                    String(freq_tenths % 10), ControlColor::Carrot, tabRadio, &cbFreqSliderFrac);
    ESPUI.addControl(ControlType::Min, "", "0", ControlColor::None, idSliderFrac);
    ESPUI.addControl(ControlType::Max, "", "9", ControlColor::None, idSliderFrac);
    ESPUI.addControl(ControlType::Step, "", "1", ControlColor::None, idSliderFrac);

    ESPUI.addControl(ControlType::Separator, "Presets (Moscow)", "",
                     ControlColor::None, tabRadio);

    idSelect = ESPUI.addControl(ControlType::Select, "Stations", "",
                                ControlColor::Peterriver, tabRadio, &cbPresetSelect);
    for (uint8_t i = 0; i < PRESETS_N; i++)
    {
        ESPUI.addControl(ControlType::Option, PRESETS[i].name,
                         String(PRESETS[i].t), ControlColor::None, idSelect);
    }

    ESPUI.addControl(ControlType::Separator, "TX power (dBuV)", "",
                     ControlColor::None, tabAdvanced);

    idTxPower = ESPUI.addControl(ControlType::Slider, "Power",
                                 String(txPower), ControlColor::Sunflower, tabAdvanced, &cbTxPower);
    ESPUI.addControl(ControlType::Min, "", "88", ControlColor::None, idTxPower);
    ESPUI.addControl(ControlType::Max, "", "115", ControlColor::None, idTxPower);
    ESPUI.addControl(ControlType::Step, "", "1", ControlColor::None, idTxPower);

    ESPUI.addControl(ControlType::Separator, "Freq correction (kHz)", "",
                     ControlColor::None, tabAdvanced);

    idFreqCorr = ESPUI.addControl(ControlType::Number, "Correction",
                                   String(freqCorrection), ControlColor::Wetasphalt, tabAdvanced, &cbFreqCorr);

    // --- RDS tab ---
    idRdsSwitch = ESPUI.addControl(ControlType::Switcher, "RDS enable",
                                   rdsOn ? "1" : "0", ControlColor::Emerald, tabRDS, &cbRdsSwitch);

    idRdsPS = ESPUI.addControl(ControlType::Text,
                               "PS (Program Service, 8 chars)", "ESPUI FM",
                               ControlColor::Wetasphalt, tabRDS);

    idRdsRT = ESPUI.addControl(ControlType::Text,
                               "RadioText (up to ~64 chars)", "Hello from ESP32-C3 + Si4713",
                               ControlColor::Wetasphalt, tabRDS);

    idRdsApply = ESPUI.addControl(ControlType::Button, "Apply RDS", "Send",
                                  ControlColor::Peterriver, tabRDS, &cbRdsApply);

    // --- Scan tab ---
    idScanStart = ESPUI.addControl(ControlType::Number, "Start MHz", "87", ControlColor::Wetasphalt, tabScan);
    idScanEnd = ESPUI.addControl(ControlType::Number, "End MHz", "108", ControlColor::Wetasphalt, tabScan);
    idScanStep = ESPUI.addControl(ControlType::Number, "Step kHz", "100", ControlColor::Wetasphalt, tabScan);
    idScanBtn = ESPUI.addControl(ControlType::Button, "Start scan", "Run", ControlColor::Peterriver, tabScan, &cbScanBtn);
    idScanGraph = ESPUI.addControl(ControlType::Graph, "Noise", "", ControlColor::None, tabScan);
    idScanOut = ESPUI.addControl(ControlType::Label, "Results", "", ControlColor::Peterriver, tabScan);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("[setup]");

    Wire.begin(SDA_PIN, SCL_PIN);
    pinMode(RESETPIN, OUTPUT);
    digitalWrite(RESETPIN, LOW); // start OFF

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    buildUI();
    ESPUI.begin("Si4713 FM Transmitter");

    // sync UI with defaults
    ESPUI.updateControlValue(idLabelFreq, "Freq: " + fmtMHz(freq_tenths, freqCorrection) + " MHz");
    ESPUI.updateControlValue(idSliderMHz, String(freq_tenths / 10));
    ESPUI.updateControlValue(idSliderFrac, String(freq_tenths % 10));
    ESPUI.updateControlValue(idTxPower, String(txPower));
    ESPUI.updateControlValue(idFreqCorr, String(freqCorrection));
}

void loop()
{
    // ESPUI async – nothing here
    // but keep function for completeness
}
