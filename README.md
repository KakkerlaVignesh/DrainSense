# DrainSense 🚰

**Smart Drainage Monitoring System powered by ESP32**

DrainSense is an IoT prototype that uses an ultrasonic sensor to measure how fast water drains through a pipe or outlet. It hosts a live web dashboard over Wi-Fi, alerts you with a buzzer if drainage is slow, and keeps a history of recent test results — all without needing the internet.

---

## Features

- 📡 **Ultrasonic distance sensing** — detects water level drop in real time
- 🌐 **Built-in Wi-Fi dashboard** — connect directly to the ESP32's access point
- 🔔 **Active buzzer alert** — sounds when slow drainage is detected
- 📊 **Live metrics** — distance, elapsed time, test status and result
- 🕓 **Test history** — keeps the last 5 test results on-screen
- 🔄 **Two test modes** — Full drainage & Restricted drainage

---

## Hardware Requirements

| Component | Details |
|---|---|
| Microcontroller | ESP32 (any standard dev board) |
| Ultrasonic sensor | HC-SR04 |
| Buzzer | Active buzzer (active-low) |
| Power | USB or 5 V supply |

### Wiring

| Signal | ESP32 Pin |
|---|---|
| Trig | GPIO 5 |
| Echo | GPIO 18 |
| Buzzer | GPIO 23 |

---

## How It Works

1. Mount the HC-SR04 sensor above a bottle or container filled with water.
2. Connect your phone or laptop to the **DrainSense** Wi-Fi network (`drain123`).
3. Open a browser and go to `http://192.168.4.1`.
4. Press **START FULL DRAINAGE TEST** or **START RESTRICTED DRAINAGE TEST**.
5. Allow the water to drain naturally.
6. The test completes automatically once the water level drops **3 cm** (detected by the sensor).
7. If the drainage takes longer than **8.27 s** (1.5× the baseline), a **SLOW DRAINAGE ALERT** is triggered and the buzzer sounds.

---

## Thresholds

| Parameter | Value |
|---|---|
| Normal baseline | 5.51 s |
| Slow-drain threshold | 8.27 s (1.5× baseline) |
| Required distance change | 3.0 cm |
| Test timeout | 90 s |

---

## Dashboard

The web dashboard is served directly from the ESP32 at `http://192.168.4.1`.

- **Live sensor distance** — current sensor-to-water-surface reading
- **Elapsed time** — time since the test started
- **Status indicator** — Ready / Test in progress / Slow drainage detected
- **Test history table** — last 5 test results with time and outcome
- **Controls** — Start, Reset, and Silence buzzer buttons

---

## Wi-Fi Credentials

| Field | Value |
|---|---|
| SSID | `DrainSense` |
| Password | `drain123` |
| Dashboard URL | `http://192.
