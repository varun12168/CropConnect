//Head
#include <ESP8266WiFi.h>
#include <espnow.h>

#define RELAY_PIN1 D1  // Relay controlling solenoid valve
#define RELAY_PIN2 D2

int prevAverageMoisture = -1;  // Set to -1 initially so that the first sensor reading always triggers an update.
int averageMoisture = 0;
 int desiredMoistureLevel = 65; // Normal operation moisture threshold
bool valveStatus = false;

struct mositure{
  int rollNo;
  int desired_value;
};


uint8_t motorNodeMAC[] = {0x48, 0x3F, 0xDA, 0x65, 0x45, 0x7D};//MAC: 48:3f:da:65:45:7d
//MAC: ec:64:c9:ce:08:a0
// Structure for sensor data (from sensor nodes)
typedef struct {
  int rollNo;
  int moistureValue;
} SensorData;

// Structure for user override command (rollNo 3)
// Here, moistureValue acts as the override valve command:
//   1 = force valve OPEN for the given duration
//   0 = force valve CLOSED for the given duration
// user_over_Write indicates that an override command is issued,
// and time indicates the override duration in minutes.
typedef struct {
  int rollNo;
  int moistureValue;
  int user_over_Write; // 1 if override command is sent
  int time;            // Duration in minutes for which the override should be active
} Temp;

Temp usercommand;

// Structure to send from Sector Head to Motor node
typedef struct {
  int sectorRollNo;
  int avgMoisture;
  bool sensor1Ack;
  bool sensor2Ack;
  bool valveStatus;
} HeadData;
 
 HeadData headData;


SensorData sensor1Data, sensor2Data;
bool sensor1Received = false, sensor2Received = false;
bool sensor1Ack = false, sensor2Ack = false;

// Global flags and timing for override control
bool overrideActive = false;     // True when an override is in effect
unsigned long overrideStartTime = 0;
unsigned long overrideDuration = 0;
bool operationEnabled = true;    // true: normal sensor-based operation; false: override mode active

// Global variable to track waiting for a second sensor data (3 sec wait)
unsigned long sensorWaitStartTime = 0;
// Global variable to track when any sensor data was last received (for 5 sec timeout)
unsigned long lastSensorReceivedTime = 0;

// Global flag to indicate that a timeout event occurred.
bool timedOut = false;

//
// Function to update the valve based on sensor data and send summary to the Motor node.
// This function is only used when no override is active.
//
void updateValveAndSend() {
  if (!operationEnabled) {
    // If override is active, sensor data is ignored.
    return;
  }

  // Wait for 3 seconds if only one sensor value is received.
  if (sensor1Received && sensor2Received) {
    // Both sensor readings are available. Process immediately.
    averageMoisture = (sensor1Data.moistureValue + sensor2Data.moistureValue) / 2;
    sensorWaitStartTime = 0; // reset waiting timer
  } else if (sensor1Received || sensor2Received) {
    // Only one sensor has been received.
    if (sensorWaitStartTime == 0) {
      sensorWaitStartTime = millis();
      return; // Wait for possible second sensor data.
    }
    if (millis() - sensorWaitStartTime >= 3000) {
      // Use whichever sensor data was received.
      if (sensor1Received) {
        averageMoisture = sensor1Data.moistureValue;
      } else {
        averageMoisture = sensor2Data.moistureValue;
      }
      sensorWaitStartTime = 0; // reset waiting timer
    } else {
      return; // still waiting for 3 sec to elapse.
    }
  } else {
    // No sensor data has been received yet.
    return;
  }

  // Force update if coming out of a timeout:
  // If timedOut was set, we reset prevAverageMoisture to force processing.
  if (timedOut) {
    prevAverageMoisture = -1;
    timedOut = false;
  }

  // Update valve state only if there is a change in average moisture.
  if (true) {
    prevAverageMoisture = averageMoisture;
    if (averageMoisture < desiredMoistureLevel) {
      // Open the valve (assuming LOW energizes the relay)
      digitalWrite(RELAY_PIN1, LOW);
      digitalWrite(RELAY_PIN2, LOW);
      valveStatus = true;
      Serial.println("Valve opened (moisture below threshold)");
    } else {
      // Close the valve
      digitalWrite(RELAY_PIN1, HIGH);
      digitalWrite(RELAY_PIN2, HIGH);
      valveStatus = false;
      Serial.println("Valve closed (moisture above threshold)");
    }

    // Prepare and send data to the Motor node.
    HeadData headData;
    headData.sectorRollNo = 1;
    headData.avgMoisture = averageMoisture;
    headData.sensor1Ack = sensor1Ack;
    headData.sensor2Ack = sensor2Ack;
    headData.valveStatus = valveStatus;

    esp_now_send(motorNodeMAC, (uint8_t *)&headData, sizeof(headData));
    Serial.print("Sector Head sent to Motor Node: Avg Moisture = ");
    Serial.println(averageMoisture);

    // Reset sensor flags for the next cycle.
    sensor1Received = sensor2Received = false;
    sensor1Ack = sensor2Ack = false;
  }
}

// Callback executed when data is received via ESP-NOW.
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  Temp received;

  memcpy(&received, incomingData, sizeof(received));

  // Process sensor data from Sensor 1 and Sensor 2.
  if (received.rollNo == 1) {
    sensor1Data.rollNo = received.rollNo;
    sensor1Data.moistureValue = received.moistureValue;
    sensor1Received = true;
    sensor1Ack = true;
    Serial.print("Received from Sensor 1: ");
    Serial.println(received.moistureValue);
  }
  else if (received.rollNo == 2) {
    sensor2Data.rollNo = received.rollNo;
    sensor2Data.moistureValue = received.moistureValue;
    sensor2Received = true;
    sensor2Ack = true;
    Serial.print("Received from Sensor 2: ");
    Serial.println(received.moistureValue);
  }
  else if(received.rollNo==13){
    desiredMoistureLevel=received.moistureValue;
    Serial.print("moisture update");
    Serial.println(desiredMoistureLevel);
  }
  else if(received.rollNo==14){
    desiredMoistureLevel=received.moistureValue;
    Serial.print("moisture update");
    Serial.println(desiredMoistureLevel);
  }
  // Process the user override command (rollNo 3).
  else if (received.rollNo == 3) {
    usercommand = received;
    Serial.print("Hello I am Here");
    Serial.println(usercommand.time);

    if (usercommand.user_over_Write == 1) {
      // Activate override mode regardless of sensor data.
      overrideActive = true;
      operationEnabled = false;  // Stop sensor-based operations.
      overrideStartTime = millis();
      overrideDuration = (unsigned long)usercommand.time * 60000;

      if (usercommand.moistureValue == 1) {
        digitalWrite(RELAY_PIN1, LOW);
        digitalWrite(RELAY_PIN2, LOW);
        valveStatus = true;
        Serial.println("User override: Valve forced OPEN for specified duration.");
      } else {
        digitalWrite(RELAY_PIN1, HIGH);
        digitalWrite(RELAY_PIN2, HIGH);
        valveStatus = false;
        Serial.println("User override: Valve forced CLOSED for specified duration.");
      }
    }else if(usercommand.user_over_Write == 0){
      overrideActive = false;
      operationEnabled = true;
    }
  }
  else if (received.rollNo == 4) {
    usercommand = received;
    Serial.print("Hello I am Here");
    Serial.println(usercommand.time);

    if (usercommand.user_over_Write == 1) {
      // Activate override mode regardless of sensor data.
      overrideActive = true;
      operationEnabled = false;  // Stop sensor-based operations.
      overrideStartTime = millis();
      overrideDuration = (unsigned long)usercommand.time * 60000;

      if (usercommand.moistureValue == 1) {
        digitalWrite(RELAY_PIN1, LOW);
        digitalWrite(RELAY_PIN2, LOW);
        valveStatus = true;
        Serial.println("User override: Valve forced OPEN for specified duration.");
      } else {
        digitalWrite(RELAY_PIN1, HIGH);
        digitalWrite(RELAY_PIN2, HIGH);
        valveStatus = false;
        Serial.println("User override: Valve forced CLOSED for specified duration.");
      }
    }else if(usercommand.user_over_Write == 0){
      overrideActive = false;
      operationEnabled = true;
    }
  }

  

  // Every time data is received, update the timestamp.
  lastSensorReceivedTime = millis();

  // If we were in a timeout state, force an update by resetting the sensor wait flags and prevAverageMoisture.
  if (timedOut) {
    prevAverageMoisture = -1;
    sensor1Received = sensor2Received = false;
    sensorWaitStartTime = 0;
    timedOut = false;
    Serial.println("Sensor data received after timeout, resuming normal operation.");
  }

  updateValveAndSend();
  Serial.print("time: ");
  Serial.println(lastSensorReceivedTime);
}

// Callback for sending status.
void sendCallback(uint8_t *mac, uint8_t sendStatus) {
  Serial.print("Send Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  digitalWrite(RELAY_PIN1, HIGH);  // Ensure relay is off initially
  digitalWrite(RELAY_PIN2, HIGH);

  WiFi.mode(WIFI_AP_STA);
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW initialization failed on Sector Head");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(sendCallback);

  // Add peer for the Motor node.
  esp_now_add_peer(motorNodeMAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  Serial.println("Sector Head setup complete.");

  // Initialize last sensor received time.
  lastSensorReceivedTime = millis();
}

void loop() {

  // esp_now_send(motorNodeMAC, (uint8_t *)&headData, sizeof(headData));
  // Check if override duration has expired.
  if (overrideActive) {
    if (millis() - overrideStartTime >= overrideDuration) {
      overrideActive = false;
      operationEnabled = true;  // Resume normal sensor-based operations.
      Serial.println("Override duration expired. Resuming normal operations.");
    }
  }
  
  // Check for sensor data timeout (5 seconds).
  if (!overrideActive && (millis() - lastSensorReceivedTime >= 5000)) {
      headData.sensor1Ack = false;
      headData.sensor2Ack = false;

 esp_now_send(motorNodeMAC, (uint8_t *)&headData, sizeof(headData));


    if (valveStatus == true) { // Only act if the valve is currently open.
      digitalWrite(RELAY_PIN1, HIGH);
      digitalWrite(RELAY_PIN2, HIGH);
      valveStatus = false;
      Serial.println("No sensor data received for 5 sec. Defaulting valve to closed.");

      // Optionally, send an update to the Motor node.
      
      headData.sectorRollNo = 1;
      headData.avgMoisture = averageMoisture;
      headData.sensor1Ack = sensor1Ack;
      headData.sensor2Ack = sensor2Ack;
      headData.valveStatus = valveStatus;
      esp_now_send(motorNodeMAC, (uint8_t *)&headData, sizeof(headData));
    }
    // Mark that a timeout event has occurred.
    timedOut = true;
  }
  
  delay(1000);
}