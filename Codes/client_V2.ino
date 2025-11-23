#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define NUM_SLOTS 4   // Total number of parking slots

const float RATE_PER_SECOND = 0.5; // Billing rate per second ($)

// --- AP/Server Configuration ---
const char *ssid = "ESP_AP_Server";      // Access Point SSID
const char *password = "12345678";       // Access Point Password
const char *serverIP = "192.168.4.1";    // Server IP Address

// Structure to hold data for a single parking slot
struct Slot {
  int slotID;
  int trigPin;          // Ultrasonic trigger pin
  int echoPin;          // Ultrasonic echo pin
  int status;           // 0 = Free, 1 = Occupied
  unsigned long startTime;      // Timestamp when slot becomes occupied
  unsigned long occupiedTime;   // Total time the slot has been occupied
  float Bill;                   // Calculated bill
};

// Slot Definitions (4 Slots with assigned pins)
Slot slots[NUM_SLOTS] = {
  {1, D0, D1, 0, 0, 0, 0.0},   // D0=GPIO16, D1=GPIO5
  {2, D2, D5, 0, 0, 0, 0.0},   // D2=GPIO4, D5=GPIO14
  {3, D6, D7, 0, 0, 0, 0.0},   // D6=GPIO12, D7=GPIO13
  {4, D3, D4, 0, 0, 0, 0.0}    // D3=GPIO0, D4=GPIO2
};

// --- Read distance from ultrasonic sensor ---
long readUltrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);        // Trigger pulse
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);  // Read echo time
  long distanceCm = duration * 0.034 / 2;  // Convert to cm

  return distanceCm;
}

// --- URL Encoder for sending formatted slot data ---
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char bufHex[4];

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);

    if (isalnum(c)) {
      encoded += c;                   // Keep alphanumeric characters
    } else {
      sprintf(bufHex, "%%%02X", c);   // Convert to HEX for URL encoding
      encoded += bufHex;
    }
  }
  return encoded;
}

// --- Update Slot Information ---
void updateSlot(Slot &slot, int index, String &message) {
  long distance = readUltrasonicDistance(slot.trigPin, slot.echoPin);
  int prevStatus = slot.status;

  // Detect occupancy
  slot.status = (distance < 3) ? 1 : 0;

  // Detect transition from free → occupied
  if (slot.status == 1 && prevStatus == 0) {
    slot.startTime = millis();  // Start timer for occupied slot
  }

  // If occupied, update time and bill
  if (slot.status == 1) {
    slot.occupiedTime = (millis() - slot.startTime) / 1000;
    slot.Bill = RATE_PER_SECOND * slot.occupiedTime;
    message += String(index + 1) + "," +
               String(slot.status) + "," +
               String(slot.occupiedTime) + "," +
               String(slot.Bill, 2) + "\n";
  } else {
    // Slot is free
    message += String(index + 1) + ",0,0,0\n";
  }
}

// --- Send data to the server ---
void sendToServer(const String &message) {
  String encoded = urlEncode(message);
  String url = "http://" + String(serverIP) + "/send?message=" + encoded;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);  // Start HTTP request
  http.GET();               // Send GET request
  http.end();               // Close connection
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure pins for ultrasonic sensors
  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(slots[i].trigPin, OUTPUT);
    pinMode(slots[i].echoPin, INPUT);
  }

  // Connect to Access Point
  Serial.print("Connecting to AP: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected successfully to Server AP!");
  Serial.print("Client IP address: ");
  Serial.println(WiFi.localIP());
}
void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    String message = "";

    // Update sensors one at a time with spacing
    for (int i = 0; i < NUM_SLOTS; i++) {
      updateSlot(slots[i], i, message);
      delay(60);  // <-- IMPORTANT: avoid ultrasonic interference
    }

    sendToServer(message);
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }

  delay(500);
}
