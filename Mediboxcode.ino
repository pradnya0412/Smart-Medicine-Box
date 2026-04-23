//================ BLYNK DETAILS =================
#define BLYNK_TEMPLATE_ID   "TMPL3mPqfeuXV"
#define BLYNK_TEMPLATE_NAME "Medicinebox"
#define BLYNK_AUTH_TOKEN    "NTu7ri_40gdnpz0BQ6K9ZGs5B2cI0uVp"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <RTClib.h>

//================ WiFi =================
char ssid[] = "Natasha";
char pass[] = "24681012";

//================ PINS =================
#define LED1   25
#define LED2   26
#define LED3   27
#define IR1    34
#define IR2    35
#define IR3    32
#define BUZZER 33

//================ BLYNK VIRTUAL PINS =================
// V0  → LED widget C1 (LED widget in app)
// V1  → LED widget C2
// V2  → LED widget C3
// V3  → Label: Status message
// V4  → Label: Taken message
// V5  → Label: Missed message
// V6  → Number Input: C1 Hour
// V7  → Number Input: C1 Minute
// V8  → Number Input: C2 Hour
// V9  → Number Input: C2 Minute
// V10 → Number Input: C3 Hour
// V11 → Number Input: C3 Minute

//================ RTC =================
RTC_DS3231 rtc;
bool rtcSynced = false;

//================ SCHEDULE =================
int  schedHour[3]   = {0, 0, 0};
int  schedMin[3]    = {0, 0, 0};
bool hourSet[3]     = {false, false, false};
bool minSet[3]      = {false, false, false};

//================ STATE MACHINE =================
// Each compartment has one of these states:
// 0 = IDLE       → waiting for scheduled time
// 1 = ALERTING   → buzzer+LED ON, waiting for IR (30s window)
// 2 = TAKEN      → IR detected, notification sent
// 3 = MISSED     → 30s elapsed, missed notification sent
int  state[3] = {0, 0, 0};

unsigned long alertStartTime[3] = {0, 0, 0};

#define TIMEOUT_MS   30000   // 30 seconds
#define DEBOUNCE_MS  300

int ledPins[3] = {LED1, LED2, LED3};
int irPins[3]  = {IR1,  IR2,  IR3};
int appLED[3]  = {V0,   V1,   V2};

// Event queue — sends one logEvent per 600ms to avoid rate limiting
bool          pendingEvent[3]   = {false, false, false};
bool          eventIsTaken[3]   = {false, false, false};
unsigned long lastEventTime     = 0;
#define EVENT_GAP_MS  600

// Grace period after boot — prevents false triggers during syncVirtual
unsigned long bootTime      = 0;
bool          systemReady   = false;

//================ BUZZER =================
void buzzerOn()  { tone(BUZZER, 1000); }
void buzzerOff() { noTone(BUZZER); }

bool anyAlerting() {
  return state[0] == 1 || state[1] == 1 || state[2] == 1;
}

//================ RTC TIME =================
void getTime(int &h, int &m, int &s) {
  DateTime now = rtc.now();
  h = now.hour();
  m = now.minute();
  s = now.second();
}

//================ BLYNK RTC SYNC =================
BLYNK_WRITE(InternalPinRTC) {
  rtc.adjust(DateTime(param.asLong()));
  rtcSynced = true;
  Serial.println("✅ RTC synced with Blynk server time!");
}

//================ BLYNK CONNECTED =================
bool blynkConnected = false;

BLYNK_CONNECTED() {
  blynkConnected = true;
  Serial.println("✅ Blynk Connected!");

  // Sync RTC with server time
  Blynk.sendInternal("rtc", "sync");

  // Reset boot timer — gives 5s grace before alerts can trigger
  bootTime    = millis();
  systemReady = false;

  // Pull saved schedule values from server
  Blynk.syncVirtual(V6, V7, V8, V9, V10, V11);

  // Reset app display
  Blynk.virtualWrite(V3, "System Online ✅");
  Blynk.virtualWrite(V4, " ");
  Blynk.virtualWrite(V5, " ");
  Blynk.virtualWrite(V0, 0);
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
}

BLYNK_DISCONNECTED() {
  blynkConnected = false;
  Serial.println("❌ Blynk Disconnected!");
}

//================ TIME INPUT FROM APP =================
BLYNK_WRITE(V6)  { schedHour[0] = param.asInt(); hourSet[0] = true;
                   Serial.printf("C1 Hour set: %d\n", schedHour[0]); }
BLYNK_WRITE(V7)  { schedMin[0]  = param.asInt(); minSet[0]  = true;
                   Serial.printf("C1 Min  set: %d\n", schedMin[0]); }
BLYNK_WRITE(V8)  { schedHour[1] = param.asInt(); hourSet[1] = true;
                   Serial.printf("C2 Hour set: %d\n", schedHour[1]); }
BLYNK_WRITE(V9)  { schedMin[1]  = param.asInt(); minSet[1]  = true;
                   Serial.printf("C2 Min  set: %d\n", schedMin[1]); }
BLYNK_WRITE(V10) { schedHour[2] = param.asInt(); hourSet[2] = true;
                   Serial.printf("C3 Hour set: %d\n", schedHour[2]); }
BLYNK_WRITE(V11) { schedMin[2]  = param.asInt(); minSet[2]  = true;
                   Serial.printf("C3 Min  set: %d\n", schedMin[2]); }

//================ EVENT QUEUE PROCESSOR =================
// Sends ONE logEvent every 600ms — prevents Blynk rate limit drops
void processEventQueue() {
  if (!blynkConnected) return;
  if (millis() - lastEventTime < EVENT_GAP_MS) return;

  for (int i = 0; i < 3; i++) {
    if (!pendingEvent[i]) continue;

    pendingEvent[i] = false;
    lastEventTime   = millis();

    String compName = "C" + String(i + 1);

    if (eventIsTaken[i]) {
      // Event codes: c1_taken, c2_taken, c3_taken
      String code = "c" + String(i + 1) + "_taken";
      Blynk.logEvent(code, compName + " medicine was taken!");
      Serial.printf("📲 logEvent sent: %s\n", code.c_str());
    } else {
      // Event codes: c1_missed, c2_missed, c3_missed
      String code = "c" + String(i + 1) + "_missed";
      Blynk.logEvent(code, compName + " dose was MISSED!");
      Serial.printf("📲 logEvent sent: %s\n", code.c_str());
    }

    return; // Only one event per loop pass
  }
}

//================ SEND TAKEN =================
void handleTaken(int i) {
  state[i] = 2;
  digitalWrite(ledPins[i], LOW);
  if (!anyAlerting()) buzzerOff();

  Serial.printf("✅ C%d TAKEN\n", i + 1);

  if (blynkConnected) {
    Blynk.virtualWrite(appLED[i], 0);
    Blynk.virtualWrite(V4, "✅ C" + String(i+1) + " medicine taken!");
    Blynk.virtualWrite(V3, "✅ C" + String(i+1) + " Taken!");
  }

  // Queue the logEvent
  pendingEvent[i]  = true;
  eventIsTaken[i]  = true;
}

//================ SEND MISSED =================
void handleMissed(int i) {
  state[i] = 3;
  digitalWrite(ledPins[i], LOW);
  if (!anyAlerting()) buzzerOff();

  Serial.printf("❌ C%d MISSED\n", i + 1);

  if (blynkConnected) {
    Blynk.virtualWrite(appLED[i], 0);
    Blynk.virtualWrite(V5, "❌ C" + String(i+1) + " dose MISSED!");
    Blynk.virtualWrite(V3, "❌ C" + String(i+1) + " Missed!");
  }

  // Queue the logEvent
  pendingEvent[i]  = true;
  eventIsTaken[i]  = false;
}

//================ ACTIVATE ALERT =================
void activateAlert(int i) {
  state[i]          = 1;
  alertStartTime[i] = millis();

  digitalWrite(ledPins[i], HIGH);
  buzzerOn();

  if (blynkConnected) {
    Blynk.virtualWrite(appLED[i], 255);
    Blynk.virtualWrite(V3, "⏰ C" + String(i+1) + " Take medicine!");
  }

  Serial.printf("🔔 ALERT: C%d — 30s window started\n", i + 1);
}

//================ SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("  Smart Medicine Reminder     ");
  Serial.println("==============================");

  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(irPins[i],  INPUT);
    digitalWrite(ledPins[i], LOW);
  }
  pinMode(BUZZER, OUTPUT);
  buzzerOff();

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("❌ RTC NOT FOUND! Check wiring.");
    while (1) delay(1000);
  }
  Serial.println("✅ RTC found!");

  if (rtc.lostPower()) {
    Serial.println("⚠️  RTC lost power — setting compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  bootTime = millis();
  Serial.println("🔌 Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("✅ Blynk connected! System starting...");
}

//================ LOOP =================
void loop() {
  Blynk.run();
  processEventQueue();

  // 5 second grace period after boot/reconnect
  if (!systemReady) {
    if (millis() - bootTime > 5000) {
      systemReady = true;
      Serial.println("✅ System ready — alerts active!");
    } else {
      return; // Skip all alert logic during grace period
    }
  }

  int h, m, s;
  getTime(h, m, s);

  // Debug print every 15 seconds
  static int lastDebugSec = -1;
  if (s % 15 == 0 && s != lastDebugSec) {
    lastDebugSec = s;
    Serial.printf("🕐 %02d:%02d:%02d | %s | RTC:%s\n",
      h, m, s,
      blynkConnected ? "Online✅" : "Offline❌",
      rtcSynced      ? "Synced✅" : "Unsynced⚠️");
    for (int i = 0; i < 3; i++) {
      bool ready = hourSet[i] && minSet[i];
      Serial.printf("   C%d [%s]: %02d:%02d | state=%d\n",
        i+1,
        ready ? "SET✅" : "NOT SET⏳",
        schedHour[i], schedMin[i],
        state[i]);
    }
  }

  for (int i = 0; i < 3; i++) {

    // Skip if time not configured yet
    if (!hourSet[i] || !minSet[i]) continue;

    //------------------------------------------
    // STATE 0: IDLE — check if time matches
    //------------------------------------------
    if (state[i] == 0) {
      if (h == schedHour[i] && m == schedMin[i] && s < 3) {
        activateAlert(i);
      }
    }

    //------------------------------------------
    // STATE 1: ALERTING — check IR or timeout
    //------------------------------------------
    else if (state[i] == 1) {

      // Check IR sensor — LOW means object detected
      if (digitalRead(irPins[i]) == LOW) {
        delay(DEBOUNCE_MS);
        if (digitalRead(irPins[i]) == LOW) {
          handleTaken(i);
          continue;
        }
      }

      // Check 30 second timeout
      if (millis() - alertStartTime[i] >= TIMEOUT_MS) {
        handleMissed(i);
      }
    }

    // STATE 2 (TAKEN) and STATE 3 (MISSED) do nothing
    // until midnight reset
  }

  // Keep buzzer ON as long as any compartment is alerting
  if (anyAlerting()) buzzerOn();
  else               buzzerOff();

  //------------------------------------------
  // MIDNIGHT RESET — fresh start every day
  //------------------------------------------
  if (h == 0 && m == 0 && s < 2) {
    Serial.println("🔄 Midnight reset...");
    for (int i = 0; i < 3; i++) {
      state[i]        = 0;
      pendingEvent[i] = false;
      digitalWrite(ledPins[i], LOW);
      if (blynkConnected) Blynk.virtualWrite(appLED[i], 0);
    }
    buzzerOff();
    if (blynkConnected) Blynk.virtualWrite(V3, "🌙 New day — ready!");
    Serial.println("✅ Reset done.");
    delay(2000);
  }

  delay(200);
}