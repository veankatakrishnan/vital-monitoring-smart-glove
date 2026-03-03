#include <TinyGPSPlus.h>

TinyGPSPlus gps;

void setup() {
  Serial.begin(115200);
  Serial.println("GPS Module Parsing Started...");

  // GPS UART on Serial2
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
  // Feed all available GPS characters into TinyGPSPlus
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // Display decoded information
  if (gps.location.isUpdated()) {
    Serial.println("\n--- GPS FIX UPDATED ---");

    Serial.print("Latitude : ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);

    Serial.print("Satellites: ");
    Serial.println(gps.satellites.value());

    Serial.print("Altitude (m): ");
    Serial.println(gps.altitude.meters());

    Serial.print("Speed (km/h): ");
    Serial.println(gps.speed.kmph());

    Serial.print("Date: ");
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.println(gps.date.year());

    Serial.print("Time (UTC): ");
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());

    Serial.println("------------------------");
  }
}