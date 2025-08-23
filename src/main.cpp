#include <Arduino.h>
#include <WiFi.h>
#define GH_INCLUDE_PORTAL // вшить сайт в прошивку (~50 КБ)
#include <GyverHub.h>

#include <Wire.h>
#include <Adafruit_Si4713.h>
#include <stdlib.h>
#include <StringUtils.h>

// ====== Si4713 ======
#define RESETPIN 10
#define TX_POWER_DBuv 115 // 88..115 (115 max)
Adafruit_Si4713 radio(RESETPIN);

// ====== GyverHub ======
GyverHub hub("MyDevices", "ESP32-C3", ""); // префикс, имя, иконка

// ====== Состояние радио ======
bool radioOn = false;
uint16_t freq_khz = 9840; // по умолчанию 98.40 МГц
uint8_t presetIdx = 0;
const uint16_t PRESETS[] = {
    8810, 9050, 9240, 9450, 9630, 9870, 10010, 10230, 10470, 10790};
#define PRESETS_N (sizeof(PRESETS) / sizeof(PRESETS[0]))

// ---------- утилиты ----------
void radioPowerOn()
{
    // вывести из reset и инициализировать
    digitalWrite(RESETPIN, HIGH);
    delay(10);
    if (!radio.begin())
    {
        Serial.println(F("[Si4713] Not found after reset"));
        return;
    }
    radio.setTXpower(TX_POWER_DBuv);
    radio.tuneFM(freq_khz);
}

void radioPowerOff()
{
    // простой «OFF» — удерживаем чип в reset
    digitalWrite(RESETPIN, LOW);
}

// ---------- GUI (builder) ----------
void build(gh::Builder &b)
{ // см. раздел "Билдер" в доке :contentReference[oaicite:1]{index=1}
    b.Title("FM Transmitter");

    b.Switch_("radio_on", &radioOn) // переключатель «включить радио»
        .label("Включить радио");

    b.Slider_("freq_slider", &freq_khz) // ползунок 8000..11000, шаг 10
        .label("Частота, кГц")
        .range(8000, 11000, 10)
        .unit("kHz");

    // выпадающий список с рандомными пресетами
    // value = индекс пункта, text = "пункты через ;"
    String list;
    for (uint8_t i = 0; i < PRESETS_N; i++)
    {
        if (i)
            list += ';';
        uint16_t kHz = PRESETS[i];
        char buf[8]; // "xxxxx.yy" хватит
        snprintf(buf, sizeof(buf), "%u.%02u", kHz / 100, kHz % 100);
        list += buf;
    }

    b.Select_("freq_select", &presetIdx)
        .label("Пресеты")
        .text(list);
}

// обработчик команд от клиента (Set/Read/Get и т.п.) — см. onRequest/gh::Request в доке :contentReference[oaicite:2]{index=2}
// обработчик ДОЛЖЕН возвращать bool
bool onReq(gh::Request &r)
{
    if (r.cmd != gh::CMD::Set)
        return false;

    if (r.name == "radio_on")
    {
        // r.value — AnyText: сравниваем напрямую или через toString()
        radioOn = (r.value == "1" || r.value == "true");
        if (radioOn)
            radioPowerOn();
        else
            radioPowerOff();
        return true;
    }

    if (r.name == "freq_slider")
    {
        freq_khz = r.value.toString().toInt();
        if (radioOn)
            radio.tuneFM(freq_khz);
        return true;
    }

    if (r.name == "freq_select")
    {
        uint8_t idx = r.value.toString().toInt();
        if (idx >= PRESETS_N)
            idx = 0;
        presetIdx = idx;
        freq_khz = PRESETS[presetIdx];

        // синхронизируем слайдер
        hub.sendUpdate("freq_slider", (int)freq_khz);

        if (radioOn)
            radio.tuneFM(freq_khz);
        return true;
    }

    return false; // неизвестное имя — не обработали
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    // ---------- Wi-Fi AP ----------
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32C3-Radio"); // без пароля (можно добавить)
    Serial.print(F("[WiFi] AP IP: "));
    Serial.println(WiFi.softAPIP());

    // ---------- I2C + Si4713 ----------
    Wire.begin(8, 9); // SDA=8, SCL=9 (твои пины)
    pinMode(RESETPIN, OUTPUT);
    radioPowerOff(); // стартуем в «выкл.»

    // ---------- GyverHub ----------
    hub.onBuild(build);   // GUI
    hub.onRequest(onReq); // обработка команд
    hub.begin();          // запуск хаба (tick в loop) :contentReference[oaicite:4]{index=4}
}

void loop()
{
    hub.tick(); // обязательно в loop :contentReference[oaicite:5]{index=5}
}
