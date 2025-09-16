#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Replace with your network credentials
const char* ssid = "POCO X3 Pro";
const char* password = "xxxx";

const char* serverName = "https://api.digitransit.fi/routing/v2/hsl/gtfs/v1";
time_t      nowTime;

// Example GraphQL query body
const char* query = R"(
{
  stop(id: "HSL:1113140") {
    name
    stoptimesWithoutPatterns(numberOfDepartures: 3) {
      scheduledDeparture
      realtimeDeparture
      trip {
        routeShortName
      }
    }
  }
}
)";

void otaSetup()
{
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // Serial.begin(115200);
  otaSetup();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

String formatTime(int arv) {
  int hours = arv / 3600;
  int minutes = (arv % 3600) / 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", hours, minutes);
  return String(buf);
}

String formatMinute(int interval) {
  int minutes = interval / 60;
  char buf[6];
  sprintf(buf, "%d", minutes);
  return String(buf);
}

void showData(String stop, String route, int arv, int interval) {
  display.println(stop);
  display.printf("Bus %s at %s in %s minutes\n", route.c_str(), formatTime(arv).c_str(), formatMinute(interval).c_str());
  display.println("");
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // skip TLS cert validation

    HTTPClient https;
    if (https.begin(client, serverName)) {
      https.addHeader("digitransit-subscription-key", "8f9f68d614d34e39beebadbb815ebcf9");
      https.addHeader("Content-Type", "application/json");

      // Send POST request with GraphQL query
      String body = F("{\"query\": \"{ stop(id: \\\"HSL:1113140\\\") { name stoptimesWithoutPatterns(numberOfDepartures: 3) { scheduledArrival realtimeArrival serviceDay trip { routeShortName } } } }\"}");
      int httpCode = https.POST(body);
      // int httpCode = https.POST(String("{\"query\": \"") + query + "\"}");
      if (httpCode > 0) {
        String payload = https.getString();
        Serial.println(payload);

        // Parse JSON
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);

        const char* stopName = doc["data"]["stop"]["name"];
        Serial.println("Stop: " + String(stopName));

        JsonArray times = doc["data"]["stop"]["stoptimesWithoutPatterns"];
        display.clearDisplay();
        display.setCursor(0,0);
        for (JsonObject t : times) {
          int arrival = t["realtimeArrival"];
          int today = t["serviceDay"];
          time(&nowTime);
          int waitTime = today + arrival - nowTime;
          const char* route = t["trip"]["routeShortName"];
          showData(stopName, route, arrival, waitTime);
        }
        display.display();
        delay(5000);
      } else {
        Serial.printf("Request failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  }
  // delay(30000); // update every 30s
}
