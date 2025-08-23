#include <Arduino.h>
#include <WiFi.h>
#include <ESPUI.h>

#include <Wire.h>
#include <Adafruit_Si4713.h>

const char *AP_SSID = "ESP32C3-ESPUI";
const char *AP_PASS = "";

#define RESETPIN 10
#define SDA_PIN 8
#define SCL_PIN 9
#define TX_POWER_DBuv 115

Adafruit_Si4713 radio(RESETPIN);
bool radioFound = false;

bool radioOn = false;
uint16_t freq_khz = 9840;

uint16_t swId, slId;

void radioPowerOn()
{
    // release reset and give the chip time to wake
    digitalWrite(RESETPIN, HIGH);
    delay(15); // >=10ms is safe

    // ALWAYS (re)initialize after a hard reset
    radioFound = radio.begin(); // I2C addr 0x63 if CS=HIGH
    if (!radioFound)
    {
        Serial.println("[Si4713] begin() failed");
        return;
    }
    radio.setTXpower(TX_POWER_DBuv);
    radio.tuneFM(freq_khz);
    Serial.printf("[Si4713] ON @ %u kHz\n", freq_khz);
}

void radioPowerOff()
{
    // put the chip into hard reset and mark uninitialized
    digitalWrite(RESETPIN, LOW);
    radioFound = false; // <— critical!
    Serial.println("[Si4713] OFF (reset held)");
}

// ---- ESPUI callbacks ----
// NOTE: 'type' is an event code; the control value is in sender->value
void cbSwitch(Control *sender, int type)
{
    (void)type;
    String s = sender->value; // "1"/"0" or "true"/"false"
    bool on = (s == "1" || s == "true");
    radioOn = on;
    if (on)
        radioPowerOn();
    else
        radioPowerOff();
    Serial.printf("[UI] Switch -> %s\n", on ? "ON" : "OFF");
}

void cbSlider(Control *sender, int type)
{
    (void)type;
    // If you used the 6-arg slider with kHz range, this is already kHz:
    uint16_t kHz = (uint16_t)sender->value.toInt();
    if (kHz < 8000)
        kHz = 8000;
    if (kHz > 11000)
        kHz = 11000;
    freq_khz = kHz;
    if (radioOn && radioFound)
        radio.tuneFM(freq_khz);
    Serial.printf("[UI] Slider -> %u kHz\n", freq_khz);
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setTimeOut(50);
    pinMode(RESETPIN, OUTPUT);
    radioPowerOff();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    ESPUI.begin("ESP32-C3 Radio");

    // Store control IDs (optional but handy)
    swId = ESPUI.switcher("Enable Radio", cbSwitch, ControlColor::Emerald, radioOn);

    // If your ESPUI version supports min/max:
    slId = ESPUI.slider("Frequency (kHz)", cbSlider, ControlColor::Turquoise,
                        freq_khz, 8000, 11000);

    // If using the 0..100 slider variant instead, do this:
    // slId = ESPUI.slider("Frequency (%)", cbSlider, ControlColor::Turquoise, 28);
    // and in cbSlider: map 0..100 to 8000..11000.
}

void loop()
{
    // ESPUI is async, nothing required here
}
