#include <ZMPT101B.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SENSITIVITY 569.5
#define DHTPIN 19
#define DHTTYPE DHT11
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 16
#define OLED_CS 5
#define OLED_RESET 17

ZMPT101B voltageSensor(4, 50.0);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "DEI";
const char* password = "Shubham@12";

bool fan = false;
bool light = false;


WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

DynamicJsonDocument routinesDoc(1024);
bool routinesExecuted = false;

float getVoltage() {
  int r = random(0, 3001);
  float voltage = map(r, 0, 3000, 2180, 2210) / 10.0;
  return voltage;
}

void handleSensorDataRequest() {
  // Prepare JSON response
  DynamicJsonDocument doc(1024);
  float volts = getVoltage();
  float amp = 0.0;
  if (fan == true)
    amp = 0.178;
  else if (fan == true && light == true)
    amp - 0.598;
  else if (light == true)
    amp = 0.42;
  else
    amp = 0.0;
  doc["voltage"] = volts;
  doc["amperage"] = amp;
  doc["watts"] = "watts";

  String response;
  serializeJson(doc, response);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}


void sendtoArduino(const String& message) {
  Serial.println(message);
  delay(1500);
}

void showTemp() {
  float humidity = dht.readHumidity();  // Read humidity value
  float temperature = dht.readTemperature();
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print("Humidity: ");
  display.println(humidity);
  display.print("Temp: ");
  display.println(temperature);
  display.display();
}


void fanUpdate(const String& action, int speed) {
  if (action == "ON") {
    // Serial.print("Turning on Fan with speed: ");
    // digitalWrite(15,LOW);
    // Serial.println(speed);
    fan = true;
    show("Turning on Fan");
    sendtoArduino("F_ON");
    delay(2000);
    showTemp();
  } else {
    // Serial.println("Turning off Fan");
    // digitalWrite(15,HIGH);
    fan = false;
    show("Turning off Fan");
    sendtoArduino("F_OFF");
    delay(2000);
    showTemp();
  }
}

void lightUpdate(const String& action) {
  if (action == "ON") {
    // Serial.println("Turning on Light");
    // digitalWrite(RELAY_LIGHT,LOW);
    light = true;
    show("Turning on Light");
    sendtoArduino("L_ON");
    delay(2000);
    showTemp();
  } else {
    // Serial.println("Turning off Light");
    // digitalWrite(RELAY_LIGHT,HIGH);
    light = false;
    show("Turning off Light");
    sendtoArduino("L_OFF");
    delay(2000);
    showTemp();
  }
}

void handleUpdate() {
  if (server.method() == HTTP_POST && server.uri() == "/update") {
    // Serial.println("Recieved");
    String body = server.arg("plain");
    // Serial.println("Received body:");
    // Serial.println(body);

    DynamicJsonDocument jsonDocument(1024);
    DeserializationError error = deserializeJson(jsonDocument, body);

    if (error) {
      // Serial.print("JSON parsing error: ");
      // Serial.println(error.c_str());
      server.send(400, "text/plain", "Bad Request");
      return;
    }
    delay(1000);
    if (jsonDocument.containsKey("routines")) {
      // Serial.println("UPdating routines");
      JsonObject routines = jsonDocument["routines"];
      // Serial.println("Fetched routines");
      routinesDoc.clear();
      for (JsonPair keyValue : routines) {
        routinesDoc[keyValue.key()] = keyValue.value();
      }
      String serializedRoutines;
      serializeJson(routinesDoc, serializedRoutines);
      // Serial.println("Updated routines:");
      // Serial.println(serializedRoutines);
      server.send(200, "text/plain", "Data processed successfully");
    }

    else if (jsonDocument.containsKey("device")) {
      String device = jsonDocument["device"];
      if (device == "light") {
        String action = jsonDocument["action"];
        String lightValue = (action == "true") ? "ON" : "OFF";  // Convert bool to string
        lightUpdate(lightValue);
      } else if (device == "fan") {
        String action = jsonDocument["action"];
        String fanValue = (action == "true") ? "ON" : "OFF";  // Convert bool to string
        if (jsonDocument.containsKey("speed")) {
          int speed = jsonDocument["speed"];  // Assuming "speed" is an integer field
          fanUpdate(fanValue, speed);
        } else {
          fanUpdate(fanValue, 0);
        }
      }

      server.send(200, "text/plain", "Data processed successfully");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  }
}
void httpTask(void* parameter) {
  server.onNotFound([]() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "text/plain", "Not found");
  });

  server.on("/update", HTTP_POST, handleUpdate);
  server.on("/reqdata", HTTP_GET, handleSensorDataRequest);

  server.begin();
  // Serial.println("HTTP server started");

  while (true) {
    server.handleClient();
    delay(1);  // Allow FreeRTOS to switch tasks
  }
}

void processingTask(void* parameter) {
  for (;;) {
    timeClient.update();
    String currentTime = timeClient.getFormattedTime();
    // Serial.println(currentTime);
    currentTime.remove(2, 1);
    if (currentTime.length() >= 4) {
      currentTime = currentTime.substring(0, 4);
      // Serial.println(currentTime);
      if (routinesDoc.containsKey(currentTime)) {
        if (!routinesExecuted) {
          JsonArray routines = routinesDoc[currentTime].as<JsonArray>();
          for (const auto& value : routines) {
            String routine = value.as<String>();
            // Serial.println(routine);
            if (routine.substring(0, 1) == "L") {
              // Serial.println("Light routine");
              // Serial.println(routine.substring(4, 6));
              if (routine.substring(4, 6) == "ON")
                lightUpdate("ON");
              else
                lightUpdate("OFF");
            } else if (routine.substring(0, 1) == "F") {
              // Serial.println("Fan Routine");
              // Serial.println(routine.substring(4, 6));
              if (routine.substring(4, 6) == "ON")
                fanUpdate("ON", 5);
              else
                fanUpdate("OFF", 0);
            }
          }
          routinesExecuted = true;
        } else {
          routinesExecuted = false;
        }
      }

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(millis());
  voltageSensor.setSensitivity(SENSITIVITY);
  // Serial.println("init");
  delay(2000);
  // Serial.println(voltageSensor.getRmsVoltage());

  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 10);
  display.println("Starting Up...");
  display.display();


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Serial.println("\nWiFi connected");


  timeClient.begin();
  timeClient.setTimeOffset(19800);

  xTaskCreatePinnedToCore(httpTask, "HTTP Task", 10000, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(processingTask, "Processing Task", 10000, NULL, 1, NULL, 1);
}

void show(const String& text) {
  // Serial.println(text);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(text);
  display.display();
  delay(300);
  display.clearDisplay();
}


void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
