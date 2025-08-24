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
uint8_t txPower = TX_POWER_DBuv;
bool rdsOn = true;

// ===== UI IDs =====
uint16_t tabRadio, tabRDS, tabAdvanced;
uint16_t idSwitch, idLabelFreq, idSliderMHz, idSliderFrac, idSelect, idTxPower;
uint16_t idRdsSwitch, idRdsPS, idRdsRT, idRdsApply;

// ===== Presets (Moscow) =====
struct Preset
{
    const char *name;
    uint16_t t;
};
Preset PRESETS[] = {
    {"Europa Plus — 106.2", 1062},
    {"Русское Радио — 105.7", 1057},
    {"Маяк — 103.4", 1034},
    {"Вести FM — 97.6", 976},
    {"Радио России — 101.5", 1015},
    {"Орфей — 99.2", 992},
    {"Авторадио — 90.3", 903},
    {"Наше Радио — 101.8", 1018},
    {"Love Radio — 106.6", 1066},
    {"Retro FM — 88.3", 883},
    {"Дорожное Радио — 96.0", 960},
    {"Шансон — 103.0", 1030},
    {"Монте-Карло — 102.1", 1021},
    {"ENERGY — 104.2", 1042},
    {"Радио Дача — 92.4", 924},
    {"Relax FM — 90.8", 908},
    {"Юмор FM — 88.7", 887},
    {"Радио 7 — 104.7", 1047},
    {"Comedy Radio — 102.5", 1025},
    {"Звезда — 95.6", 956},
    {"Rock FM — 95.2", 952},
    {"Хит FM — 107.4", 1074},
    {"JAZZ — 89.1", 891}};
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
static String fmtMHz(uint16_t t)
{
    char b[8];
    snprintf(b, sizeof(b), "%u.%u", t / 10, t % 10);
    return String(b);
}

void applyTxPower(uint8_t pwr)
{
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
    tenths = clampTenths(tenths);
    freq_tenths = tenths;
    ESPUI.updateControlValue(idLabelFreq, "Freq: " + fmtMHz(freq_tenths) + " MHz");
    ESPUI.updateControlValue(idSliderMHz, String(freq_tenths / 10));
    ESPUI.updateControlValue(idSliderFrac, String(freq_tenths % 10));
    if (radioOn && radioFound)
    {
        radio.tuneFM(freq_tenths * 10); // Si4713 expects 10 kHz units
    }
}

void radioPowerOn()
{
    digitalWrite(RESETPIN, HIGH);
    delay(15);                  // wakeup time
    radioFound = radio.begin(); // (re)initialize every ON
    if (!radioFound)
    {
        Serial.println("[Si4713] begin() failed");
        return;
    }
    radio.setTXpower(txPower);
    radio.tuneFM(freq_tenths * 10);
    if (rdsOn)
        radio.beginRDS();
    Serial.printf("[Si4713] ON @ %s MHz\n", fmtMHz(freq_tenths).c_str());
}

void radioPowerOff()
{
    digitalWrite(RESETPIN, LOW); // hold reset = OFF
    radioFound = false;
    Serial.println("[Si4713] OFF (reset)");
}

// ===== ESPUI callbacks (classic API) =====
// NOTE: 'type' is the event code; actual value lives in sender->value
void cbRadioSwitch(Control *sender, int type)
{
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
    if (type == SL_VALUE)
    {
        uint16_t mhz = (uint16_t)sender->value.toInt();
        uint16_t t = mhz * 10 + (freq_tenths % 10);
        applyTune(t);
    }
}

void cbFreqSliderFrac(Control *sender, int type)
{
    if (type == SL_VALUE)
    {
        uint16_t frac = (uint16_t)sender->value.toInt();
        uint16_t t = (freq_tenths / 10) * 10 + frac;
        applyTune(t);
    }
}

void cbPresetSelect(Control *sender, int type)
{
    if (type == S_VALUE)
    {
        uint16_t t = (uint16_t)sender->value.toInt(); // we store tenths in Option value
        applyTune(t);
    }
}

void cbTxPower(Control *sender, int type)
{
    if (type == SL_VALUE)
    {
        applyTxPower((uint8_t)sender->value.toInt());
    }
}

static String trimPS8(const String &s)
{
    String t = s;
    t.trim();
    if (t.length() > 8)
        t.remove(8);
    return t;
}

void cbRdsSwitch(Control *sender, int type)
{
    rdsOn = (type == S_ACTIVE);
    if (rdsOn && radioOn && radioFound)
        radio.beginRDS();
}

void cbRdsApply(Control *sender, int type)
{
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

// ===== build UI (classic API) =====
void buildUI()
{
    // Tabs
    tabRadio = ESPUI.addControl(ControlType::Tab, "Radio", "Radio");
    tabRDS = ESPUI.addControl(ControlType::Tab, "RDS", "RDS");
    tabAdvanced = ESPUI.addControl(ControlType::Tab, "Advanced", "Advanced");

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
}

void setup()
{
    Serial.begin(115200);
    delay(200);

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
    ESPUI.updateControlValue(idLabelFreq, "Freq: " + fmtMHz(freq_tenths) + " MHz");
    ESPUI.updateControlValue(idSliderMHz, String(freq_tenths / 10));
    ESPUI.updateControlValue(idSliderFrac, String(freq_tenths % 10));
    ESPUI.updateControlValue(idTxPower, String(txPower));
}

void loop()
{
    // ESPUI async – nothing here
}
