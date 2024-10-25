#include <Arduino.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266HTTPClient.h>
#include <SmartThing.h>
#include <ArduinoJson.h>

#define BUTTON_PIN D7

#define CONF_SENSOR_IP "snsr"

void addActions();
void addSensors();

void setupRest();
void printSensors();
void fetchSensors();

enum MODES {
  SENSORS_MODE,
  MY_IP_MODE,
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

int mode = FISRT_MODE;
bool btnPressed = false;
bool needUpdate = true;
bool sleep = false;
long lastUpdate;
long lastAction;

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.printf("Initializating...");
  lcd.setCursor(0,1);

  setupRest();
  addActions();
  addSensors();
  SmartThing.addConfigEntry(CONF_SENSOR_IP, "Sensor ip for first fetch", "string");

  if (SmartThing.init("meteo_display")) {
    LOGGER.info("main", "SmartThing successfully initialized");
  } else {
    LOGGER.error("main", "Failed to init SmartThing!");
  }

  ArduinoOTA.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

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

  SmartThing.loop();
  ArduinoOTA.handle();
  delay(200);
}

void addActions() {
  // todo enable backlight?
}

void addSensors() {
  SmartThing.addSensor("last_update", [&]() {
    return (millis() - lastUpdate) / 1000;
  });
  SmartThing.addDigitalSensor("button", BUTTON_PIN);
}

int extractSensorValue(JsonDocument doc, const char * name) {
  if (doc.containsKey(name)) {
    return doc[name];
  }
  return -1;
}

void fetchSensors() {
  lastUpdate = millis();

  String sensorIp = STSettings.getConfig()[CONF_SENSOR_IP];
  if (sensorIp.isEmpty()) {
    LOGGER.warning("main", "Empty sensor ip, skipping first fetch");
    return;
  }

  String url = "http://" + sensorIp + "/sensors";
  LOGGER.info("main", "Trying to fetch sensors values from %s", url.c_str());

  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin(wifiClient, url);
  if (httpClient.GET() !=  200) {
    LOGGER.error("main", "Faied to fetch sensors values");
    sensorValues.temp = -1;
    sensorValues.hum = -1;
    sensorValues.pressure = -1;
    return;
  }

  String response = httpClient.getString();
  LOGGER.info("main", "Got response: %s", response.c_str());
  JsonDocument doc;
  deserializeJson(doc, response);

  sensorValues.temp = extractSensorValue(doc, "temperature");
  sensorValues.hum = extractSensorValue(doc, "humidity");
  sensorValues.pressure = extractSensorValue(doc, "pressure");

  LOGGER.info("main", "Got sensors values: temp=%d hum=%d pressure=%d", sensorValues.temp, sensorValues.hum, sensorValues.pressure);
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
  RestController.getWebServer()->on("/temp", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request->hasArg("value")) {
      sensorValues.temp = atoi(request->arg("value").c_str());
      LOGGER.debug("main", "Got new value! temp=%d", sensorValues.temp);
      needUpdate = true;
      lastUpdate = millis();
      request->send(200);
    } else {
      request->send(400);
    }
  });
  RestController.getWebServer()->on("/hum", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request->hasArg("value")) {
      sensorValues.hum = atoi(request->arg("value").c_str());
      LOGGER.debug("main", "Got new value! hum=%d", sensorValues.hum);
      needUpdate = true;
      lastUpdate = millis();
      request->send(200);
    } else {
      request->send(400);
    }
  });
  RestController.getWebServer()->on("/pressure", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request->hasArg("value")) {
      sensorValues.pressure = atoi(request->arg("value").c_str());
      LOGGER.debug("main", "Got new value! pressure=%d", sensorValues.pressure);
      needUpdate = true;
      lastUpdate = millis();
      request->send(200);
    } else {
      request->send(400);
    }
  });
}