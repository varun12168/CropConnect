🌱 CropConnect – Smart Irrigation System

📌 Overview

CropConnect is an IoT-based smart irrigation system designed to optimize water usage in agriculture by monitoring soil moisture levels in real-time and automating irrigation decisions. The system uses distributed ESP8266 modules, sensors, and intelligent control logic to ensure efficient and sustainable farming.

---

🚀 Features

🌾 Real-time soil moisture monitoring

💧 Automated irrigation control based on thresholds

📡 Wireless communication using ESP-NOW protocol

🔋 Power-efficient deep sleep mode

📊 Data visualization via mobile/web app

🌦️ Weather integration (future scope)

🧠 AI-based crop and fertilizer recommendation (planned)

📶 GSM support for non-internet users (future scope)

---

🏗️ System Architecture

The system is divided into multiple sections:

🔹 Sensor Nodes (ESP8266)

Connected to soil moisture sensors

Sends moisture data periodically to Head ESP


🔹 Head ESP (Section Controller)

Receives data from multiple sensors

Calculates average moisture level

Compares with threshold (e.g., 75%)

Controls solenoid valve

Sends request to Motor ESP if irrigation is needed


🔹 Motor ESP (Central Controller)

Receives requests from all Head ESPs

Starts motor if any section requires water

Sends override commands to control valves

Uploads data to Firebase

---

⚙️ Working Principle

1. Sensors measure soil moisture in different sections


2. Data is sent to respective Head ESP


3. Head ESP:

Calculates average moisture

Compares with threshold

Opens valve if moisture is low


4. Head ESP sends request to Motor ESP


5. Motor ESP:

Starts motor if any request is active

Stops motor when all sections are satisfied



6. System enters deep sleep for power saving


---

🧠 Logic Flow

IF moisture < threshold:
    Open valve
    Send request to motor
ELSE:
    Close valve
    Enter deep sleep

Motor Logic:

IF any head requests water:
    Turn ON motor
ELSE:
    Turn OFF motor
    Enter deep sleep


---

🛠️ Tech Stack

Hardware:

ESP8266 (NodeMCU)

Soil Moisture Sensors

Solenoid Valves

Relay Module

Water Pump


Software:

Arduino IDE

ESP-NOW Protocol

Firebase Realtime Database


UI/UX:

Figma (App Design)




---

📱 Application Features (Planned)

Farmer details (Name, Location, Crop)

Soil moisture visualization (graphs + sections)

Weather forecast integration

AI-based recommendations

Remote irrigation control



---

🔌 Communication Protocol

ESP-NOW for device-to-device communication

Wi-Fi for cloud connectivity (Motor ESP only)



---

🔋 Power Optimization

Deep sleep mode after irrigation cycle

Periodic wake-up for data transmission

Reduces power consumption significantly



---

📊 Data Flow

Sensor ESP → Head ESP → Motor ESP → Firebase → App


---

🔮 Future Enhancements

🌿 NPK Sensor integration

🤖 AI-based irrigation optimization

📡 GSM module for offline farmers

☁️ Advanced analytics dashboard

📍 Geo-based crop suggestions



---

📁 Project Structure

CropConnect/
│── Sensor_Node/
│── Head_Controller/
│── Motor_Controller/
│── App_UI/
│── Documentation/
│── README.md


---

🧪 Use Cases

Smart farming for small to large fields

Water conservation in agriculture

Precision irrigation systems

College capstone/IoT projects



---

🏆 Advantages

Saves water 💧

Reduces manual effort 👨‍🌾

Improves crop yield 🌾

Scalable architecture 📈

Cost-effective solution 💰



---

📜 License

This project is for educational and research purposes. You may modify and use it with proper credits.


---

👨‍💻 Author

Gauravi Naik

Varun Mudaliyar

Rahul Yadav

Krishna Bitthariya
