#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"

const int analogPin = 35;        // GPIO4 (D4) ESP32
const float Vcc = 3.3;          // Napięcie zasilania dzielnika
const float R_fixed = 11000;  // Rezystor 10kΩ
const float A = 1.129148e-3;
const float B = 2.34125e-4;
const float C = 8.76741e-8;

const char* ssid = "POCO1";
const char* password = "Kotlet12345";
const char* mqtt_server = "10.202.98.157";  // IP Raspberry Pi

WiFiClient espClient;
PubSubClient client(espClient);
MAX30105 particleSensor;
void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // ESP32: 0–4095
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  //Pulse sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }

  //Setup to sense a nice looking saw tooth on the plotter
  byte ledBrightness = 0x1F; //Options: 0=Off to 255=50mA
  byte sampleAverage = 32; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 3; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 1000; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void loop() {
  
  float raw = analogRead(analogPin);
  if (!client.connected()) reconnect();
  // ADC -> napięcie
  float Vout = (raw * Vcc) / 4095.0;

  // Obliczanie rezystancji NTC (pull-up: R_fixed -> 3.3V)
  float R_ntc = R_fixed * (Vout / (Vcc - Vout));

  // Steinhart–Hart:
  float lnR = log(R_ntc);
  float invT = A + B * lnR + C * lnR * lnR * lnR; // 1/T
  float temperatureK = 1.0 / invT;               // Kelvin
  float temperatureC = temperatureK - 273.15;    // Celsius

  char payload[32];
  sprintf(payload, "%.2f", temperatureC);
  client.publish("sensor/temperature", payload);
  
  char payload2[16];
  sprintf(payload2, "%ld", particleSensor.getIR());
  client.publish("sensor/pulse", payload2);
}