
<div align="center">

# Smart Health Companion

### IoT-Based Wearable Health Monitoring System

Real-Time Monitoring of Heart Rate, SpO₂, and Body Temperature using **IndusBoard Coin V2 (ESP32-S2)**

![Status](https://img.shields.io/badge/Status-Completed-success)
![Platform](https://img.shields.io/badge/Platform-IndusBoard_Coin_V2-blue)
![Controller](https://img.shields.io/badge/MCU-ESP32--S2-orange)
![Communication](https://img.shields.io/badge/Protocol-MQTT-green)
![Language](https://img.shields.io/badge/Language-C++-00599C)
![License](https://img.shields.io/badge/License-MIT-yellow)

</div>

---

# Overview

Smart Health Companion is an IoT-enabled wearable health monitoring system developed to continuously monitor vital health parameters and transmit them to a cloud platform using MQTT.

The system integrates biomedical sensors with the IndusBoard Coin V2 (ESP32-S2) to acquire physiological data, display it locally on an OLED display, and publish it to an MQTT broker for remote monitoring.

The project demonstrates the integration of embedded systems, sensor interfacing, wireless communication, and cloud connectivity into a compact wearable health monitoring platform.

---

# Key Features

* Real-Time Heart Rate Monitoring
* Blood Oxygen (SpO₂) Monitoring
* Body Temperature Monitoring
* OLED-Based Live Data Display
* MQTT-Based Cloud Communication
* Wi-Fi Connectivity
* Continuous Health Monitoring

---

# Hardware Components

| Component                     | Description               |
| ----------------------------- | ------------------------- |
| IndusBoard Coin V2 (ESP32-S2) | Main Controller           |
| MAX30102                      | Heart Rate & SpO₂ Sensor  |
| LM35                          | Body Temperature Sensor   |
| SH1106 OLED Display           | Real-Time Display         |
| Built-in Accelerometer        | Motion Sensing (On-board) |

---

# Software Stack

| Category                | Technology                                |
| ----------------------- | ----------------------------------------- |
| Programming Language    | C++                                       |
| Development Environment | Arduino IDE                               |
| Communication Protocol  | MQTT                                      |
| Connectivity            | Wi-Fi                                     |
| Controller              | ESP32-S2                                  |
| Libraries               | ArduinoJson, PubSubClient, MAX30105, U8g2 |

---

# System Architecture

```text
          MAX30102
             │
             │
          LM35 Sensor
             │
             ▼
   IndusBoard Coin V2 (ESP32-S2)
        │                │
        │                │
        ▼                ▼
 SH1106 OLED        Wi-Fi Module
    Display             │
                         ▼
                    MQTT Broker
                         │
                         ▼
                 Cloud Dashboard
```

---

# Repository Structure

```text
Smart-Health-Companion
│
├── Documentation
│
├── Firmware
│
├── Hardware
│
├── Images
│
├── Results
│
├── README.md
│
└── LICENSE
```

---

# Results

| Function               |   Status  |
| ---------------------- | :-------: |
| Heart Rate Monitoring  | Completed |
| SpO₂ Monitoring        | Completed |
| Temperature Monitoring | Completed |
| OLED Display           | Completed |
| MQTT Communication     | Completed |
| Cloud Monitoring       | Completed |

---

# Future Scope

* Mobile Application Integration
* Health Data Logging
* Emergency SOS Alert System
* AI-Based Health Analysis
* Cloud Database Integration
* Remote Patient Monitoring

---

# Project Gallery

The project images, hardware setup, OLED output, dashboard screenshots, and testing results are available in the **Images** directory.

---

# Demonstration

A demonstration video of the Smart Health Companion is available in the **Videos** directory.

---

# Author

**Shashank H**

Electronics and Telecommunication Engineering

Siddaganga Institute of Technology

---

# License

This project is licensed under the MIT License.

