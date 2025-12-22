## Introduction
The **ESP32-C3 Fan Controller MQTT** project is an IoT application using **ESP32-C3** microcontroller to remotely control a fan via **MQTT protocol**.  
The system allows ESP32 to connect to WiFi and communicate with **HiveMQ Cloud MQTT Broker** to receive control commands and send status data.

The fan is controlled via a **Flutter application**, enabling users to send commands and receive data from ESP32, which is then displayed on the app interface.

The project is built on **ESP-IDF** using **ESP-MQTT library** and **MQTT version 5**.

---

## Main Features
- WiFi and Internet connection
- Connect to **HiveMQ Cloud MQTT Broker (MQTT v5)**
- Remotely control a 220V oscillating fan
- Receive control commands from Flutter App
- Send status and temperature data to MQTT Broker
- Display fan status and data on Flutter App
- Trigger buzzer and LED for notifications
- Publish / Subscribe MQTT topics

---

## Supported Targets
| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

---

## Hardware Components
- **ESP32-C3** – Main microcontroller
- **4-channel Relay** – Control the 220V oscillating fan and other functions
- **DHT22** – Temperature sensor
- **Buzzer** – Notification alerts
- **LED** – Status indicator
- **220V AC Fan** – Oscillating fan

> ⚠️ **Safety Note:**  
> The fan operates on **220V AC**. Proper isolation and correct wiring to the relay is required to ensure safety.

---

## Technology Stack
- ESP-IDF
- ESP-MQTT Library
- MQTT v5
- WiFi
- HiveMQ Cloud MQTT Broker
- Flutter (control application)

---

## System Architecture
- **Flutter App**
  - Sends control commands to fan
  - Receives and displays fan status and temperature
- **HiveMQ Cloud**
  - Acts as MQTT Broker
- **ESP32-C3**
  - Subscribes to control topics
  - Publishes status and temperature data
  - Controls relay, buzzer, and LED

>  <img width="1784" height="1011" alt="image" src="https://github.com/user-attachments/assets/a1c38ef6-8729-4317-99b1-2807826ab422" /> 

---

## GPIO Pin Mapping
>  <img width="1111" height="507" alt="image" src="https://github.com/user-attachments/assets/72bb29f5-9f51-4c9f-b4d7-685ada509f1a" /> 

## Project Configuration

### 1. Open Project Configuration

idf.py menuconfig

### 2. Configure WiFi

Go to Example Connection Configuration

Enter:

WiFi SSID

WiFi Password

### 3. Configure MQTT

Enter HiveMQ Cloud MQTT Broker URI

MQTT v5 (CONFIG_MQTT_PROTOCOL_5) is enabled by default in sdkconfig.defaults

Note:
If MQTT URI is set to FROM_STDIN, the broker address will be read from stdin at startup (mainly for testing purposes).

Build and Flash
idf.py build
idf.py -p PORT flash monitor


Replace PORT with your board's COM port (e.g., COM3 or /dev/ttyUSB0)

Exit the serial monitor using Ctrl + ]

Sample Output (Serial Monitor)

After a successful run, you will see:

ESP32 connected to WiFi

Connected to HiveMQ Cloud MQTT Broker

Commands received from Flutter App

Status and temperature data published to MQTT Broker

Example:

MQTT_EVENT_CONNECTED
MQTT_EVENT_SUBSCRIBED
MQTT_EVENT_DATA
TOPIC=/topic/qos1
DATA=data_3
MQTT_EVENT_DISCONNECTED

Applications

Remote control of 220V oscillating fan

Monitor ambient temperature

IoT Smart Home system

IoT / Embedded Systems coursework project

Easy extension for more sensors or dashboards

## Authors

Project implemented by the student group:

Le Thanh Ban

Le Thanh Tra

Tran Dai Tin

Nguyen Xuan Truong

