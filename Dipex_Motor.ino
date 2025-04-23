#include <ESP8266WiFi.h>
#include <espnow.h>
#include <SoftwareSerial.h>

#define RX_PIN D5
#define TX_PIN D6
SoftwareSerial mySerial(RX_PIN, TX_PIN);

// MAC addresses for the two Sector Heads
uint8_t sector1MAC[] = {0xEC, 0x64, 0xC9, 0xCE, 0x01, 0x3E};  // Head 1
uint8_t sector2MAC[] = {0xEC, 0x64, 0xC9, 0xCE, 0x08, 0x7F};  // Head 2

#define RELAY_PIN D4  // Relay controlling the motor

// Battery variables
float batteryVoltage;
float batteryPercentage;

// --- Override variables for each head ---
// For Sector 1 (rollNo == 3)
bool overrideActive3 = false;
unsigned long overrideStartTime3 = 0;
unsigned long overrideDuration3 = 0;
bool status3 = false;  // true if override command forces motor ON

// For Sector 2 (rollNo == 4)
bool overrideActive4 = false;
unsigned long overrideStartTime4 = 0;
unsigned long overrideDuration4 = 0;
bool status4 = false;  // true if override command forces motor ON

// Sensor timeout (7 seconds)
unsigned long lastSensorReceivedTime1 = 0;
unsigned long lastSensorReceivedTime2 = 0;
const unsigned long sensorTimeout = 7000;

// --- Structures used for communication ---

// Structure used for user override command
struct Userwants {
  int rollno;
  int status;
  int overWrite; // 1 for override command; 0 to cancel
  int time;      // Duration (in minutes) for which override is active
};

Userwants user;

// Structure received from Sector Heads (HeadData)
typedef struct {
  int sectorRollNo;
  int avgMoisture;
  bool sensor1Ack;
  bool sensor2Ack;
  bool valveStatus;
} HeadData;

HeadData receivedData;

// Structure to maintain overall motor status (to be sent to the server)
typedef struct {
  int avgMoistureSector1;
  int avgMoistureSector2;
  bool sector1Sensor1Ack;
  bool sector1Sensor2Ack;
  bool sector2Sensor1Ack;
  bool sector2Sensor2Ack;
  bool sector1ValveStatus;
  bool sector2ValveStatus;
  bool motorStatus;
  int batteryVoltage;
  int sector1working;
  int sector2working;
} MotorStatusData;

MotorStatusData motorData;

// Global variable to store the previously sent status string.
String prevStatus = "";

// --- ESP-NOW send callback ---
void sendCallback(uint8_t *mac, uint8_t sendStatus) {
  Serial.print("Send Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

// --- Read battery status ---
void readBatteryStatus() {
  int adc = analogRead(A0);
  batteryVoltage = (adc - 104) / 15.4;
  batteryPercentage = ((batteryVoltage - 9) / (12.6 - 10.5)) * 100;
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  Serial.print("Battery: ");
  Serial.print(batteryVoltage, 2);
  Serial.print("V, ");
  Serial.print(batteryPercentage);
  Serial.println("%");
  motorData.batteryVoltage = 3;
}

// --- Build status string ---
String buildStatusString() {
  // Format: avgMoistureSector1,sector1Sensor1Ack,sector1Sensor2Ack,sector1ValveStatus,
  //         avgMoistureSector2,sector2Sensor1Ack,sector2Sensor2Ack,sector2ValveStatus,
  //         motorStatus,batteryVoltage,sector1working,sector2workinga
  String s = String(motorData.avgMoistureSector1) + "," +
             String(motorData.sector1Sensor1Ack) + "," +
             String(motorData.sector1Sensor2Ack) + "," +
             String(motorData.sector1ValveStatus) + "," +
             String(motorData.avgMoistureSector2) + "," +
             String(motorData.sector2Sensor1Ack) + "," +
             String(motorData.sector2Sensor2Ack) + "," +
             String(motorData.sector2ValveStatus) + "," +
             String(motorData.motorStatus) + "," +
             String(motorData.batteryVoltage) + "," +
             String(motorData.sector1working) + "," +
             String(motorData.sector2working) + "a";
  return s;
}

// --- Send status string if it has changed ---
void sendStatusStringIfChanged() {
  String currentStatus = buildStatusString();
  if (currentStatus != prevStatus) {
    mySerial.println(currentStatus);
    prevStatus = currentStatus;
    Serial.print("Status updated: ");
    Serial.println(currentStatus);
  }
}

// --- ESP-NOW data receive callback ---
// This callback is invoked when a Sector Head sends HeadData.
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(HeadData)) {
    Serial.println("Invalid data length received");
    return;
  }
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  Serial.println("Data received from Sector Head:");
  Serial.print("Sector: ");
  Serial.println(receivedData.sectorRollNo);
  Serial.print("Avg Moisture: ");
  Serial.println(receivedData.avgMoisture);
  Serial.print("Sensor1 Ack: ");
  Serial.println(receivedData.sensor1Ack ? "Yes" : "No");
  Serial.print("Sensor2 Ack: ");
  Serial.println(receivedData.sensor2Ack ? "Yes" : "No");
  Serial.print("Valve Status: ");
  Serial.println(receivedData.valveStatus ? "Open" : "Closed");
  
  // Update motorData based on which head sent data.
  if (receivedData.sectorRollNo == 1) {
    motorData.avgMoistureSector1 = receivedData.avgMoisture;
    motorData.sector1Sensor1Ack = receivedData.sensor1Ack;
    motorData.sector1Sensor2Ack = receivedData.sensor2Ack;
    motorData.sector1ValveStatus = receivedData.valveStatus;
    motorData.sector1working = 1;
    lastSensorReceivedTime1 = millis();
  } else if (receivedData.sectorRollNo == 2) {
    motorData.avgMoistureSector2 = receivedData.avgMoisture;
    motorData.sector2Sensor1Ack = receivedData.sensor1Ack;
    motorData.sector2Sensor2Ack = receivedData.sensor2Ack;
    motorData.sector2ValveStatus = receivedData.valveStatus;
    motorData.sector2working = 1;
    lastSensorReceivedTime2 = millis();
  }
  
  // Determine motor state:
  // Priority: If any override is active and its command is ON, then force motor ON.
  // Otherwise, follow sensor (head) data – if any head’s valve is open, motor ON.
  bool motorState = false;
  if ((overrideActive3 && status3) || (overrideActive4 && status4)) {
    motorState = true;
  } else {
    motorState = (motorData.sector1ValveStatus || motorData.sector2ValveStatus);
  }
  digitalWrite(RELAY_PIN, motorState ? HIGH : LOW);
  motorData.motorStatus = motorState;
  
  readBatteryStatus();
  sendStatusStringIfChanged();
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(A0, INPUT);  // Motor relay off initially

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW initialization failed on Motor Node");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(sendCallback);
  esp_now_add_peer(sector1MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_add_peer(sector2MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  
  Serial.println("Motor Node setup complete.");
  lastSensorReceivedTime1 = millis();
  lastSensorReceivedTime2 = millis();
}

void loop() {
  // --- Check for expired override commands ---
  if (overrideActive3 && (millis() - overrideStartTime3 >= overrideDuration3)) {
    overrideActive3 = false;
    status3 = false;
    Serial.println("Override for Sector 1 expired.");
  }
  if (overrideActive4 && (millis() - overrideStartTime4 >= overrideDuration4)) {
    overrideActive4 = false;
    status4 = false;
    Serial.println("Override for Sector 2 expired.");
  }
  
  // --- Check sensor data timeouts ---
  if (millis() - lastSensorReceivedTime1 >= sensorTimeout) {
    motorData.sector1working = 0;
    motorData.sector1ValveStatus = false;
    Serial.println("Timeout: No data from Sector 1");
  }
  if (millis() - lastSensorReceivedTime2 >= sensorTimeout) {
    motorData.sector2working = 0;
    motorData.sector2ValveStatus = false;
    Serial.println("Timeout: No data from Sector 2");
  }
  
  // --- Update overall motor state ---
  bool finalState = false;
  if ((overrideActive3 && status3) || (overrideActive4 && status4)) {
    finalState = true;
  } else {
    finalState = (motorData.sector1ValveStatus || motorData.sector2ValveStatus);
  }
  digitalWrite(RELAY_PIN, finalState ? HIGH : LOW);
  motorData.motorStatus = finalState;
  
  sendStatusStringIfChanged();
  
  // --- Handle user override commands from SoftwareSerial ---
  if (mySerial.available()) {
    String receivedStr = mySerial.readStringUntil('\n');
    Serial.print("Command received: ");
    Serial.println(receivedStr);
    
    // Example command formats:
    // "13,1" to override Sector 1, "14,1" to override Sector 2,
    // or full command: "overWrite,rollno,status,time" (e.g., "1,3,1,5")
    if (receivedStr.startsWith("13,")) {
      int commaIndex = receivedStr.indexOf(',');
      user.rollno = 13;
      user.status = receivedStr.substring(commaIndex + 1).toInt();
      Serial.print("Forwarding override to Sector 1: ");
      Serial.println(user.status);
      esp_now_send(sector1MAC, (uint8_t *)&user, sizeof(user));
    }
    else if (receivedStr.startsWith("14,")) {
      int commaIndex = receivedStr.indexOf(',');
      user.rollno = 14;
      user.status = receivedStr.substring(commaIndex + 1).toInt();
      Serial.print("Forwarding override to Sector 2: ");
      Serial.println(user.status);
      esp_now_send(sector2MAC, (uint8_t *)&user, sizeof(user));
    }
    else {
      // Parse full override command in the format: "overWrite,rollno,status,time"
      int firstComma = receivedStr.indexOf(',');
      int secondComma = receivedStr.indexOf(',', firstComma + 1);
      int thirdComma = receivedStr.indexOf(',', secondComma + 1);
      user.overWrite = receivedStr.substring(0, firstComma).toInt();
      user.rollno = receivedStr.substring(firstComma + 1, secondComma).toInt();
      user.status = receivedStr.substring(secondComma + 1, thirdComma).toInt();
      user.time = receivedStr.substring(thirdComma + 1).toInt();
      
      // Process the override command:
      if (user.overWrite == 1) {
        if (user.rollno == 3) {
          overrideActive3 = true;
          overrideStartTime3 = millis();
          overrideDuration3 = (unsigned long)user.time * 60000; // convert minutes to ms
          status3 = (user.status == 1);
          esp_now_send(sector1MAC, (uint8_t *)&user, sizeof(user));
          Serial.print("Override command sent to Sector 1 for ");
          Serial.print(user.time);
          Serial.println(" minutes.");
        }
        else if (user.rollno == 4) {
          overrideActive4 = true;
          overrideStartTime4 = millis();
          overrideDuration4 = (unsigned long)user.time * 60000;
          status4 = (user.status == 1);
          esp_now_send(sector2MAC, (uint8_t *)&user, sizeof(user));
          Serial.print("Override command sent to Sector 2 for ");
          Serial.print(user.time);
          Serial.println(" minutes.");
        }
      }
      else if (user.overWrite == 0) {
        if (user.rollno == 3) {
          overrideActive3 = false;
          status3 = false;
          esp_now_send(sector1MAC, (uint8_t *)&user, sizeof(user));
          Serial.println("Override cancelled for Sector 1.");
        }
        else if (user.rollno == 4) {
          overrideActive4 = false;
          status4 = false;
          esp_now_send(sector2MAC, (uint8_t *)&user, sizeof(user));
          Serial.println("Override cancelled for Sector 2.");
        }
      }
    }
  }
  
  delay(2000);
}