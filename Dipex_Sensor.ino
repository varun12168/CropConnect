/* Sensor Node Code for Solar Powered Smart Irrigation System
   Reads moisture sensor value and sends it via ESP-NOW.
   Also listens for an ACK from its sector head.
*/
// Head 1 - 0xEC, 0x64, 0xC9, 0xCE, 0x01, 0x3E
// Head 2  - 0xEC, 0x64, 0xC9, 0xCE, 0x08, 0x7F
#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_PIN D2
uint8_t sectorHeadMAC[] = {0xEC, 0x64, 0xC9, 0xCE, 0x08, 0x7F};
// Data structure for sensor data
typedef struct {
  int rollNo;         // Unique sensor ID (set individually per sensor)
  int moistureValue;
     // Moisture reading (percentage)
} SensorData;

SensorData sensorData;


void sendCallback(uint8_t *mac, uint8_t sendStatus) {
  Serial.print("Send Status: ");
  // Serial.println(sendStatus == 0 ? "Success" : "Fail");
if(!sendStatus){
  Serial.println("successs ");
}
else{
   Serial.println("fail ");
}
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

esp_now_register_send_cb(sendCallback);
  // Set sensor unique roll number (adjust per sensor device)
  sensorData.rollNo = 2; // For example, sensor 1 uses rollNo=1; sensor 2 would use 2.
  
  Serial.println("Sensor node setup complete.");
}

void loop() {
  // Read the moisture sensor (simulate with analogRead)
  int rawValue = analogRead(A0);
  Serial.print("Raw value: ");
  Serial.println(rawValue);
  // Map raw value to a percentage (calibrate as needed)
  sensorData.moistureValue = constrain(map(rawValue, 460, 1023, 100, 0),0,100);

  // Send sensor data to the Sector (Head) node
  // Update the MAC address below to match the sector head device

  esp_now_send(sectorHeadMAC, (uint8_t *)&sensorData, sizeof(sensorData));

  // Blink LED to indicate transmission
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  Serial.print("Sensor ");
  Serial.print(sensorData.rollNo);
  Serial.print(" sent moisture: ");
  Serial.println(sensorData.moistureValue);

  // Wait up to 1 second for an ACK
  delay(1000); // Delay before next reading
}