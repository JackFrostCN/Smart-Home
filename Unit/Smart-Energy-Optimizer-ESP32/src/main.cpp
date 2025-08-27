#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------- OLED Settings ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- Sensor Objects ----------------
Adafruit_BME280 bme;
BH1750 lightMeter;

// ---------------- Pin Definitions ----------------
#define PIR_PIN 27
#define FAN_RELAY_PIN 14
#define LIGHT_RELAY_PIN 12
#define AC_RELAY_PIN 4

// ---------------- Timing ----------------
unsigned long previousMillis = 0;
const long updateInterval = 1000;          // Sensor/OLED update interval
unsigned long lastWeatherAttempt = 0;
const long weatherRetryInterval = 30000;   // Weather update interval
unsigned long lastWiFiCheck = 0;
const long wifiCheckInterval = 10000;      // WiFi reconnect check

// ---------------- System States ----------------
bool fanOn = false;
bool lightOn = false;
bool acOn = false;
float outdoorTemp = -999.0;
float outdoorHum = -999.0;

// ---------------- Calibration & Thresholds ----------------
const float TEMP_OFFSET = -5.0;
const float HUM_OFFSET  = +10.0;
const float FAN_THRESHOLD = 28.0;   // start cooling
const float AC_THRESHOLD  = 30.0;   // strong cooling
const float HUM_THRESHOLD = 70.0;   // high humidity
const float LUX_THRESHOLD = 100.0;

// ---------------- WiFi ----------------
const char* ssid = "ME Staff";
const char* password = "NeTw@2Wsx!";

// Flask backend (your PC IP)
const char* serverUrl = "http://192.168.8.182:5000/api/update";

const String weatherUrl =
  "https://api.openweathermap.org/data/2.5/weather?lat=6.8177&lon=79.8749&appid=eca483009d0e5e53599351b8f8f33a30";

// ---------------- Wi-Fi Functions ----------------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print(F("Connecting to WiFi..."));
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(200);
    Serial.print(F("."));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi Connected!"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\nWiFi Failed, retry later..."));
  }
}

void handleWiFi() {
  if (millis() - lastWiFiCheck < wifiCheckInterval) return;
  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi lost, reconnecting..."));
    WiFi.reconnect();
  }
}

// ---------------- Weather ----------------
void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.begin(weatherUrl);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      outdoorTemp = doc["main"]["temp"].as<float>() - 273.15;
      outdoorHum = doc["main"]["humidity"].as<float>();
      Serial.println(F("Weather updated!"));
    } else {
      Serial.println(F("JSON parse error!"));
    }
  } else {
    Serial.print(F("HTTP error: "));
    Serial.println(httpCode);
  }
  http.end();
}

// ---------------- Display ----------------
void updateDisplay(float temp, float hum, bool motion) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 10);
  display.print(F("Temp:"));
  display.print(temp, 1);
  display.print((char)247);
  display.print(F("C"));

  display.setCursor(81, 10);
  display.print(F("Hum:"));
  display.print(hum, 0);
  display.print(F("%"));

  display.drawLine(0, 20, SCREEN_WIDTH, 20, SSD1306_WHITE);
  display.drawLine(0, 43, SCREEN_WIDTH, 43, SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("Indoor"));
  display.setCursor(0, 22);
  display.print(F("Outdoor"));

  display.setCursor(0, 33);
  if (WiFi.status() != WL_CONNECTED) {
    display.print(F("No WiFi"));
  } else if (outdoorTemp == -999.0) {
    display.print(F("Fetching..."));
  } else {
    display.print(F("Temp:"));
    display.print(outdoorTemp, 1);
    display.print((char)247);
    display.print(F("C"));
    display.setCursor(81, 33);
    display.print(F("Hum:"));
    display.print(outdoorHum, 0);
    display.print(F("%"));
  }

  display.setCursor(0, 46);
  display.print(F("Sys: "));
  display.print(motion ? F("ON") : F("OFF"));

  display.setCursor(0, 56);
  display.print(F("Fan: "));
  display.print(fanOn ? F("ON") : F("OFF"));

  display.setCursor(65, 56);
  display.print(F("Light: "));
  display.print(lightOn ? F("ON") : F("OFF"));

  display.setCursor(65, 46);
  display.print(F("AC: "));
  display.print(acOn ? F("ON") : F("OFF"));

  display.display();
}

// ---------------- Climate Control Logic ----------------
void controlClimate(float Tin, float Hin, float Tout, float Hout) {
  bool useFan = false, useAC = false;

  if (Tin <= 28 && Hin <= 60) {
    useFan = false;
    useAC  = false;
  }
  else if (Hin > HUM_THRESHOLD) {
    if (Hout < Hin && Tout <= Tin) {
      useFan = true;  // ventilation helps
    } else {
      useAC = true;   // only AC can dehumidify
    }
  }
  else if (Tin > AC_THRESHOLD) {
    useAC = true;     // too hot
  }
  else if (Tin > FAN_THRESHOLD) {
    if (Tout < Tin) {
      useFan = true;  // outside cooler
    } else {
      useAC = true;   // outside hotter
    }
  }

  fanOn = useFan && !useAC;
  acOn  = useAC;

  digitalWrite(FAN_RELAY_PIN, fanOn ? LOW : HIGH);
  digitalWrite(AC_RELAY_PIN, acOn ? LOW : HIGH);
}

// ---------------- Send Data to Flask ----------------
void sendDataToServer(float temp, float hum, float lux, bool motion) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["indoor"]["temperature"] = temp;
  doc["indoor"]["humidity"] = hum;
  doc["lightLevel"] = lux;
  doc["motion"] = motion;
  doc["wifi"] = (WiFi.status() == WL_CONNECTED);

  JsonObject devices = doc.createNestedObject("devices");
  devices["fan"]["status"] = fanOn;
  devices["fan"]["manual"] = true;
  devices["ac"]["status"] = acOn;
  devices["ac"]["manual"] = true;
  devices["light"]["status"] = lightOn;
  devices["light"]["manual"] = true;

  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);
  if (httpCode > 0) {
    Serial.printf("POST /update [%d]\n", httpCode);
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED fail"));
    while (true);
  }
  display.clearDisplay();
  display.display();

  if (!bme.begin(0x76) || !lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("Sensor init fail!"));
    while (1);
  }

  pinMode(PIR_PIN, INPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(AC_RELAY_PIN, OUTPUT);

  digitalWrite(FAN_RELAY_PIN, HIGH);
  digitalWrite(LIGHT_RELAY_PIN, HIGH);
  digitalWrite(AC_RELAY_PIN, HIGH);

  connectWiFi();
}

// ---------------- Main Loop ----------------
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    float temp = bme.readTemperature() + TEMP_OFFSET;
    float hum  = bme.readHumidity() + HUM_OFFSET;
    float lux  = lightMeter.readLightLevel();
    bool motion = digitalRead(PIR_PIN);

    // Climate control
    controlClimate(temp, hum, outdoorTemp, outdoorHum);

    // Light logic
    lightOn = motion && (lux < LUX_THRESHOLD);
    digitalWrite(LIGHT_RELAY_PIN, lightOn ? LOW : HIGH);

    // Update display
    updateDisplay(temp, hum, motion);

    // Send to backend
    sendDataToServer(temp, hum, lux, motion);
  }

  if (currentMillis - lastWeatherAttempt >= weatherRetryInterval) {
    lastWeatherAttempt = currentMillis;
    updateWeatherData();
  }

  handleWiFi();
  yield();
}
