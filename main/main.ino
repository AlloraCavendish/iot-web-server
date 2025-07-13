#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

const char* ssid = "PYAN_5G@unifi";
const char* password = "0E2E41FFB3";

const char* websocket_host = "websocket-relayserver.onrender.com";
const int websocket_port = 443;
const char* websocket_path = "/";

WebSocketsClient webSocket;

const int ledPin = LED_BUILTIN; // Active LOW
const int sensorPin = A0; // Analog sensor pin
const int buttonPin = D1; // Digital button pin (change as needed)

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long sensorInterval = 5000; // Send sensor data every 5 seconds
unsigned long lastButtonState = HIGH;
unsigned long buttonDebounceTime = 0;
const unsigned long debounceDelay = 50;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[WSS] Connected to server");
      webSocket.sendTXT("ESP8266");  // identify as ESP8266 to server
      break;

    case WStype_DISCONNECTED:
      Serial.println("[WSS] Disconnected");
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);
      Serial.printf("[WSS] Received: %s\n", msg.c_str());

      if (msg == "on") {
        digitalWrite(ledPin, LOW);
        webSocket.sendTXT("ON");   // respond with status
      } else if (msg == "off") {
        digitalWrite(ledPin, HIGH);
        webSocket.sendTXT("OFF");  // respond with status
      }

      break;
    }
  }
}

void sendSensorData() {
  // Read analog sensor (0-1024)
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (3.3 / 1024.0); // Convert to voltage
  
  // Read digital button
  int buttonState = digitalRead(buttonPin);
  
  // Get WiFi signal strength
  int rssi = WiFi.RSSI();
  
  // Create JSON payload for Firebase
  StaticJsonDocument<200> doc;
  doc["type"] = "sensor_data";
  doc["sensor_value"] = sensorValue;
  doc["voltage"] = voltage;
  doc["button_state"] = (buttonState == LOW) ? "PRESSED" : "RELEASED";
  doc["wifi_rssi"] = rssi;
  doc["uptime"] = millis();
  doc["device_id"] = "ESP8266_001";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send to relay server
  webSocket.sendTXT(jsonString);
  
  Serial.println("Sent sensor data: " + jsonString);
}

void sendButtonEvent() {
  // Create JSON payload for button event
  StaticJsonDocument<150> doc;
  doc["type"] = "button_event";
  doc["button_state"] = "PRESSED";
  doc["timestamp"] = millis();
  doc["device_id"] = "ESP8266_001";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send to relay server
  webSocket.sendTXT(jsonString);
  
  Serial.println("Button pressed - sent: " + jsonString);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // Use internal pullup
  digitalWrite(ledPin, HIGH); // LED OFF

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());

  // Setup WebSocket client for WSS
  webSocket.beginSSL(websocket_host, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // try reconnecting every 5s
}

void loop() {
  webSocket.loop();
  
  // Send sensor data periodically
  if (millis() - lastSensorRead >= sensorInterval) {
    sendSensorData();
    lastSensorRead = millis();
  }
  
  // Check button state (with debouncing)
  int buttonReading = digitalRead(buttonPin);
  
  if (buttonReading != lastButtonState) {
    buttonDebounceTime = millis();
  }
  
  if ((millis() - buttonDebounceTime) > debounceDelay) {
    if (buttonReading == LOW && lastButtonState == HIGH) {
      // Button was pressed
      sendButtonEvent();
    }
  }
  
  lastButtonState = buttonReading;
  
  delay(50); // Small delay to prevent excessive polling
}