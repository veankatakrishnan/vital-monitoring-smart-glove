/*************************************************
  Integrated Health Monitoring System
*************************************************/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <MPU6050.h>
#include <math.h>

// ---------------- OLED (SPI) ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     17
#define OLED_CS     5
#define OLED_RESET  16

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ---------------- CONSTANT GPS ----------------
const float LATITUDE  = 10.902435;
const float LONGITUDE = 76.896310;

// ---------------- MAX30102 ----------------
MAX30105 particleSensor;

uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t bufferLength = 100;

int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

int displayHR = 0;
int displaySpO2 = 0;
bool fingerDetected = false;

// ---------------- MPU6050 ----------------
MPU6050 mpu;
#define IMPACT_THRESHOLD 2.5

bool fallDetectedFlag = false;
bool potentialFallDetected = false;
unsigned long impactTime = 0;
unsigned long fallEventTime = 0;

// ---------------- DS18B20 ----------------
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float currentTempC = 0.0;

// ---------------- Timing ----------------
unsigned long lastDeciSecond = 0;
unsigned long lastSecond = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // OLED Init
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED failed");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Initializing...");
  display.display();

  // MPU6050
  mpu.initialize();

  // Temperature
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  // MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found");
  }
  particleSensor.setup(60, 4, 2, 100, 411, 4096);

  display.clearDisplay();
  display.println("Sensors Ready");
  display.display();
  delay(1000);
}

void loop() {

  // -------- MAX30102 Continuous --------
  static int sampleIndex = 0;
  particleSensor.check();

  while (particleSensor.available()) {

    long irValue = particleSensor.getIR();

    if (irValue < 50000) {
      fingerDetected = false;
      displayHR = 0;
      displaySpO2 = 0;
    } else {
      fingerDetected = true;
    }

    redBuffer[sampleIndex] = particleSensor.getRed();
    irBuffer[sampleIndex] = irValue;

    particleSensor.nextSample();
    sampleIndex++;

    if (sampleIndex >= bufferLength) {
      sampleIndex = 0;

      maxim_heart_rate_and_oxygen_saturation(
        irBuffer, bufferLength, redBuffer,
        &spo2, &validSPO2,
        &heartRate, &validHeartRate
      );

      if (validHeartRate && heartRate > 40 && heartRate < 120)
        displayHR = heartRate;

      if (validSPO2 && spo2 > 70 && spo2 <= 100)
        displaySpO2 = spo2;
    }
  }

  // -------- Fall Detection (0.1 sec) --------
  if (millis() - lastDeciSecond >= 100) {
    lastDeciSecond = millis();

    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    float accX = ax / 16384.0;
    float accY = ay / 16384.0;
    float accZ = az / 16384.0;

    float accMagnitude = sqrt(accX*accX + accY*accY + accZ*accZ);

    if (!potentialFallDetected && accMagnitude > IMPACT_THRESHOLD) {
      potentialFallDetected = true;
      impactTime = millis();
    }

    if (potentialFallDetected && millis() - impactTime >= 1000) {
      fallDetectedFlag = true;
      fallEventTime = millis();
      potentialFallDetected = false;
    }

    if (fallDetectedFlag && millis() - fallEventTime > 2000) {
      fallDetectedFlag = false;
    }
  }

  // -------- OLED Update (1 sec) --------
  if (millis() - lastSecond >= 1000) {
    lastSecond = millis();

    currentTempC = sensors.getTempCByIndex(0);
    sensors.requestTemperatures();

    display.clearDisplay();
    display.setCursor(0,0);

    // Line 1: Temperature
    display.print("T:");
    display.print(currentTempC,1);
    display.println("C");

    // Line 2 & 3: HR/SpO2 or Finger
    if (!fingerDetected) {
      display.println("Finger Not Placed");
      display.println("Place finger...");
    } else {
      display.print("HR:");
      display.print(displayHR);
      display.print("  SpO2:");
      display.print(displaySpO2);
      display.println("%");
    }

    // Line 4 & 5: GPS
    display.print("Lat:");
    display.println(LATITUDE,4);

    display.print("Lon:");
    display.println(LONGITUDE,4);

    // Bottom Line (always visible)
    display.setCursor(0,56);
    if (fallDetectedFlag) {
      display.print("Status: FALL !");
    } else {
      display.print("Status: OK");
    }

    display.display();
  }
}