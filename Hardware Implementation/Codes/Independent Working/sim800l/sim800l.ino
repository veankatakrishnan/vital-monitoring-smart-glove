#define RXD2 1

#define TXD2 3

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(20000);  // Wait 20 seconds

  Serial2.println("AT+CREG?");
}

void loop() {
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}