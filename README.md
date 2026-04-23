# Smart-Medicine-Box
Smart medicine reminder box usingESP32, RTC , IR sensors &amp; Blynk  notifications
# 💊 Smart Medicine Reminder Box


##  Project Overview

The **Smart Medicine Reminder Box** is an IoT-based system that:
- Reminds patients to take medicine at scheduled times via LED and buzzer alerts
- Detects medicine removal using IR sensors
- Sends real-time notifications to the Blynk app (medicine taken / dose missed)
- Syncs time with Blynk server via DS3231 RTC module
- Automatically resets daily at midnight



##  Hardware Components

| Component        | Quantity | Purpose                         |
|------------------|----------|---------------------------------|
| ESP32 Dev Board  | 1        | Microcontroller + WiFi          |
| DS3231 RTC       | 1        | Real-time clock / time sync     |
| IR Sensor        | 3        | Detect medicine presence/removal|
| LED              | 3        | Visual alert per compartment    |
| Buzzer           | 1        | Audio alert                     |
| 220Ω Resistors   | 3        | LED current limiting            |
| Jumper Wires     | –        | Connections                     |
| Breadboard/PCB   | 1        | Circuit assembly                |



##  Circuit Connections

| Component | Pin | ESP32 GPIO |
|-----------|-----|------------|
| IR Sensor 1 | OUT | GPIO 34 |
| IR Sensor 2 | OUT | GPIO 35 |
| IR Sensor 3 | OUT | GPIO 32 |
| LED 1 | + | GPIO 26 (via 220Ω) |
| LED 2 | + | GPIO 27 (via 220Ω) |
| LED 3 | + | GPIO 14 (via 220Ω) |
| Buzzer | + | GPIO 25 |
| DS3231 | SDA | GPIO 21 |
| DS3231 | SCL | GPIO 22 |
| All VCC | – | 3.3V |
| All GND | – | GND |




## 📱 Blynk App Setup

### Virtual Pins
| Pin | Widget | Purpose |
|-----|--------|---------|
| V0 | Time Input | Medicine 1 Schedule Time |
| V1 | Time Input | Medicine 2 Schedule Time |
| V2 | Time Input | Medicine 3 Schedule Time |
| V3 | LED Widget | Medicine 1 Status |
| V4 | LED Widget | Medicine 2 Status |
| V5 | LED Widget | Medicine 3 Status |

### Events
- `medicine_taken` → Medicine Taken 
- `dose_missed` → Dose Missed 



## Libraries Required

| Library | Author | Install via |
|---------|--------|-------------|
| Blynk | Volodymyr Shymanskyy | Arduino Library Manager |
| RTClib | Adafruit | Arduino Library Manager |
| Time | Michael Margolis | Arduino Library Manager |
| Wire | Built-in | Pre-installed |

---

## 🔄 How It Works
Power ON
↓
Connect to WiFi + Blynk
↓
Sync time via Blynk server → DS3231 RTC (IST UTC+5:30)
↓
User sets medicine times in Blynk app (V0, V1, V2)
↓
At scheduled time:
→ LED turns ON + Buzzer beeps
↓
IR Sensor detects medicine:
HIGH = Medicine still in box
LOW  = Medicine removed (TAKEN)
↓
If IR goes LOW (medicine removed):
→ LED OFF, Buzzer stops
→ Blynk notification: "Medicine Taken ✅"
↓
If 30 seconds pass and IR still HIGH:
→ LED OFF, Buzzer stops
→ Blynk notification: "MISSED DOSE ⚠️"
↓
Resets at midnight → Ready for next day
---

## ✅ Test Results

| Test Case | Expected Output | Result |
|-----------|----------------|--------|
| Medicine removed at alarm time | LED OFF, buzzer stops, notification sent | ✅ Pass |
| Medicine not removed within 30 sec | Missed dose notification sent | ✅ Pass |
| Time sync on boot via Blynk | RTC updated to correct IST time | ✅ Pass |
| All 3 compartments triggered | All 3 LEDs and buzzer activate | ✅ Pass |
| Daily reset at midnight | All states cleared automatically | ✅ Pass |

---

## 🚀 Future Scope
- Add OLED display to show next medicine time
- Add more compartments for weekly pill planning
- Integrate voice alerts using DFPlayer Mini module
- Cloud data logging of daily medicine intake history
- Mobile camera to visually confirm medicine removal

---

## 📊 Applications
- Elderly patient medicine management at home
- Hospital ward medicine dispensing reminders
- Chronic disease patients daily dose tracking
- Hostel students managing their own medication

---

