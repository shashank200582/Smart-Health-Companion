#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "time.h"
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <U8g2lib.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_LIS3DH.h>
#include <math.h>
// ---------------- WIFI + MQTT CONFIG ----------------
const char* WIFI_SSID = "SHANKS";
const char* WIFI_PASS = "shashank1234";
const char* MQTT_HOST = "d734bc451989465f9d07c03830b7e048.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "Miniproject";
const char* MQTT_PASS = "Miniproject@1";
const char* DEVICE_ID = "induscoin_v2_01";
WiFiClientSecure net;
PubSubClient mqttClient(net);
// publish every 5s
const unsigned long MQTT_PUB_INTERVAL = 5000;
unsigned long lastMqttPublish = 0;
// ---------------- PINS / CONSTANTS ----------------
#define SDA_PIN 8
#define SCL_PIN 9
#define LM35_PIN A0
const float ADC_VREF = 3.3;
const int ADC_BITS = 13;
#define BTN_HEALTH 2
#define BTN_SOS 3
MAX30105 particleSensor;
// avoid conflict with SparkFun's BUFFER_SIZE macro
const int HR_BUFFER_SIZE = 100;
uint32_t irBuffer[HR_BUFFER_SIZE];
uint32_t redBuffer[HR_BUFFER_SIZE];
int bufferIndex = 0;
int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRate = 0;
int8_t validHeartRate = 0;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
char line1[32];
char line2[32];
char line3[32];
char line4[32];
float temperatureC = 0.0f;
Adafruit_MPU6050 mpu;
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
Adafruit_LIS3DH lis;
enum SensorType { NONE, MPU6050_TYPE, ADXL345_TYPE, LIS3DH_TYPE };
SensorType sensorType = NONE;
const int SAMPLE_INTERVAL_MS = 40; // ~25 Hz for steps
const int MA_WINDOW = 20;
float stepThreshold = 0.8f;
unsigned long minStepInterval = 300;
float maBuffer[MA_WINDOW];
int maIndex = 0;
float maSum = 0;
bool maFilled = false;
unsigned long lastStepTime = 0;
unsigned long lastSampleTime = 0;
bool wasAbove = false;
unsigned long stepCount = 0;
// --------- Weather / Time ---------
const String city = "Bengaluru";
const String countryCode = "IN";
const String apiKey = "b191c61f37a4e68d72f6fd11046ef35e";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
String weatherTemp = "--";
String weatherHumidity = "--";
String weatherDesc = "--";
unsigned long lastWeatherUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastTempUpdate = 0;
enum DisplayMode { MODE_WEATHER, MODE_HEALTH, MODE_SOS };
DisplayMode displayMode = MODE_WEATHER;
unsigned long modeChangeTime = 0;
unsigned long sosStartTime = 0;
const unsigned long SOS_DISPLAY_MS = 5000;
// for simple edge detection
int lastHealthBtnState = HIGH;
int lastSosBtnState = HIGH;
// ---------------- SENSOR LOGIC ----------------
void detectSensor() {
 if (mpu.begin()) {
 sensorType = MPU6050_TYPE;
 mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
 mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
 return;
 }
 if (adxl.begin()) {
 sensorType = ADXL345_TYPE;
 adxl.setRange(ADXL345_RANGE_2_G);
 return;
 }
 if (lis.begin(0x18) || lis.begin(0x19)) {
 sensorType = LIS3DH_TYPE;
 lis.setRange(LIS3DH_RANGE_2_G);
 return;
 }
 sensorType = NONE;
}
bool readAccelG(float &ax, float &ay, float &az) {
 const float invG = 1.0f / 9.80665f;
 if (sensorType == MPU6050_TYPE) {
 sensors_event_t a, g, t;
 mpu.getEvent(&a, &g, &t);
 ax = a.acceleration.x * invG;
 ay = a.acceleration.y * invG;
 az = a.acceleration.z * invG;
 return true;
 }
 if (sensorType == ADXL345_TYPE) {
 sensors_event_t ev;
 adxl.getEvent(&ev);
 ax = ev.acceleration.x * invG;
 ay = ev.acceleration.y * invG;
 az = ev.acceleration.z * invG;
 return true;
 }
 if (sensorType == LIS3DH_TYPE) {
 sensors_event_t ev;
 lis.getEvent(&ev);
 ax = ev.acceleration.x * invG;
 ay = ev.acceleration.y * invG;
 az = ev.acceleration.z * invG;
 return true;
 }
 return false;
}
void updateSteps() {
 if (sensorType == NONE) return;
 unsigned long now = millis();
 if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
 lastSampleTime = now;
 float ax, ay, az;
 if (!readAccelG(ax, ay, az)) return;
 float mag = sqrt(ax * ax + ay * ay + az * az);
 maSum -= maBuffer[maIndex];
 maBuffer[maIndex] = mag;
 maSum += maBuffer[maIndex];
 maIndex++;
 if (maIndex >= MA_WINDOW) {
 maIndex = 0;
 maFilled = true;
 }
 int denom = maFilled ? MA_WINDOW : (maIndex == 0 ? 1 : maIndex);
 float ma = maSum / denom;
 float hp = mag - ma;
 bool isAbove = (hp > stepThreshold);
 if (isAbove && !wasAbove) {
 if (now - lastStepTime > minStepInterval) {
 stepCount++;
 lastStepTime = now;
 }
 }
 wasAbove = isAbove;
}
float readLM35C() {
 int raw = analogRead(LM35_PIN);
 float voltage = (raw * ADC_VREF) / ((1 << ADC_BITS) - 1);
 return voltage / 0.01f;
}
bool getWeather() {
 if (WiFi.status() != WL_CONNECTED) {
 WiFi.reconnect();
 delay(2000);
 if (WiFi.status() != WL_CONNECTED) return false;
 }
 HTTPClient http;
 String url = "http://api.openweathermap.org/data/2.5/weather?q="
 + city + "," + countryCode
 + "&appid=" + apiKey + "&units=metric";
 http.begin(url);
 int httpResponseCode = http.GET();
 if (httpResponseCode == 200) {
 String payload = http.getString();
 DynamicJsonDocument doc(2048);
 DeserializationError error = deserializeJson(doc, payload);
 if (error) {
 http.end();
 return false;
 }
 weatherTemp = String(doc["main"]["temp"].as<float>(), 1) + "C";
 weatherHumidity = String(doc["main"]["humidity"].as<int>()) + "%";
 weatherDesc = doc["weather"][0]["main"].as<String>();
 http.end();
 return true;
 } else {
 http.end();
 return false;
 }
}
String getFormattedTime() {
 struct tm timeinfo;
 if (!getLocalTime(&timeinfo)) return "--:--";
 char buffer[6];
 strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
 return String(buffer);
}
void computeHRSpO2() {
 int32_t tmpSpO2, tmpHR;
 int8_t tmpValidSpO2, tmpValidHR;
 maxim_heart_rate_and_oxygen_saturation(
 irBuffer, HR_BUFFER_SIZE,
 redBuffer,
 &tmpSpO2, &tmpValidSpO2,
 &tmpHR, &tmpValidHR
 );
 spo2 = tmpSpO2;
 validSPO2 = tmpValidSpO2;
 heartRate = tmpHR;
 validHeartRate = tmpValidHR;
}
void updateMaxSensor() {
 particleSensor.check();
 while (particleSensor.available()) {
 redBuffer[bufferIndex] = particleSensor.getRed();
 irBuffer[bufferIndex] = particleSensor.getIR();
 particleSensor.nextSample();
 bufferIndex++;
 if (bufferIndex >= HR_BUFFER_SIZE) {
 bufferIndex = 0;
 computeHRSpO2();
 }
 }
}
// ---------------- UI ----------------
void drawHealthScreen() {
 u8g2.clearBuffer();
 u8g2.setFont(u8g2_font_5x8_tr);
 u8g2.drawBox(0, 0, 128, 12);
 u8g2.setDrawColor(0);
 u8g2.drawStr(2, 9, "HEALTH DASHBOARD");
 u8g2.setDrawColor(1);
 const int boxW = 60;
 const int boxH = 22;
 u8g2.drawRFrame(2, 14, boxW, boxH, 3); // HR
 u8g2.drawRFrame(66, 14, boxW, boxH, 3); // SpO2
 u8g2.drawRFrame(2, 38, boxW, boxH, 3); // Temp
 u8g2.drawRFrame(66, 38, boxW, boxH, 3); // Steps
 u8g2.setFont(u8g2_font_6x10_tf);
 snprintf(line1, sizeof(line1), "HR");
 snprintf(line2, sizeof(line2), "%3ld bpm",
 (validHeartRate && heartRate > 0) ? heartRate : 0L);
 u8g2.drawStr(6, 24, line1);
 u8g2.drawStr(6, 32, line2);
 snprintf(line1, sizeof(line1), "SpO2");
 if (validSPO2) {
 snprintf(line2, sizeof(line2), "%3ld %%", spo2);
 } else {
 snprintf(line2, sizeof(line2), "-- %%");
 }
 u8g2.drawStr(70, 24, line1);
 u8g2.drawStr(70, 32, line2);
 snprintf(line1, sizeof(line1), "Temp");
 snprintf(line2, sizeof(line2), "%.1f C", temperatureC);
 u8g2.drawStr(6, 48, line1);
 u8g2.drawStr(6, 56, line2);
 snprintf(line1, sizeof(line1), "Steps");
 snprintf(line2, sizeof(line2), "%lu", stepCount);
 u8g2.drawStr(70, 48, line1);
 u8g2.drawStr(70, 56, line2);
 u8g2.sendBuffer();
}
void drawWeatherScreen() {
 String timeStr = getFormattedTime();
 u8g2.clearBuffer();
 u8g2.setFont(u8g2_font_6x10_tf);
 u8g2.drawStr(0, 10, "Smart Health Companion");
 u8g2.drawHLine(0, 12, 128);
 u8g2.setFont(u8g2_font_logisoso18_tr);
 uint8_t w = u8g2.getStrWidth(timeStr.c_str());
 uint8_t x = (128 - w) / 2;
 u8g2.drawStr(x, 34, timeStr.c_str());
 u8g2.setFont(u8g2_font_6x10_tf);
 snprintf(line1, sizeof(line1), "Temp: %s Hum: %s",
 weatherTemp.c_str(), weatherHumidity.c_str());
 u8g2.drawStr(0, 50, line1);
 snprintf(line2, sizeof(line2), "Cond: %s", weatherDesc.c_str());
 u8g2.drawStr(0, 62, line2);
 u8g2.sendBuffer();
}
void drawSOSScreen() {
 u8g2.clearBuffer();
 u8g2.setFont(u8g2_font_7x14B_tf);
 u8g2.drawStr(20, 24, "!!! SOS !!!");
 u8g2.setFont(u8g2_font_6x10_tf);
 u8g2.drawStr(10, 40, "Emergency alert sent");
 u8g2.drawStr(10, 52, "to server with vitals");
 u8g2.sendBuffer();
}
// ---------------- MQTT ----------------
void reconnectMQTT() {
 while (!mqttClient.connected()) {
 Serial.println("Connecting to HiveMQ...");
 if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)) {
 Serial.println("MQTT connected!");
 } else {
 Serial.print("Failed, rc=");
 Serial.println(mqttClient.state());
 delay(2000);
 }
 }
}
void publishTelemetry() {
 float temp = temperatureC;
 int hr = (validHeartRate && heartRate > 0) ? heartRate : 0;
 int s = (int)stepCount;
 int sp = (validSPO2 && spo2 > 0) ? spo2 : 0;
 String topic = String("induscoin/v2/") + DEVICE_ID + "/telemetry";
 String payload = "{";
 payload += "\"temp\":" + String(temp, 2) + ",";
 payload += "\"hr\":" + String(hr) + ",";
 payload += "\"spo2\":" + String(sp) + ",";
 payload += "\"steps\":" + String(s);
 payload += "}";
 mqttClient.publish(topic.c_str(), payload.c_str());
 Serial.println("MQTT Published: " + payload);
}
void publishSOS() {
 float temp = temperatureC;
 int hr = (validHeartRate && heartRate > 0) ? heartRate : 0;
 int s = (int)stepCount;
 int sp = (validSPO2 && spo2 > 0) ? spo2 : 0;
 String topic = String("induscoin/v2/") + DEVICE_ID + "/sos";
 String payload = "{";
 payload += "\"sos\":true,";
 payload += "\"temp\":" + String(temp, 2) + ",";
 payload += "\"hr\":" + String(hr) + ",";
 payload += "\"spo2\":" + String(sp) + ",";
 payload += "\"steps\":" + String(s);
 payload += "}";
 mqttClient.publish(topic.c_str(), payload.c_str());
 Serial.println("MQTT SOS Published: " + payload);
}
// ---------------- SETUP & LOOP ----------------
void setup() {
 Serial.begin(115200);
 delay(500);
 pinMode(BTN_HEALTH, INPUT_PULLUP);
 pinMode(BTN_SOS, INPUT_PULLUP);
 Wire.begin(SDA_PIN, SCL_PIN);
 analogReadResolution(ADC_BITS);
 u8g2.begin();
 u8g2.clearBuffer();
 u8g2.setFont(u8g2_font_6x10_tf);
 u8g2.drawStr(0, 12, "Initializing...");
 u8g2.sendBuffer();
 // WiFi
 WiFi.begin(WIFI_SSID, WIFI_PASS);
 Serial.print("Connecting WiFi");
 while (WiFi.status() != WL_CONNECTED) {
 delay(500);
 Serial.print(".");
 }
 Serial.println("\nWiFi Connected!");
 // NTP
 configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
 getWeather();
 lastWeatherUpdate = millis();
 // MQTT TLS setup
 net.setInsecure(); // for testing only
 mqttClient.setServer(MQTT_HOST, MQTT_PORT);
 // MAX3010x
 if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
 u8g2.clearBuffer();
 u8g2.drawStr(0, 12, "MAX3010x ERROR!");
 u8g2.sendBuffer();
 while (1) { delay(1000); }
 }
 byte ledBrightness = 40;
 byte sampleAverage = 4;
 byte ledMode = 2; // Red + IR
 byte sampleRate = 25; // 25 Hz sample rate
 int pulseWidth = 411;
 int adcRange = 16384;
 particleSensor.setup(ledBrightness, sampleAverage, ledMode,
 sampleRate, pulseWidth, adcRange);
 for (int i = 0; i < MA_WINDOW; i++) maBuffer[i] = 0;
 detectSensor();
 displayMode = MODE_WEATHER;
 drawWeatherScreen();
}
void loop() {
 unsigned long now = millis();
 // MQTT connection maintenance
 if (!mqttClient.connected()) reconnectMQTT();
 mqttClient.loop();
 // ----- Buttons with edge detection -----
 int healthState = digitalRead(BTN_HEALTH);
 int sosState = digitalRead(BTN_SOS);
 // Health button: on press, show health screen for 6s
 if (healthState == LOW && lastHealthBtnState == HIGH) {
 displayMode = MODE_HEALTH;
 modeChangeTime = now;
 }
 // SOS button: on press, send SOS + show SOS screen
 if (sosState == LOW && lastSosBtnState == HIGH) {
 publishSOS();
 displayMode = MODE_SOS;
 sosStartTime = now;
 }
 lastHealthBtnState = healthState;
 lastSosBtnState = sosState;
 // Auto return from health screen
 if (displayMode == MODE_HEALTH && (now - modeChangeTime >= 6000)) {
 displayMode = MODE_WEATHER;
 }
 // Auto return from SOS screen
 if (displayMode == MODE_SOS && (now - sosStartTime >= SOS_DISPLAY_MS)) {
 displayMode = MODE_WEATHER;
 }
 // Weather refresh
 if (now - lastWeatherUpdate >= 60000) {
 getWeather();
 lastWeatherUpdate = now;
 }
 // Temp refresh
 if (now - lastTempUpdate >= 200) {
 temperatureC = readLM35C();
 lastTempUpdate = now;
 }
 // Sensors
 updateSteps();
 updateMaxSensor();
 // OLED refresh
 if (now - lastDisplayUpdate >= 200) {
 if (displayMode == MODE_HEALTH) {
 drawHealthScreen();
 } else if (displayMode == MODE_SOS) {
 drawSOSScreen();
 } else {
 drawWeatherScreen();
 }
 lastDisplayUpdate = now;
 }
 // MQTT telemetry publish every 5 seconds
 if (now - lastMqttPublish >= MQTT_PUB_INTERVAL) {
 publishTelemetry();
 lastMqttPublish = now;
 }
}
