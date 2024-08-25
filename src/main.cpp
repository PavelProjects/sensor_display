#include <Arduino.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "ESP8266WebServer.h"
#include <ArduinoJson.h>

#define BUTTON_PIN D7
#define SSID "PUK"
#define PASSWORD "donttouch"
#define SENSOR_IP "192.168.2.107"
#define SENSOR_URL "http://192.168.2.107/sensors"

void setupRest();
void printSensors();
void fetchSensors();

enum MODES {
  SENSORS_MODE,
  MY_IP_MODE,
  SENSOR_IP_MODE,
  INFO_MODE,
  FISRT_MODE = SENSORS_MODE,
  LAST_MODE = INFO_MODE
};
struct SENSOR_VALUES {
  int temp;
  int hum;
  int pressure;
} sensorValues;


LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP8266WebServer server(80);

int mode = FISRT_MODE;
bool btnPressed = false;
bool needUpdate = true;
bool sleep = false;
long lastUpdate;
long lastAction;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.begin(SSID, PASSWORD);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.printf("Connecting");
  lcd.setCursor(0,1);

  while(WiFi.status() != WL_CONNECTED) {
    lcd.print(".");
    delay(1000);
  }
  setupRest();

  ArduinoOTA.begin();
  server.begin();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fetching values");
  fetchSensors();
}

void loop() {
  int btn = digitalRead(BUTTON_PIN);
  if (btn == 0 && !btnPressed) {
    if (sleep) {
      sleep = false;
      lcd.backlight();
    } else {
      mode = mode == LAST_MODE ? FISRT_MODE : mode + 1;
      needUpdate = true;
    }
    btnPressed = true;
    lastAction = millis();
  } else if (btn == 1 && btnPressed) {
    btnPressed = false;
  }

  if (!sleep && millis() - lastAction > 15000) {
    sleep = true;
    lcd.noBacklight();
  }

  if (needUpdate || mode == INFO_MODE) {
    lcd.clear();
    
    switch(mode) {
      case SENSORS_MODE:
        printSensors();
        break;
      case MY_IP_MODE:
        lcd.setCursor(5, 0);
        lcd.print("My ip:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        break;
      case SENSOR_IP_MODE:
        lcd.setCursor(3, 0);
        lcd.print("Sensor ip:");
        lcd.setCursor(0, 1);
        lcd.print(SENSOR_IP);
        break;
      case INFO_MODE:
        lcd.setCursor(2, 0);
        lcd.print("Last update");
        lcd.setCursor(0, 1);
        lcd.print((millis() - lastUpdate) / 1000);
        lcd.print(" sec ago");
        break;
      default:
        mode = FISRT_MODE;
    }
    needUpdate = false;
  }

  server.handleClient();
  ArduinoOTA.handle();
  delay(200);
}

int extractSensorValue(JsonDocument doc, const char * name) {
  if (doc.containsKey(name) && doc[name].containsKey("value")) {
    return doc[name]["value"];
  }
  return -1;
}

void fetchSensors() {
  lastUpdate = millis();

  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin(wifiClient, SENSOR_URL);
  if (httpClient.GET() !=  200) {
    sensorValues.temp = -1;
    sensorValues.hum = -1;
    sensorValues.pressure = -1;
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, httpClient.getString());

  sensorValues.temp = extractSensorValue(doc, "temperature");
  sensorValues.hum = extractSensorValue(doc, "humidity");
  sensorValues.pressure = extractSensorValue(doc, "pressure");
}

void printSensors() {
  lcd.setCursor(0, 0);
  lcd.printf("Temp:%d", sensorValues.temp);
  lcd.write(223);

  lcd.setCursor(9, 0);
  lcd.printf("Hum:%d%%", sensorValues.hum);
  
  lcd.setCursor(1, 1);
  lcd.printf("Pressure: %d", sensorValues.pressure);
}

void setupRest() {
  server.on("/temp", HTTP_POST, []() {
    if (server.hasArg("value")) {
      sensorValues.temp = atoi(server.arg("value").c_str());
      needUpdate = true;
    }
    lastUpdate = millis();
    server.send(200);
  });
  server.on("/hum", HTTP_POST, []() {
    if (server.hasArg("value")) {
      sensorValues.hum = atoi(server.arg("value").c_str());
      needUpdate = true;
    }
    lastUpdate = millis();
    server.send(200);
  });
  server.on("/pressure", HTTP_POST, []() {
    if (server.hasArg("value")) {
      sensorValues.pressure = atoi(server.arg("value").c_str());
      needUpdate = true;
    }
    lastUpdate = millis();
    server.send(200);
  });
}