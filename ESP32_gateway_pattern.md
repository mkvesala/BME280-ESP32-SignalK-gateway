# ESP32 Gateway Design Pattern - Claude Context Document

Tama tiedosto on tarkoitettu Clauden kontekstiksi uusia ESP32 gateway -toteutuksia laadittaessa.
Se kuvaa arkkitehtuurin, design patternin, luokkarakenteet ja niiden valiset suhteet
geneerisessa muodossa niin etta minka tahansa sensorin gateway voidaan toteuttaa samalla mallilla.

## Arkkitehtuurin yleiskuva

```
                        +---------------------+
                        |    XXX-ESP32-       |
                        |    SignalK-gateway  |
                        |        .ino         |
                        |                     |
                        |  XXXApplication app |
                        +----------+----------+
                                   |
                                   | app.begin() / app.loop()
                                   v
                   +-------------------------------+
                   |       XXXApplication           |
                   |  (stack-allocated members)     |
                   |                               |
                   |  XXXSensor        sensor      |
                   |  XXXProcessor     processor   |
                   |  XXXPreferences   prefs       |
                   |  SignalKBroker    signalk      |
                   |  ESPNowBroker     espnow       |
                   |  DisplayManager   display      |
                   |  WebUIManager     webui        |
                   +-------------------------------+
                          |   |   |   |   |   |
              +-----------+   |   |   |   |   +------------+
              |               |   |   |   |                |
        +-----v-----+  +-----v---v---v----+   +------------v----------+
        | XXXSensor  |  | XXXProcessor     |   | SignalKBroker         |
        | (laite-I/O)|  | (bisneslogiikka) |   | (WebSocket -> SignalK)|
        +------------+  +------------------+   +-----------------------+
                                |                          |
                         +------v--------+        +--------v---------+
                         | XXXPreferences |        | ESPNowBroker     |
                         | (NVS)         |        | (ESP-NOW bcast)  |
                         +---------------+        +------------------+
```

## 1. Main .ino -tiedosto

Main-tiedosto pidetaan mahdollisimman kevyena. Se luo globaalin app-instanssin stackiin,
kutsuu `app.begin()` setupissa ja `app.loop()` loopissa. Kriittiset tarkistukset
(esim. sensorin olemassaolo) tehdaan setup()-vaiheessa.

```cpp
#include <Arduino.h>
#include "XXXApplication.h"

XXXApplication app;

void setup() {
  Serial.begin(115200);
  delay(47);

  app.begin();

  if (!app.sensorOk()) {
    Serial.println("SENSOR INIT FAILED! CHECK SYSTEM!");
    while(1) delay(1999);
  }
}

void loop() {
  app.loop();
}
```

### Keskeiset periaatteet:
- Ei globaaleja muuttujia paitsi yksi `app`-instanssi
- `setup()` ja `loop()` sisaltavat vain app-kutsut ja kriittisen tarkistuksen
- `while(1)` looppi pysayttaa ohjelman jos kriittinen komponentti puuttuu

---

## 2. XXXApplication - Sovelluksen orkestrointiluokka

Application-luokka on koko ohjelman omistaja. Se:
- Omistaa kaikki muut luokat **stack-allokoituina membereina** (ei `new`, ei pointtereita)
- Johdottaa luokkien valiset riippuvuudet **konstruktorin initializer listassa** `&`-viittauksilla
- Hallinnoi WiFi-tilaa, ajastimia ja kutsuu handlereita loop():ssa
- Tarjoaa begin() ja loop() metodit main-tiedostolle

### Header (.h)

```cpp
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include "WifiState.h"
#include "XXXSensor.h"
#include "XXXProcessor.h"
#include "XXXPreferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"
#include "WebUIManager.h"
#include "ESPNowBroker.h"

class XXXApplication {

  public:
    explicit XXXApplication();

    void begin();
    void loop();
    bool sensorOk() const { return sensor_ok; }

  private:

    // Laitteistokohtaiset vakiot (esim. I2C osoite, pinnit)
    static constexpr uint8_t SENSOR_ADDR = 0x60;       // I2C-osoite
    static constexpr uint8_t I2C_SDA = 16;              // SH-ESP32 default SDA
    static constexpr uint8_t I2C_SCL = 17;              // SH-ESP32 default SCL

    // Ajastusvakiot - maarittele implementaation mukaan
    static constexpr unsigned long MIN_TX_INTERVAL_MS    = 101;
    static constexpr unsigned long READ_MS               = 47;
    static constexpr unsigned long WIFI_STATUS_CHECK_MS  = 503;
    static constexpr unsigned long WIFI_TIMEOUT_MS       = 90001;
    static constexpr unsigned long WS_RETRY_MS           = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS       = 119993;
    static constexpr unsigned long ESPNOW_TX_INTERVAL_MS = 53;

    // Ajastimet
    unsigned long expn_retry_ms         = WS_RETRY_MS;
    unsigned long next_ws_try_ms        = 0;
    unsigned long last_tx_ms            = 0;
    unsigned long last_read_ms          = 0;
    unsigned long wifi_conn_start_ms    = 0;
    unsigned long wifi_last_check_ms    = 0;
    unsigned long last_espnow_tx_ms     = 0;

    bool sensor_ok = false;
    WifiState wifi_state = WifiState::INIT;

    // === CORE INSTANCES - stack allocated, koko ohjelman elinkaaren ajan ===
    XXXSensor sensor;
    XXXProcessor processor;
    XXXPreferences prefs;
    SignalKBroker signalk;
    ESPNowBroker espnow;
    DisplayManager display;
    WebUIManager webui;

    // Handlerit loop():lle
    void handleWifi(const unsigned long now);
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(const unsigned long now);
    void handleSensorRead(const unsigned long now);
    void handleSignalK(const unsigned long now);
    void handleESPNow(const unsigned long now);
    void handleDisplay();

    void initWifiServices();
};
```

### Implementation (.cpp) - Konstruktori ja riippuvuuksien johdotus

```cpp
#include "XXXApplication.h"
#include "secrets.h"

// KRIITTINEN: Konstruktorin initializer list johdottaa riippuvuudet
XXXApplication::XXXApplication():
  sensor(SENSOR_ADDR),                            // Sensor saa osoitteen
  processor(sensor),                               // Processor saa viittauksen sensoriin
  prefs(processor),                                // Preferences saa viittauksen processoriin
  signalk(processor),                              // SignalK saa viittauksen processoriin
  espnow(processor),                               // ESPNow saa viittauksen processoriin
  display(processor, signalk),                     // Display saa viittaukset processoriin ja signalkiin
  webui(processor, prefs, signalk, display) {}     // WebUI saa viittaukset kaikkiin tarvittaviin

void XXXApplication::begin() {
  // 1. I2C alustus
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(47);
  Wire.setClock(400000);
  delay(47);

  // 2. Naytto ensin (jotta voidaan nayttaa viesteja)
  display.begin();

  // 3. Sensori ja prosessori
  sensor_ok = processor.begin(Wire);

  // 4. Tallennetut asetukset NVS:sta
  prefs.load();

  // 5. Bluetooth pois
  btStop();

  // 6. WiFi AP_STA-tilaan (ESP-NOW tarvitsee taman)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifi_state = WifiState::CONNECTING;
  wifi_conn_start_ms = millis();

  // 7. ESP-NOW alustus (toimii myos ilman WiFi-yhteytta)
  espnow.begin();
}

void XXXApplication::loop() {
  const unsigned long now = millis();
  this->handleWifi(now);
  this->handleOTA();
  this->handleWebUI();
  this->handleWebsocket(now);
  this->handleSensorRead(now);
  this->handleSignalK(now);
  this->handleESPNow(now);
  this->handleDisplay();
}
```

### WiFi-tilan hallinta

WiFi-tilaa hallinnoidaan WifiState enum class -arvolla. Application-luokka on ainoa
joka kutsuu WiFi APIa ja paivittaa tilan. Muut luokat eivat koskaan kutsu WiFi-kirjastoa suoraan.

```cpp
// WifiState.h
#pragma once
#include <Arduino.h>

enum class WifiState : uint8_t {
    INIT            = 0,
    CONNECTING      = 1,
    CONNECTED       = 2,
    FAILED          = 3,
    DISCONNECTED    = 4,
    OFF             = 5
};

static inline const char* wifiStateToString(WifiState state) {
    switch (state) {
        case WifiState::INIT:         return "INIT";
        case WifiState::CONNECTING:   return "CONNECTING";
        case WifiState::CONNECTED:    return "CONNECTED";
        case WifiState::FAILED:       return "FAILED";
        case WifiState::DISCONNECTED: return "DISCONNECTED";
        case WifiState::OFF:          return "OFF";
        default:                      return "UNKNOWN";
    }
}
```

### WiFi-tilakoneen handleri

```cpp
void XXXApplication::handleWifi(const unsigned long now) {
  if ((long)(now - wifi_last_check_ms) < WIFI_STATUS_CHECK_MS) return;
  wifi_last_check_ms = now;

  switch (wifi_state) {
    case WifiState::INIT:
      break;

    case WifiState::CONNECTING: {
      wl_status_t status = WiFi.status();
      if (status == WL_CONNECTED) {
        wifi_state = WifiState::CONNECTED;
        this->initWifiServices();
        expn_retry_ms = WS_RETRY_MS;
      }
      else if ((long)(now - wifi_conn_start_ms) >= WIFI_TIMEOUT_MS) {
        wifi_state = WifiState::OFF;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      }
      else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        wifi_state = WifiState::OFF;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      }
      break;
    }

    case WifiState::CONNECTED: {
      if(!WiFi.isConnected()) {
        wifi_state = WifiState::CONNECTING;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        wifi_conn_start_ms = now;
      }
      break;
    }

    case WifiState::FAILED:
    case WifiState::DISCONNECTED:
    case WifiState::OFF:
      break;
  }
}
```

### Websocket-yhteyden hallinta eksponentiaalisella backoffilla

```cpp
void XXXApplication::handleWebsocket(const unsigned long now) {
  if (wifi_state != WifiState::CONNECTED) return;
  signalk.handleStatus();

  if (!signalk.isOpen() && (long)(now - next_ws_try_ms) >= 0) {
    signalk.connectWebsocket();
    next_ws_try_ms = now + expn_retry_ms;
    expn_retry_ms = min(expn_retry_ms * 2, WS_RETRY_MAX_MS);
  }
  if (signalk.isOpen()) expn_retry_ms = WS_RETRY_MS;
}
```

### WiFi-palveluiden alustus (kutsutaan kerran yhteyden muodostuttua)

```cpp
void XXXApplication::initWifiServices() {
  // SignalK websocket
  signalk.begin();

  // OTA
  ArduinoOTA.setHostname(signalk.getSignalKSource());
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();

  // Web-kayttoliittyma
  webui.begin();
}
```

### Ajastetut handler-kutsut loop():ssa - (long)-castaus pattern

Kaikissa ajastuksissaa kaytetaan `(long)` castaus -patternia ylivuotokestavan ajastuksen saavuttamiseksi:

```cpp
void XXXApplication::handleSensorRead(const unsigned long now) {
  if ((long)(now - last_read_ms) >= READ_MS) {
    last_read_ms = now;
    processor.update();
  }
}

void XXXApplication::handleSignalK(const unsigned long now) {
  if (wifi_state != WifiState::CONNECTED) return;
  if ((long)(now - last_tx_ms) >= MIN_TX_INTERVAL_MS) {
    last_tx_ms = now;
    signalk.sendDelta();  // Implementaatiokohtainen lahetysmetodi
  }
}

void XXXApplication::handleESPNow(const unsigned long now) {
  espnow.processIncomingCommands();  // Kasittele saapuneet komennot
  if ((long)(now - last_espnow_tx_ms) < ESPNOW_TX_INTERVAL_MS) return;
  last_espnow_tx_ms = now;
  espnow.sendDelta();  // Implementaatiokohtainen lahetysmetodi
}
```

### WiFi-riippuvaisten handlerien guardaus

Handlerit jotka vaativat WiFi-yhteyden tarkastavat wifi_state:n ennenkuin tekevat mitaan:

```cpp
void XXXApplication::handleOTA() {
  if (wifi_state != WifiState::CONNECTED) return;
  ArduinoOTA.handle();
}

void XXXApplication::handleWebUI() {
  if (wifi_state != WifiState::CONNECTED) return;
  webui.handleRequest();
}
```

---

## 3. XXXSensor - Sensorin lukukerros

Sensor-luokka vastaa **ainoastaan** fyysisen sensorin kommunikaatiosta. Se ei prosessoi
dataa, ei pidalla tilaa (paitsi yhteysparametrit), eika tieda muista luokista.

### Periaatteet:
- Konstruktori ottaa vain laitteistoparametrit (I2C-osoite, SPI-pinni tms.)
- `begin(Wire)` alustaa yhteyden ja tarkistaa sensorin lasnaolon
- `read(...)` lukee raakadatan float-muuttujiin referenssien kautta
- Palauttaa `bool` onnistumisesta
- Ei sisamann minkaan muun luokan viittausta

### Header

```cpp
#pragma once
#include <Arduino.h>
#include <Wire.h>

class XXXSensor {
public:
    explicit XXXSensor(uint8_t i2c_addr);

    bool begin(TwoWire &wirePort);
    bool available() const;

    // Sensorista riippuvat lukumetodit - raakadata ulos float-viittauksina
    bool read(float &value1, float &value2, float &value3);

    // Mahdolliset komentometodit sensorille
    bool sendCommand(uint8_t cmd);
    uint8_t readRegister(uint8_t reg);

private:
    uint8_t addr;
    TwoWire *wire;

    // Sensorin rekisterikartta
    static constexpr uint8_t REG_DATA_START = 0x02;
    // ... muut rekisterit
};
```

### Implementation

```cpp
#include "XXXSensor.h"

XXXSensor::XXXSensor(uint8_t i2c_addr) : addr(i2c_addr), wire(&Wire) {}

bool XXXSensor::begin(TwoWire &wirePort) {
    wire = &wirePort;
    wire->beginTransmission(addr);
    return (wire->endTransmission() == 0);
}

bool XXXSensor::available() const {
    wire->beginTransmission(addr);
    return (wire->endTransmission() == 0);
}

bool XXXSensor::read(float &value1, float &value2, float &value3) {
    wire->beginTransmission(addr);
    wire->write(REG_DATA_START);
    if (wire->endTransmission(false) != 0) return false;

    // Lue raakadata ja konvertoi float-arvoiksi
    // ... sensorin datasheet mukaisesti
    return true;
}
```

---

## 4. XXXProcessor - Bisneslogiikka ja datan prosessointi

Processor-luokka on vastuussa raakadatan prosessoinnista lahetyskelpoiseksi.
Se omistaa kaiken "aldykkaan" logiikan: suodatuksen, muunnokset, tilahallinnan.

### Periaatteet:
- Ottaa konstruktorissa `&`-viittauksen XXXSensor-luokkaan
- `begin(Wire)` alustaa sensorin kautta
- `update()` lukee sensorilta ja prosessoi datan
- Tarjoaa prosessoidun datan gettereiden kautta
- Tarjoaa datan brokereiden lahetysformaatissa (esim. struct joka sisaltaa radiaaneja)
- Setterit konfiguraatiolle (Preferences ja WebUI kayttavat naita)

### Header

```cpp
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "XXXSensor.h"

class XXXProcessor {
public:
    explicit XXXProcessor(XXXSensor &sensorRef);

    bool begin(TwoWire &wirePort);
    bool update();  // Lue sensorilta ja prosessoi

    // Delta-structit SignalK/ESPNow brokereita varten
    struct SensorDelta {
        float value1 = NAN;
        float value2 = NAN;
        float value3 = NAN;
    };

    auto getDelta() const { return delta; }

    // Getterit (naytto, web UI)
    float getValue1() const { return processed_value1; }
    float getValue2() const { return processed_value2; }
    // ... jne

    // Setterit (Preferences, WebUI)
    void setConfig1(float v) { config1 = v; }
    void setConfig2(float v) { config2 = v; }

private:
    XXXSensor &sensor;
    TwoWire *wire;

    // Prosessoidut arvot
    float processed_value1 = NAN;
    float processed_value2 = NAN;

    // Konfiguraatio
    float config1 = 0.0f;
    float config2 = 0.0f;

    // Delta-struct brokereiden lahetysta varten
    SensorDelta delta;

    void updateDelta();  // Paivita delta-struct prosessoiduista arvoista
};
```

### Prosessoinnin update()-logiikka

```cpp
bool XXXProcessor::update() {
    float raw1, raw2, raw3;
    if (!sensor.read(raw1, raw2, raw3)) return false;

    // Prosessoi raakadata
    // Esim. suodatus, kalibrointi, offsetit, yksikkomuunnokset
    processed_value1 = raw1 + config1;
    // ... jne

    // Paivita delta radiaaneiksi/SI-yksikoksi SignalK:ta varten
    this->updateDelta();
    return true;
}

void XXXProcessor::updateDelta() {
    delta.value1 = processed_value1 * DEG_TO_RAD;
    delta.value2 = processed_value2 * DEG_TO_RAD;
    // ... jne
}
```

---

## 5. SignalKBroker - WebSocket-yhteys SignalK-palvelimeen

SignalKBroker vastaa WebSocket-yhteydesta SignalK-palvelimeen ja delta-viestien
lahettamisesta ja vastaanottamisesta.

### Periaatteet:
- Ottaa konstruktorissa `&`-viittauksen Processor-luokkaan (lukee prosessoitua dataa)
- Omistaa WebsocketsClient-instanssin (ArduinoWebsockets-kirjasto)
- Omistaa uudelleenkaytettavat StaticJsonDocument-instanssit (ArduinoJson-kirjasto)
- `begin()` muodostaa URL:n ja yhteyden
- `handleStatus()` pollataan loop():ssa pitamaan yhteytta yllaa
- Delta-lahetysmetodit ovat implementaatiokohtaisia
- Kayttaa deadband-tarkistusta: lahettaa vain kun arvo on muuttunut riittavasti
- Luo ESP32:n MAC-osoitteeseen perustuvan uniikin source-nimen

### Header

```cpp
#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include "XXXProcessor.h"

namespace websockets {
    class WebsocketsClient;
    class WebsocketsMessage;
    enum class WebsocketsEvent;
}

class SignalKBroker {
public:
    explicit SignalKBroker(XXXProcessor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();
    const char* getSignalKSource() { return SK_SOURCE; }
    bool isOpen() const { return ws_open; }

    // Implementaatiokohtaiset lahetysmetodit
    void sendDelta();
    // void sendXxxDelta();  // Lisaa tarpeen mukaan

private:
    void setSignalKURL();
    void setSignalKSource();
    void onMessageCallback(websockets::WebsocketsMessage msg);
    void onEventCallback(websockets::WebsocketsEvent event);

private:
    XXXProcessor &processor;
    websockets::WebsocketsClient ws;

    // Uudelleenkaytettavat JSON-dokumentit (varattu stackiin)
    StaticJsonDocument<512> delta_doc;
    StaticJsonDocument<1024> incoming_doc;
    StaticJsonDocument<256> subscribe_doc;

    bool ws_open = false;
    char SK_URL[512];
    char SK_SOURCE[32];

    // Deadband-kynnysarvot (radiaaneina)
    static constexpr float DB_THRESHOLD = 0.00436f;  // 0.25 astetta
};
```

### Implementation

```cpp
#include "SignalKBroker.h"
#include "secrets.h"

using namespace websockets;

SignalKBroker::SignalKBroker(XXXProcessor &processorRef)
    : processor(processorRef) {}

bool SignalKBroker::begin() {
    if (strlen(SK_HOST) <= 0 || SK_PORT <= 0) return false;
    this->setSignalKURL();
    this->setSignalKSource();
    return this->connectWebsocket();
}

void SignalKBroker::handleStatus() {
    if (ws_open) {
        ws.poll();
    }
}

bool SignalKBroker::connectWebsocket() {
    ws_open = ws.connect(SK_URL);
    if (ws_open) {
        ws.onMessage([this](WebsocketsMessage msg) {
            this->onMessageCallback(msg);
        });
        ws.onEvent([this](WebsocketsEvent event, const String &data) {
            this->onEventCallback(event);
        });
    }
    return ws_open;
}

void SignalKBroker::closeWebsocket() {
    ws.close();
    ws_open = false;
}

// SignalK URL muodostaminen secrets.h vakioista
void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream", SK_HOST, SK_PORT);
}

// Uniikki source-nimi MAC-osoitteen perusteella
void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(SK_SOURCE, sizeof(SK_SOURCE), "esp32.xxx-%02x%02x%02x", m[3], m[4], m[5]);
    // ^^^ "xxx" tilalle sensorin/laitteen nimi
}

// WebSocket event handler
void SignalKBroker::onEventCallback(WebsocketsEvent event) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            ws_open = true;
            // Mahdolliset subscribe-viestit tahan
            break;
        case WebsocketsEvent::ConnectionClosed:
            ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            ws.pong();
            break;
        default:
            break;
    }
}

// Saapuvan viestin kasittely (esim. subscribe-datan vastaanotto)
void SignalKBroker::onMessageCallback(WebsocketsMessage msg) {
    if (!msg.isText()) return;
    incoming_doc.clear();
    if (deserializeJson(incoming_doc, msg.data())) return;
    // Kasittele saapuva delta implementaation mukaisesti
}
```

### SignalK delta -viestin rakenne ja lahetys

```cpp
void SignalKBroker::sendDelta() {
    if (!ws_open) return;

    auto delta = processor.getDelta();

    // Validoi data
    if (!validf(delta.value1)) return;

    // Deadband-tarkistus: laheta vain kun muuttunut
    static float last_v1 = NAN;
    bool changed = false;
    if (!validf(last_v1) || fabsf(delta.value1 - last_v1) >= DB_THRESHOLD) {
        changed = true;
        last_v1 = delta.value1;
    }
    if (!changed) return;

    // Rakenna SignalK delta JSON
    delta_doc.clear();
    delta_doc["context"] = "vessels.self";
    auto updates = delta_doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = SK_SOURCE;
    auto values  = up.createNestedArray("values");

    // Lisaa path-value pareja lambdalla
    auto add = [&](const char* path, float v) {
        auto o = values.createNestedObject();
        o["path"]  = path;
        o["value"] = v;
    };

    // Implementaatiokohtaiset SignalK polut
    if (changed) add("path.to.your.value", delta.value1);

    if (values.size() == 0) return;

    char buf[640];
    size_t n = serializeJson(delta_doc, buf, sizeof(buf));
    bool ok = ws.send(buf, n);
    if (!ok) {
        ws.close();
        ws_open = false;
    }
}
```

### SignalK delta JSON -formaatti

SignalK-palvelimelle lahetettava viesti noudattaa aina samaa rakennetta:

```json
{
  "context": "vessels.self",
  "updates": [
    {
      "$source": "esp32.xxx-aabbcc",
      "values": [
        { "path": "navigation.headingMagnetic", "value": 1.234 },
        { "path": "navigation.attitude.pitch", "value": 0.05 }
      ]
    }
  ]
}
```

- `context`: aina `"vessels.self"` laivaa varten
- `$source`: ESP32:n uniikki nimi (MAC-pohjainen)
- `values`: taulukko `path`/`value`-pareja
- Arvot SI-yksikossa (radiaanit, kelvinit, metrit/sekunti jne.)
- Polut SignalK-spesifikaation mukaisia (esim. `navigation.headingMagnetic`)

### SignalK subscribe (vastaanotto palvelimelta)

Jos tarvitaan dataa palvelimelta, lahetetaan subscribe-viesti yhteyden avautuessa:

```cpp
void SignalKBroker::subscribeToPath() {
    subscribe_doc.clear();
    subscribe_doc["context"] = "vessels.self";
    auto subscribe = subscribe_doc.createNestedArray("subscribe");
    auto s = subscribe.createNestedObject();
    s["path"] = "path.to.data";
    s["format"] = "delta";
    s["policy"] = "ideal";
    s["period"] = 1000;  // millisekuntia

    char buf[256];
    size_t n = serializeJson(subscribe_doc, buf, sizeof(buf));
    ws.send(buf, n);
}
```

---

## 6. ESPNowBroker - ESP-NOW-protokollan hallinta

ESPNowBroker vastaa ESP-NOW-protokollan kautta tapahtuvasta tiedonsiirrosta
muiden ESP32-laitteiden kanssa. Se on itsenaiinen WiFista (toimii myos ilman WiFi-yhteytta).

Kaikki ESP-NOW-paketit kayttavat jaettua `espnow_protocol.h` -headeria, joka maarittelee
yhteisen pakettiformaatin: 8-tavuinen `ESPNowHeader` + tyypitetty payload `ESPNowPacket<T>` -kaareen sisalla.
Sama header jaetaan kaikkien ESP32-gateway-projektien kesken.

### Periaatteet:
- Ottaa konstruktorissa `&`-viittauksen Processor-luokkaan
- Kayttaa broadcast-lahetysta (FF:FF:FF:FF:FF:FF) kaikille kuulijoille
- Staattiset callback-funktiot (ESP-NOW API vaatii C-tyyliset callbackit)
- Komennon vastaanotto volatile-lipulla + kasittely loop()-kontekstissa
- Sama deadband-logiikka kuin SignalKBrokerissa
- Lahettaa `ESPNowPacket<T>` -rakenteen (header + payload) binaarisena (ei JSON:ia)
- Vastaanotto validoi `ESPNowHeader`:n magic-numeron, msg_type:n ja payload_len:n ennen kasittelya

### Header

```cpp
#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "XXXProcessor.h"
#include "espnow_protocol.h"

class ESPNowBroker {
public:
    explicit ESPNowBroker(XXXProcessor &processorRef);

    bool begin();
    void sendDelta();
    void processIncomingCommands();

private:
    XXXProcessor &processor;
    bool initialized = false;

    // Staattinen tila vastaanotolle (callback -> loop kasittely)
    static uint8_t last_sender_mac[6];
    static volatile bool command_received;

    // Deadband
    float last_v1 = NAN;
    static constexpr float DB_THRESHOLD = 0.00436f;

    static constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // ESP-NOW API:n vaatimat staattiset callbackit
    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);
};
```

### Implementation

```cpp
#include "espnow_protocol.h"
#include "ESPNowBroker.h"
#include <WiFi.h>

// Staattiset muuttujat
uint8_t ESPNowBroker::last_sender_mac[6] = {0};
volatile bool ESPNowBroker::command_received = false;

ESPNowBroker::ESPNowBroker(XXXProcessor &processorRef) : processor(processorRef) {}

bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) return false;

    // Rekisteroi broadcast-peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    // Rekisteroi callbackit
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    initialized = true;
    return true;
}

// Laheta prosessorin delta broadcast-viestina ESPNowPacket-kaareen sisalla
void ESPNowBroker::sendDelta() {
    if (!initialized) return;

    auto delta = processor.getDelta();
    if (!validf(delta.value1)) return;

    // Deadband
    bool changed = false;
    if (!validf(last_v1) || fabsf(delta.value1 - last_v1) >= DB_THRESHOLD) {
        changed = true;
        last_v1 = delta.value1;
    }
    if (!changed) return;

    // Kokoa ESPNowPacket: header + payload
    ESPNow::ESPNowPacket<ESPNow::SensorDelta> pkt;
    initHeader(pkt.hdr, ESPNow::ESPNowMsgType::SENSOR_DELTA, sizeof(ESPNow::SensorDelta));
    memcpy(&pkt.payload, &delta, sizeof(ESPNow::SensorDelta));
    esp_now_send(BROADCAST_ADDR, (const uint8_t*)&pkt, sizeof(pkt));
}

// Kasittele saapunut komento loop()-kontekstissa (EI callbackin sisalla)
void ESPNowBroker::processIncomingCommands() {
    if (!command_received) return;
    command_received = false;

    // Suorita komento
    // Esim: processor.doSomething();

    // Kokoa vastaus-paketti: header + payload
    ESPNow::ESPNowPacket<ESPNow::CommandResponse> pkt;
    initHeader(pkt.hdr, ESPNow::ESPNowMsgType::COMMAND_RESPONSE, sizeof(ESPNow::CommandResponse));
    pkt.payload.success = 1;
    // ... tayta payload implementaation mukaan

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, last_sender_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (!esp_now_is_peer_exist(last_sender_mac)) esp_now_add_peer(&peer);

    esp_now_send(last_sender_mac, (const uint8_t*)&pkt, sizeof(pkt));
}

// STAATTINEN callback - EI saa tehda raskasta tyota
void ESPNowBroker::onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status) {
    // Tyypillisesti tyhja tai debug-logi
}

// STAATTINEN callback - validoi header, aseta lippu, kasittely loop():ssa
void ESPNowBroker::onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    // 1. Tarkista paketin minimikoko
    if (len < (int)sizeof(ESPNow::ESPNowHeader)) return;

    // 2. Lue ja validoi header
    ESPNow::ESPNowHeader hdr;
    memcpy(&hdr, data, sizeof(ESPNow::ESPNowHeader));
    if (hdr.magic != ESPNow::ESPNOW_MAGIC) return;

    // 3. Tarkista payload-koko
    if (len < (int)(sizeof(ESPNow::ESPNowHeader) + hdr.payload_len)) return;

    // 4. Reititys msg_type:n perusteella
    if (static_cast<ESPNow::ESPNowMsgType>(hdr.msg_type) == ESPNow::ESPNowMsgType::SOME_COMMAND) {
        memcpy(last_sender_mac, recv_info->src_addr, 6);
        command_received = true;
    }
}
```

### ESP-NOW-protokollan suunnitteluohjeet:

1. **Pakettiformaatti**: Kaikki paketit kayttavat `ESPNowPacket<T>` (header + payload), maaritelty `espnow_protocol.h`:ssa
2. **Header-validointi**: Vastaanottaja tarkistaa aina: magic, msg_type, payload_len ennen kasittelya
3. **Lahetys**: Broadcast kaikille (`FF:FF:FF:FF:FF:FF`)
4. **Vastaanotto**: Staattiset callbackit validoivat headerin ja asettavat `volatile bool` -lipun
5. **Kasittely**: Lippu tarkistetaan ja kasitellaan `loop()`-kontekstissa (`processIncomingCommands`)
6. **Vastaus**: Unicast-vastaus lahettajan MAC-osoitteeseen, myos `ESPNowPacket`-kaareen sisalla
7. **Data**: Binary struct `ESPNowPacket<T>`, ei JSON:ia (tehokkuus ja nopeus)
8. **WiFi-tila**: `WIFI_AP_STA` tarvitaan jotta WiFi ja ESP-NOW toimivat rinnakkain
9. **Jaettu header**: `espnow_protocol.h` kopioidaan kaikkiin ESP32-gateway-projekteihin — kaikki laitteet kayttavat samaa pakettiformaattia

---

## 7. XXXPreferences - NVS-tallennuksen hallinta

Preferences-luokka vastaa konfiguraation tallennuksesta ESP32:n NVS-muistiin (Non-Volatile Storage).

### Periaatteet:
- Ottaa konstruktorissa `&`-viittauksen Processor-luokkaan
- Omistaa `Preferences`-instanssin (ESP32 Preferences-kirjasto)
- `load()` lataa kaikki asetukset NVS:sta ja asettaa ne Processoriin settereilla
- Jokainen yksittainen save-metodi avaa NVS:n, kirjoittaa ja sulkee sen
- NVS namespace maaritellaan `const char*` memberina

### Header

```cpp
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "XXXProcessor.h"

class XXXPreferences {
public:
    explicit XXXPreferences(XXXProcessor &processorRef);

    void load();
    void saveConfig1(float value);
    void saveConfig2(float value);
    void saveWebPassword(const char* password_sha256_hex);
    bool loadWebPasswordHash(char* out_hash_64bytes);

private:
    const char* ns = "xxx";  // NVS namespace
    Preferences prefs;
    XXXProcessor &processor;
};
```

### Implementation

```cpp
#include "XXXPreferences.h"

XXXPreferences::XXXPreferences(XXXProcessor &processorRef) : processor(processorRef) {}

void XXXPreferences::load() {
    if (!prefs.begin(ns, false)) return;

    processor.setConfig1(prefs.getFloat("config1", 0.0f));
    processor.setConfig2(prefs.getFloat("config2", 0.0f));
    // ... muut asetukset

    prefs.end();
}

void XXXPreferences::saveConfig1(float value) {
    if (!prefs.begin(ns, false)) return;
    prefs.putFloat("config1", value);
    prefs.end();
}

// Web-salasanan tallennus ja lataus NVS:iin SHA256-hashina
void XXXPreferences::saveWebPassword(const char* password_sha256_hex) {
    prefs.begin(ns, false);
    prefs.putString("web_pass", password_sha256_hex);
    prefs.end();
}

bool XXXPreferences::loadWebPasswordHash(char* out_hash_64bytes) {
    if (!prefs.begin(ns, true)) {
        out_hash_64bytes[0] = '\0';
        return false;
    }
    size_t len = prefs.getString("web_pass", out_hash_64bytes, 65);
    prefs.end();
    if (len == 0 || strlen(out_hash_64bytes) != 64) {
        out_hash_64bytes[0] = '\0';
        return false;
    }
    return true;
}
```

---

## 8. WebUIManager - Web-kayttoliittyman hallinta (skeleton)

WebUIManager tarjoaa HTTP-pohjaisen kayttoliittyman ESP32:n konfigurointiin.
Se kayttaa session-pohjaista autentikointia SHA256-salasanalla ja
satunnaisella 128-bittisella session-tokenilla.

### Periaatteet:
- Ottaa konstruktorissa `&`-viittaukset kaikkiin luokkiin joita UI tarvitsee
- Omistaa `WebServer`-instanssin (portissa 80)
- `begin()` rekisteroi reitit ja kaynnistaa palvelimen
- `handleRequest()` wrappaa `server.handleClient()` kutsuttavaksi loop():ssa
- Kaikki konfiguraatioendpointit vaativat autentikoinnin (`requireAuth()`)
- Login, logout ja salasananvaihto eivat vaadi sessiota

### Header (skeleton)

```cpp
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include "XXXProcessor.h"
#include "XXXPreferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"
#include "version.h"

class WebUIManager {
public:
    explicit WebUIManager(
        XXXProcessor &processorRef,
        XXXPreferences &prefsRef,
        SignalKBroker &signalkRef,
        DisplayManager &displayRef
    );

    void begin();
    void handleRequest();

private:
    WebServer server;
    XXXProcessor &processor;
    XXXPreferences &prefs;
    SignalKBroker &signalk;
    DisplayManager &display;

    // Uudelleenkaytettava JSON status-endpointille
    StaticJsonDocument<1024> status_doc;

    // Reittien maaritys
    void setupRoutes();

    // Endpoint handlerit - implementoi naista tarvitsemasi
    void handleRoot();       // Paakonfiguraatiosivu (HTML)
    void handleStatus();     // JSON status API
    void handleRestart();    // ESP32 restart
    // void handleSetXxx();  // Lisaa tarpeen mukaan

    // === SESSION-AUTENTIKOINTI ===
    // (tama kokonaisuus kopioidaan sellaisenaan, se on geneerinen)

    bool requireAuth();
    bool isAuthenticated();
    bool validateSession(const char* token);
    char* createSession();
    void cleanExpiredSessions();
    bool parseSessionToken(const char* cookies, char* token_out_33bytes);
    void sha256Hash(const char* input, char* output_hex_64bytes);
    bool checkLoginRateLimit(uint32_t client_ip);
    void recordFailedLogin(uint32_t client_ip);
    void recordSuccessfulLogin(uint32_t client_ip);
    void cleanOldLoginAttempts();

    void handleLogin();
    void handleLoginPage();
    void handleLogout();
    void handleChangePassword();
    void handleChangePasswordPage();

    struct Session {
        char token[33];
        unsigned long created_ms;
        unsigned long last_seen_ms;
    };

    struct LoginAttempt {
        uint32_t ip_address;
        unsigned long timestamp_ms;
        uint8_t count;
    };

    static constexpr uint8_t MAX_SESSIONS = 3;
    static constexpr unsigned long SESSION_TIMEOUT_MS = 21600000;  // 6 h
    static constexpr uint8_t MAX_LOGIN_ATTEMPTS = 5;
    static constexpr uint8_t MAX_IP_FOLLOWUP = 5;
    static constexpr unsigned long THROTTLE_WINDOW_MS = 60000;     // 1 min
    static constexpr unsigned long LOCKOUT_DURATION_MS = 300000;   // 5 min

    static const char* HEADER_KEYS[1];

    Session sessions[MAX_SESSIONS];
    LoginAttempt login_attempts[MAX_IP_FOLLOWUP];
};
```

### Implementation - Autentikointi (geneerinen, kopioi sellaisenaan)

Autentikoinnin toteutus on taysinnalleen geneerinen eika riipu sensorista.
Se sisaltaa:

1. **Session-hallinta**: 128-bittinen satunnainen token (esp_fill_random), 32 hex-merkkia
2. **SHA256-salasana**: mbedtls-kirjaston avulla
3. **Cookie-pohjainen sessio**: `session=<token>; Path=/; Max-Age=21600; HttpOnly; SameSite=Lax`
4. **Rate limiting**: IP-kohtainen kirjautumisyritysten rajoitus
5. **Session timeout**: 6 tuntia viimeisesta aktiviteetista

Autentikointitoteutus on identtinen referenssiprojektissa (CMPS14-ESP32-SignalK-gateway/WebUIManager.cpp).
Kopioi kaikki seuraavat metodit sellaisenaan uuteen toteutukseen:

- `requireAuth()`, `isAuthenticated()`
- `validateSession()`, `createSession()`, `cleanExpiredSessions()`
- `parseSessionToken()`, `sha256Hash()`
- `checkLoginRateLimit()`, `recordFailedLogin()`, `recordSuccessfulLogin()`, `cleanOldLoginAttempts()`
- `handleLogin()`, `handleLoginPage()`, `handleLogout()`
- `handleChangePassword()`, `handleChangePasswordPage()`

### Implementation - Reittien maaritys (skeleton)

```cpp
void WebUIManager::setupRoutes() {
    // Julkiset reitit (ei autentikointia)
    server.on("/", HTTP_GET, [this]() {
        if (this->isAuthenticated()) {
            server.sendHeader("Location", "/config");
            server.send(302, "text/plain", "");
        } else this->handleLoginPage();
    });
    server.on("/login", HTTP_POST, [this]() { this->handleLogin(); });
    server.on("/logout", HTTP_POST, [this]() { this->handleLogout(); });
    server.on("/changepassword", HTTP_POST, [this]() { this->handleChangePassword(); });

    // Autentikoidut reitit
    server.on("/config", HTTP_GET, [this]() {
        if (!this->requireAuth()) return;
        this->handleRoot();
    });
    server.on("/status", HTTP_GET, [this]() {
        if (!this->requireAuth()) return;
        this->handleStatus();
    });
    server.on("/restart", HTTP_POST, [this]() {
        if (!this->requireAuth()) return;
        this->handleRestart();
    });
    server.on("/changepassword", HTTP_GET, [this]() {
        if (!this->requireAuth()) return;
        this->handleChangePasswordPage();
    });

    // Implementaatiokohtaiset reitit (esim.)
    // server.on("/setting/set", HTTP_POST, [this]() {
    //     if (!this->requireAuth()) return;
    //     this->handleSetSetting();
    // });
}
```

### Implementation - begin() ja handleRequest()

```cpp
const char* WebUIManager::HEADER_KEYS[1] = {"Cookie"};

WebUIManager::WebUIManager(
    XXXProcessor &processorRef,
    XXXPreferences &prefsRef,
    SignalKBroker &signalkRef,
    DisplayManager &displayRef
) : server(80),
    processor(processorRef),
    prefs(prefsRef),
    signalk(signalkRef),
    display(displayRef) {
      for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
          sessions[i].token[0] = '\0';
          sessions[i].created_ms = 0;
          sessions[i].last_seen_ms = 0;
      }
      for (uint8_t i = 0; i < MAX_IP_FOLLOWUP; i++) {
          login_attempts[i].ip_address = 0;
          login_attempts[i].timestamp_ms = 0;
          login_attempts[i].count = 0;
      }
}

void WebUIManager::begin() {
    // Tarkista/alusta oletussalasana
    char stored_hash[65];
    char default_hash[65];
    this->sha256Hash(DEFAULT_WEB_PASSWORD, default_hash);
    if (!prefs.loadWebPasswordHash(stored_hash)) {
        prefs.saveWebPassword(default_hash);
    }
    server.collectHeaders(HEADER_KEYS, 1);
    this->setupRoutes();
    server.begin();
}

void WebUIManager::handleRequest() {
    server.handleClient();
}
```

### Implementation - Status JSON endpoint

```cpp
void WebUIManager::handleStatus() {
    status_doc.clear();

    // Implementaatiokohtaiset arvot
    status_doc["value1"] = processor.getValue1();
    status_doc["value2"] = processor.getValue2();

    // Debug-tiedot
    status_doc["heap_free"]    = ESP.getFreeHeap() / 1024;
    status_doc["heap_total"]   = ESP.getHeapSize() / 1024;
    status_doc["version"]      = SW_VERSION;

    // No-cache headerit
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");

    char out[1048];
    size_t n = serializeJson(status_doc, out, sizeof(out));
    server.send(200, "application/json; charset=utf-8", out);
}
```

### Implementation - Restart handler

```cpp
void WebUIManager::handleRestart() {
    uint32_t ms = 2999;
    if (server.hasArg("ms")) {
        long v = server.arg("ms").toInt();
        if (v >= ms && v < 20000) ms = (uint32_t)v;
    }

    // Laheta restart-sivu HTML:na
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Connection", "close");
    server.send(200, "text/html; charset=utf-8", "");
    // ... restart HTML/JS ...
    server.sendContent("");

    delay(300);
    WiFiClient client = server.client();
    if (client) client.stop();

    if (signalk.isOpen()) signalk.closeWebsocket();

    delay(ms);
    ESP.restart();
}
```

---

## 9. DisplayManager - Nayton ja LED:ien hallinta

DisplayManager on optionaalinen luokka joka vastaa fyysisesta naytosta
(esim. LCD, OLED) ja LED-indikaattoreista. Se on toteutettavissa uudelleen
eri nayttotyypeille pitaen public APIn samana.

### Periaatteet:
- Ottaa konstruktorissa `&`-viittaukset Processoriin ja SignalKBrokeriin
- FIFO-jono viestien naytolle puskurointiin
- `begin()` alustaa nayton
- `handle()` purkaa jonoa ja paivittaa nayton loop()-kutsussa

DisplayManager ei ole geneerinen - se riippuu kaytettavasta naytosta ja LEDeista.
Pidha public API samana (begin, handle, showInfoMessage, showSuccessMessage)
jotta muut luokat (erityisesti WebUIManager) voivat kayttaa sita.

---

## 10. Tukitiedostot

### secrets.h / secrets.example.h

```cpp
#pragma once

// Kopioi secrets.example.h -> secrets.h ja tayta oikeat arvot
inline constexpr const char* WIFI_SSID = "your_wifi_ssid_here";
inline constexpr const char* WIFI_PASS = "your_wifi_password_here";
inline constexpr const char* SK_HOST = "your_signalk_address_here";
inline constexpr uint16_t SK_PORT = 3000;
inline constexpr const char* SK_TOKEN = "your_signalk_auth_token_here";
inline constexpr const char* OTA_PASS = "your_OTA_password_here";
inline constexpr const char* DEFAULT_WEB_PASSWORD = "your_default_web_password_here";
```

### version.h

```cpp
#pragma once
inline constexpr const char* SW_VERSION = "v1.0.0";
```

### espnow_protocol.h - Jaettu ESP-NOW-protokollamaarittely

Tama tiedosto kopioidaan kaikkiin ESP32-gateway-projekteihin. Se maarittelee yhteisen
pakettiformaatin jolla kaikki verkon ESP32-laitteet kommunikoivat.

```cpp
#pragma once
#include <Arduino.h>
#include <cmath>
#include <cstring>

namespace ESPNow {

    // Magic-numero tunnistaa omat paketit muiden ESP-NOW-laitteiden joukosta
    static constexpr uint32_t ESPNOW_MAGIC = 0x45534E57; // 'E''S''N''W'

    // Viestityypit (laajenna kun uusia sensoreita lisataan)
    enum class ESPNowMsgType : uint8_t {
        HEADING_DELTA   = 1,
        BATTERY_DELTA   = 2,
        WEATHER_DELTA   = 3,
        LEVEL_COMMAND   = 10,
        LEVEL_RESPONSE  = 11,
    };

    // 8-tavuinen header kaikille viesteille
    struct ESPNowHeader {
        uint32_t magic;           // ESPNOW_MAGIC
        uint8_t  msg_type;        // ESPNowMsgType
        uint8_t  payload_len;     // payloadin koko tavuina (max 250)
        uint8_t  reserved[2];     // padding, nollataan
    } __attribute__((packed));

    // Payload-structit - maarittele sensorin mukaan
    struct HeadingDelta { /* ... */ };
    struct BatteryDelta { /* ... */ };
    struct WeatherDelta { /* ... */ };
    struct LevelCommand { /* ... */ };
    struct LevelResponse { /* ... */ };

    // Paketti-wrapper: header + payload
    template <typename TPayload>
    struct ESPNowPacket {
        ESPNowHeader hdr;
        TPayload payload;
    } __attribute__((packed));

    // Alustusfunktio headerille
    inline void initHeader(ESPNowHeader& h, ESPNowMsgType type, uint8_t payload_len) {
        h.magic       = ESPNOW_MAGIC;
        h.msg_type    = static_cast<uint8_t>(type);
        h.payload_len = payload_len;
        h.reserved[0] = 0;
        h.reserved[1] = 0;
    }

} // namespace ESPNow
```

Taydelliset payload-structit loytyyvat referenssiprojektista: `CMPS14-ESP32-SignalK-gateway/espnow_protocol.h`.

### Globaalit apufunktiot (harmonic.h / muu vastaava)

```cpp
// Float-validaattori - kaytossa kaikkialla
inline bool validf(float x) { return !isnan(x) && isfinite(x); }

// Lyhyin kulmaero 360 astetta ympyrdlld, radiaaneina
float computeAngDiffRad(float a, float b);
```

---

## 11. Riippuvuuksien johdotuksen yhteenveto

### Luokkien riippuvuusketju (konstruktori-injektio)

```
XXXSensor(addr)
    |
    v
XXXProcessor(sensor&)
    |
    +---> XXXPreferences(processor&)
    +---> SignalKBroker(processor&)
    +---> ESPNowBroker(processor&)
    +---> DisplayManager(processor&, signalk&)
              |
              v
         WebUIManager(processor&, prefs&, signalk&, display&)
```

### Sddnto: Kaikki viittaukset ovat `&`-referensseja, ei pointtereita

Jokainen luokka tallentaa saamansa viittauksen private-memberiksi:

```cpp
class SignalKBroker {
public:
    explicit SignalKBroker(XXXProcessor &processorRef);
private:
    XXXProcessor &processor;  // <-- referenssi, ei pointteri
};

// Konstruktorissa:
SignalKBroker::SignalKBroker(XXXProcessor &processorRef)
    : processor(processorRef) {}
```

### Sddnto: Application omistaa kaiken

```cpp
class XXXApplication {
private:
    XXXSensor sensor;         // Stack allocated
    XXXProcessor processor;   // Stack allocated
    XXXPreferences prefs;     // Stack allocated
    SignalKBroker signalk;    // Stack allocated
    ESPNowBroker espnow;     // Stack allocated
    DisplayManager display;   // Stack allocated
    WebUIManager webui;       // Stack allocated
};

// Konstruktorin initializer list johdottaa kaiken:
XXXApplication::XXXApplication():
  sensor(SENSOR_ADDR),
  processor(sensor),
  prefs(processor),
  signalk(processor),
  espnow(processor),
  display(processor, signalk),
  webui(processor, prefs, signalk, display) {}
```

---

## 12. Arduino-kirjastoriippuvuudet

Seuraavat kirjastot tarvitaan (asennetaan Arduino Library Managerista tai PlatformIO:sta):

| Kirjasto | Kaytto |
|---|---|
| `ArduinoWebsockets` | WebSocket-yhteys SignalK-palvelimeen |
| `ArduinoJson` | JSON serialisointi/deserialisointi |
| `WebServer` (ESP32 built-in) | HTTP-palvelin |
| `Preferences` (ESP32 built-in) | NVS-tallennus |
| `WiFi` (ESP32 built-in) | WiFi-yhteys |
| `Wire` (ESP32 built-in) | I2C-vayla |
| `esp_now` (ESP-IDF) | ESP-NOW-protokolla |
| `mbedtls` (ESP-IDF) | SHA256-hash |
| `ArduinoOTA` (ESP32 built-in) | Over-the-air updates |
| Nayttokirjasto (esim. `LiquidCrystal_I2C`, `Adafruit_SSD1306`) | LCD/OLED |

---

## 13. Uuden toteutuksen tarkistuslista

Kun luodaan uusi XXX-ESP32-SignalK-gateway:

1. [ ] Kopioi tiedostorakenne ja nimeaa uudelleen XXX-prefiksilla
2. [ ] Kopioi `espnow_protocol.h` referenssiprojektista — lisaa tarvittavat payload-structit ja msg_type-arvot
3. [ ] Toteuta `XXXSensor` - sensorin raakadatan lukeminen
4. [ ] Toteuta `XXXProcessor` - datan prosessointi ja delta-structit
5. [ ] Toteuta `XXXPreferences` - NVS-avaimet ja load/save
6. [ ] Muokkaa `SignalKBroker` - oikeat SignalK-polut ja delta-metodit
7. [ ] Muokkaa `ESPNowBroker` - oikea `ESPNowPacket<T>` lahetysdata, msg_type ja komentoprotokolla
8. [ ] Toteuta `WebUIManager` - HTML/JS kayttoliittyma (autentikointi kopioidaan)
9. [ ] Toteuta `DisplayManager` - naytto ja LEDit (tai poista jos ei nayttoa)
10. [ ] Tayta `secrets.example.h` oikeilla vakioilla
11. [ ] Maarittele `version.h`
12. [ ] Maarittele Application-luokan ajastusvakiot
13. [ ] Johdota riippuvuudet Application-konstruktorissa
14. [ ] Toteuta main .ino

---

## 14. ESP32 Board konfiguraatio (Arduino IDE)

Referenssiprojektissa kaytetty SH-ESP32 (Sailor Hat ESP32).
Board-asetukset Arduino IDE:ssa:

- Board: `ESP32 Dev Module` (tai vastaava)
- Partition Scheme: `Default 4MB with spiffs`
- Flash Mode: `QIO`
- Flash Frequency: `80MHz`
- Upload Speed: `921600`
- CPU Frequency: `240MHz`

WiFi-tila: `WIFI_AP_STA` (mahdollistaa ESP-NOW:n WiFi:n rinnalla)
