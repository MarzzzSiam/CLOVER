# 🤖 CLOVER — Autonomous Sanitizing Rover

### CSE-438: Robotics Lab Project

**CLOVER** is an IoT-based sanitizing rover designed to remotely navigate an environment, detect obstacles, and dispense disinfectant using an automated spraying mechanism.

---

## ⚙️ Features

- 📡 Wi-Fi-enabled remote control using ESP32
- 🛞 Four-wheel rover movement
- 🚧 Ultrasonic obstacle detection
- 💧 Automated disinfectant spraying
- 🔄 Servo-controlled spray direction
- 🔌 Organized sensor and actuator interfacing using an ESP32 Expansion Board

---

## 🛠️ Hardware

- **ESP32** — Main microcontroller & Wi-Fi connectivity
- **ESP32 Expansion Board** — Sensor and actuator interfacing
- **L298N Motor Driver** — Motor control
- **4× TT Gear Motors** — Rover movement
- **HC-SR04 Ultrasonic Sensor** — Obstacle detection
- **Buzzer** — Alerts when an object is detected
- **Servo Motor** — Spray direction control
- **5V Water Pump** — Disinfectant dispensing
- **Relay Module** — Pump control

---

## 🧠 Working Principle

The ESP32 controls the rover's movement and communicates with the connected components through the expansion board. The **HC-SR04 ultrasonic sensor** detects obstacles in the rover's path, while the **servo motor and 5V water pump** control the direction and dispensing of disinfectant.

---

**Course:** CSE-438 — Robotics Lab  
**Department:** Computer Science & Engineering  
**University of Asia Pacific**

---

> 🚀 **CLOVER — Clean. Navigate. Protect.**
