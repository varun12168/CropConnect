//Server
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SoftwareSerial.h>

#define WIFI_SSID "2601"
#define WIFI_PASSWORD "02072005"

#define RX_PIN D5  // Receiver's RX (Connected to sender's TX)
#define TX_PIN D6  // Receiver's TX (Connected to sender's RX)

SoftwareSerial mySerial(RX_PIN, TX_PIN);
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;  // Create FirebaseConfig object

String user;

struct moisture {
  int rollNo;
  int desired_value;
};

moisture pre1 = {13, 0};
moisture new1 = {13, 0};
moisture pre2 = {14, 0};
moisture new2 = {14, 0};

struct Userwants {
  int rollno = 0;
  int status = 0;
  int overWrite = 0;
  int time = 0;
};

Userwants pre;
Userwants newData;

struct MotorStatusData {
  int sector1working = 0;
  int sector2working = 0;
  int avgMoistureSector1 = 0;
  int avgMoistureSector2 = 0;
  bool sector1ValveStatus = false;
  bool sector2ValveStatus = false;
  bool sector1Sensor1Ack = false;
  bool sector1Sensor2Ack = false;
  bool sector2Sensor1Ack = false;
  bool sector2Sensor2Ack = false;
  bool motorStatus = false;
  int batteryVoltage = 0;
};

MotorStatusData motorData;

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  // Set Firebase configuration
  config.host = "smart-irrigation-b4095-default-rtdb.firebaseio.com";
  config.signer.tokens.legacy_token = "kCoHAXvU4KcQIZetkCSPiwXgnCg7bRVoT4Go8eUX";
  Firebase.begin(&config, &auth);
}

void loop() {
  if (mySerial.available()) {
    // Read the complete string sent by the sender
    String receivedData = mySerial.readStringUntil('\n');
    Serial.print("Data from sender: ");
    Serial.println(receivedData);

    // Locate comma positions to extract values
    int firstComma    = receivedData.indexOf(',');
    int secondComma   = receivedData.indexOf(',', firstComma + 1);
    int thirdComma    = receivedData.indexOf(',', secondComma + 1);
    int fourthComma   = receivedData.indexOf(',', thirdComma + 1);
    int fifthComma    = receivedData.indexOf(',', fourthComma + 1);
    int sixthComma    = receivedData.indexOf(',', fifthComma + 1);
    int seventhComma  = receivedData.indexOf(',', sixthComma + 1);
    int eighthComma   = receivedData.indexOf(',', seventhComma + 1);
    int ninthComma    = receivedData.indexOf(',', eighthComma + 1);
    int tenthComma    = receivedData.indexOf(',', ninthComma + 1);
    int eleventhComma = receivedData.indexOf(',', tenthComma + 1);
    int twelfthComma  = receivedData.indexOf(',', eleventhComma + 1);

    // Extracting and converting values from the received string
    motorData.avgMoistureSector1 = receivedData.substring(0, firstComma).toInt();
    motorData.sector1Sensor1Ack  = receivedData.substring(firstComma + 1, secondComma).toInt();
    motorData.sector1Sensor2Ack  = receivedData.substring(secondComma + 1, thirdComma).toInt();
    motorData.sector1ValveStatus = receivedData.substring(thirdComma + 1, fourthComma).toInt();
    motorData.avgMoistureSector2 = receivedData.substring(fourthComma + 1, fifthComma).toInt();
    motorData.sector2Sensor1Ack  = receivedData.substring(fifthComma + 1, sixthComma).toInt();
    motorData.sector2Sensor2Ack  = receivedData.substring(sixthComma + 1, seventhComma).toInt();
    motorData.sector2ValveStatus = receivedData.substring(seventhComma + 1, eighthComma).toInt();
    motorData.motorStatus        = receivedData.substring(eighthComma + 1, ninthComma).toInt();
    motorData.batteryVoltage     = receivedData.substring(ninthComma + 1, tenthComma).toInt();
    motorData.sector1working     = receivedData.substring(tenthComma + 1, eleventhComma).toInt();
    motorData.sector2working     = receivedData.substring(eleventhComma + 1, twelfthComma).toInt();

    // Create a FirebaseJson object to combine all values
    FirebaseJson json;
    json.add("sectors/sector1/avgMoisture", motorData.avgMoistureSector1);
    json.add("sectors/sector1/sensor1Ack", motorData.sector1Sensor1Ack);
    json.add("sectors/sector1/sensor2Ack", motorData.sector1Sensor2Ack);
    json.add("sectors/sector1/valveStatus", motorData.sector1ValveStatus);
    json.add("sectors/sector2/avgMoisture", motorData.avgMoistureSector2);
    json.add("sectors/sector2/sensor1Ack", motorData.sector2Sensor1Ack);
    json.add("sectors/sector2/sensor2Ack", motorData.sector2Sensor2Ack);
    json.add("sectors/sector2/valveStatus", motorData.sector2ValveStatus);
    json.add("motor/status", motorData.motorStatus);
    json.add("motor/Battery", motorData.batteryVoltage);
    json.add("sectors/sector1/working", motorData.sector1working);
    json.add("sectors/sector2/working", motorData.sector2working);

    // Send the JSON update in one call
    if (Firebase.updateNode(firebaseData, "/", json)) {
      Serial.println("Data sent to Firebase immediately");
    } else {
      Serial.println("Firebase send failed: " + firebaseData.errorReason());
    }
  }

  if (Firebase.getJSON(firebaseData, "/userWants")) {
    FirebaseJson &json = firebaseData.jsonObject();
    FirebaseJsonData jsonData;

    json.get(jsonData, "rollNo");
    newData.rollno = jsonData.intValue;

    json.get(jsonData, "status");
    newData.status = jsonData.intValue;

    json.get(jsonData, "over_write");
    newData.overWrite = jsonData.intValue;

    json.get(jsonData, "time");
    newData.time = jsonData.intValue;

    Serial.println("Userwants data retrieved:");
    Serial.print("RollNo: "); Serial.println(newData.rollno);
    Serial.print("Status: "); Serial.println(newData.status);
    Serial.print("OverWrite: "); Serial.println(newData.overWrite);
    Serial.print("Time: "); Serial.println(newData.time);
  } else {
    Serial.println("Firebase getJSON failed: " + firebaseData.errorReason());
  }

  if (Firebase.getJSON(firebaseData, "/UserData/Sector1")) {
    FirebaseJson &json = firebaseData.jsonObject();
    FirebaseJsonData jsonData;

    json.get(jsonData, "DesiredMoisture");
    new1.desired_value = jsonData.intValue;
    Serial.println(new1.desired_value);
  } else {
    Serial.println("Firebase getJSON failed: " + firebaseData.errorReason());
  }
  
  if (Firebase.getJSON(firebaseData, "/UserData/Sector2")) {
    FirebaseJson &json = firebaseData.jsonObject();
    FirebaseJsonData jsonData;

    json.get(jsonData, "DesiredMoisture");
    new2.desired_value = jsonData.intValue;
    Serial.println(new2.desired_value);
  } else {
    Serial.println("Firebase getJSON failed: " + firebaseData.errorReason());
  }

  if ((new1.desired_value != pre1.desired_value)) {
    pre1.desired_value = new1.desired_value;
    user = String(new1.rollNo) + "," + String(new1.desired_value);
    Serial.print("Update:");
    Serial.println(new1.desired_value);
    mySerial.print(user);
  }
  
  delay(500);
  
  if ((new2.desired_value != pre2.desired_value)) {
    pre2.desired_value = new2.desired_value;
    user = String(new2.rollNo) + "," + String(new2.desired_value);
    mySerial.print(user);
    Serial.print("Update:");
    Serial.println(new2.desired_value);
  }

  delay(500);

  if ((pre.overWrite != newData.overWrite) || (pre.rollno != newData.rollno) || 
      (pre.status != newData.status) || (pre.time != newData.time)) {
    pre.overWrite = newData.overWrite;
    pre.rollno = newData.rollno;
    pre.status = newData.status;
    pre.time = newData.time;
    Serial.print("I will send data");
    user = String(newData.overWrite) + "," + String(newData.rollno) + "," + 
           String(newData.status) + "," + String(newData.time);
    mySerial.print(user);
  }

  delay(1000);
}