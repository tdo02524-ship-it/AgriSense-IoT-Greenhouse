# AgriSense-IoT-Greenhouse

Automated greenhouse monitoring system: collects sensor data, sends it to a
cloud server, and raises SMS/call alerts when thresholds are exceeded.

## Demo
![Hardware](images/hardware.png)
![Dashboard](images/dashboard.png)

## Features
- Real-time monitoring via web dashboard (and Android app)
- Offline operation with automatic data sync after network reconnection
- Threshold-based device control
- Emergency alerts via SMS or call
- Report generation and remote control

## Hardware & Technologies
- MCU: ESP32 (C)
- Sensor: Pt100
- RTC: DS1302
- Connectivity: 4G LTE module (SIM7680C), WiFi
- Server: Viettel Cloud Server, MQTTS, HTTP/RESTful API
- Web: HTML, JavaScript, Node.js (server.js)

## Repository Structure
- `Firm/`: ESP32 firmware
- `server.js`: server
- `web.html`: web dashboard

## Results
- >98% system stability
- Incident alerts delivered in <3 seconds

## Author
Do Ngoc Bao Tam
