#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <math.h>
#include <time.h>

// --- OLED Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Wi-Fi + ThingSpeak ---
const char* ssid = "Wokwi-GUEST";   // For Wokwi simulation
const char* password = "";          
String apiKey = "OPWZAEUHBS7JGB5F"; // Your ThingSpeak Write API Key
const char* serverName = "http://api.thingspeak.com";

// --- DS18B20 Setup ---
#define ONE_WIRE_BUS 4   // GPIO4 → DS18B20 data pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- MPU6050 Setup ---
MPU6050 mpu;
bool fallDetected = false;

// --- GPS (NEO-6M via UART2 on GPIO16/17) ---
TinyGPSPlus gps;
HardwareSerial SerialGPS(2); // UART2 for GPS
#define RXD2 16
#define TXD2 17

// --- SOS Button Setup ---
#define SOS_BUTTON 13
bool sosActivated = false;

// --- Vitals ---
float heartRate;
float temperature;
float spo2;
float latitude = 0.0;
float longitude = 0.0;
int alertStatus = 0;

// --- Time Setup (IST = GMT+5:30) ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// --- Get Current IST Time ---
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Failed to get time";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// --- Simulate Sensors + Read Real Temp + Fall Detection ---
void simulateSensorValues() {
  // Simulated HR & SpO2 using potentiometers
  int potHR = analogRead(34);
  int potSpO2 = analogRead(35);

  heartRate = map(potHR, 0, 4095, 60, 120);
  spo2 = map(potSpO2, 0, 4095, 90, 100);

  // DS18B20 Temp (real)
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

  // MPU6050 Fall Detection
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float accX = ax / 16384.0;
  float accY = ay / 16384.0;
  float accZ = az / 16384.0;
  float accTotal = sqrt(accX * accX + accY * accY + accZ * accZ);

  fallDetected = (accTotal < 0.3 || accTotal > 2.5);
}

// --- Read GPS Data ---
void readGPS() {
  while (SerialGPS.available() > 0) {
    char c = SerialGPS.read();
    gps.encode(c);

    if (gps.location.isUpdated()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
    }
  }
}

// --- Check SOS Button ---
void checkSOSButton() {
  if (digitalRead(SOS_BUTTON) == LOW) {  // Button pressed (Active LOW)
    sosActivated = true;
    Serial.println("!!! EMERGENCY SOS ACTIVATED !!!");
  } else {
    sosActivated = false;
  }
}

// --- Alarm Logic ---
void checkAlarms() {
  if (heartRate > 100 || temperature > 38.0 || spo2 < 90 || fallDetected || sosActivated) {
    alertStatus = 1;
    Serial.println("!!! ALERT: Critical Condition or FALL/SOS Detected !!!");
  } else {
    alertStatus = 0;
    Serial.println("Vitals Normal.");
  }
}

// --- Send to ThingSpeak ---
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverName) + "/update?api_key=" + apiKey +
                 "&field1=" + String(heartRate) +
                 "&field2=" + String(temperature) +
                 "&field3=" + String(spo2) +
                 "&field4=" + String(latitude, 6) +
                 "&field5=" + String(longitude, 6) +
                 "&field6=" + String(alertStatus) +
                 "&field7=" + String(fallDetected) +
                 "&field8=" + String(sosActivated); // NEW: SOS STATUS FIELD

    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.printf("ThingSpeak response: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error sending: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(SOS_BUTTON, INPUT_PULLUP);  // Use internal pull-up for SOS button

  // OLED Init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  display.clearDisplay();
  display.display();

  // Wi-Fi Init
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Time Init
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronization started...");
  delay(2000);

  // DS18B20 Init
  sensors.begin();

  // MPU6050 Init
  Serial.println("Initializing MPU6050...");
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful!");
  } else {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }

  // GPS Init
  SerialGPS.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS Initialized!");
}

// --- Loop ---
void loop() {
  simulateSensorValues();
  readGPS();
  checkSOSButton();
  checkAlarms();

  String timestamp = getTimeStamp();

  // Serial Output
  Serial.printf("[%s] HR: %.1f bpm | Temp: %.2f °C | SpO2: %.1f %% | Lat: %.6f | Lon: %.6f | Fall=%d | SOS=%d | Alert=%d\n",
                timestamp.c_str(), heartRate, temperature, spo2, latitude, longitude, fallDetected, sosActivated, alertStatus);

  // OLED Output
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.printf("HR: %.1f bpm\n", heartRate);
  display.printf("Temp: %.2f C\n", temperature);
  display.printf("SpO2: %.1f %%\n", spo2);
  display.printf("Lat: %.4f\nLon: %.4f\n", latitude, longitude);
  display.printf("%s\n", timestamp.c_str());
  if (fallDetected) display.println("FALL DETECTED!");
  if (sosActivated) display.println("SOS: EMERGENCY!");
  if (alertStatus == 1 && !sosActivated) display.println("ALERT: CRITICAL!");
  else if (!fallDetected && !sosActivated) display.println("Status: Normal");
  display.display();

  sendToThingSpeak();

  delay(5000); // ThingSpeak update rate
}
