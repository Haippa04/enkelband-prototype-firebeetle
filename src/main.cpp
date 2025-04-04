// Include the required Arduino libraries:
#include "OneWire.h"
#include "DallasTemperature.h"
#include "WiFi.h"
#include <AsyncMqttClient.h>
#include <secret.h>

// Define the OneWire bus pin (use GPIO number for ESP32)
#define ONE_WIRE_BUS D2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define MQTT_PUB_TEMPERATURE "esp/sensor/temperature"
#define MQTT_PUB_WARNING "esp/sensor/warning"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

unsigned long tempBelowThresholdStart = 0;
bool warningSent = false;

// Initialize WiFi
void initWifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

// Connect to MQTT
void connectToMqtt() {
  if (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
  }
}

// Handle successful MQTT connection
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
}

// Handle MQTT disconnection
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT. Reconnecting...");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// Publish MQTT messages
void publishMqttMessages(float temperature) {
  String tempStr = String(temperature, 2); // Omzetten naar string met 2 decimalen
  uint16_t packetIdTemp = mqttClient.publish(MQTT_PUB_TEMPERATURE, 1, true, tempStr.c_str());
  Serial.printf("Publishing on topic %s, packetId: %i, Message: %s\n", MQTT_PUB_TEMPERATURE, packetIdTemp, tempStr.c_str());
  
  if (temperature < 31.0) {
    if (tempBelowThresholdStart == 0) {
      tempBelowThresholdStart = millis(); // Start timer
    }
    if (millis() - tempBelowThresholdStart >= 60000 && !warningSent) { // Check if 1 minute has passed
      String warningMessage = "Warning: Temperature below 31°C for 1 minute!";
      mqttClient.publish(MQTT_PUB_WARNING, 1, true, warningMessage.c_str());
      Serial.println(warningMessage);
      warningSent = true;
    }
  } else {
    tempBelowThresholdStart = 0; // Reset timer if temperature goes above 31
    warningSent = false;
  }
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  
  // Start WiFi en MQTT timers
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(initWifi));

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  initWifi();
  connectToMqtt();
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");

  publishMqttMessages(tempC);

  delay(5000);
}
