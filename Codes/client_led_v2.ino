#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// WiFi AP credentials (ESP-A Access Point)
const char* ssid = "ESP_AP_Server";
const char* password = "12345678";

// Server IP of ESP-A (where /data is hosted)
const char* serverIP = "192.168.4.1";

// ------ LED pins for 4 parking slots ------
// greenPins[i] → GREEN LED for slot (i+1)
// redPins[i]   → RED LED for slot (i+1)
int greenPins[4] = {D0, D2, D5, D7};
int redPins[4]   = {D1, D3, D6, D8};

void setup() {
  Serial.begin(115200);

  // Initialize all LED pins as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(greenPins[i], OUTPUT);
    pinMode(redPins[i], OUTPUT);

    // Default state: slot is FREE → green ON, red OFF
    digitalWrite(greenPins[i], HIGH);
    digitalWrite(redPins[i], LOW);
  }

  // Connect to the Access Point created by ESP-A
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  // Wait until ESP8266 connects to AP
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nLED Controller connected!");
}

void loop() {
  // Ensure WiFi is still connected
  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;
    HTTPClient http;

    // Build request URL → http://192.168.4.1/data
    String url = "http://" + String(serverIP) + "/data";
    http.begin(client, url);

    // Perform HTTP GET request
    int code = http.GET();

    // If data received successfully
    if (code == HTTP_CODE_OK) {
      String data = http.getString();
      Serial.println("DATA:");
      Serial.println(data);

      // Parse lines and update LEDs based on slot status
      parseAndUpdateLEDs(data);
    }

    http.end();  // Free HTTP client
  }

  delay(1000);  // Poll server every 1 sec
}

void parseAndUpdateLEDs(String data) {
  // Data format is multiple lines:
  // slot:1, stat:1, time:20, bill:10.00

  int index = 0;

  while (index < data.length()) {
    // Locate end of current line
    int nl = data.indexOf('\n', index);
    if (nl == -1) nl = data.length();

    // Extract one full line
    String line = data.substring(index, nl);

    // Extract slot ID and status
    int slotID = getValue(line, "slot:", ",").toInt();
    int status = getValue(line, "stat:", ",").toInt();

    // Only valid for slots 1–4
    if (slotID >= 1 && slotID <= 4) {
      int i = slotID - 1;

      // status == 1 → OCCUPIED
      if (status == 1) {
        digitalWrite(redPins[i], HIGH);
        digitalWrite(greenPins[i], LOW);
      }
      // status == 0 → FREE
      else {
        digitalWrite(redPins[i], LOW);
        digitalWrite(greenPins[i], HIGH);
      }
    }

    index = nl + 1;  // Move to next line
  }
}

// Extract a value between a key and end delimiter
// Example: getValue("slot:1, stat:0", "slot:", ",") → "1"
String getValue(String line, String key, String end) {
  int start = line.indexOf(key);
  if (start == -1) return "";

  start += key.length();
  int stop = line.indexOf(end, start);

  // If no end delimiter exists → return until end of line
  return (stop == -1) ? line.substring(start) : line.substring(start, stop);
}
