#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================
//                    WI-FI / BACKEND CONFIG
// ============================================================

const char* ssid = "";
const char* password = "";

// TIP: use a static/reserved IP for the laptop (router DHCP reservation),
// otherwise the IP can change and alerts silently go nowhere.
const char* serverURL = "http://:3000/alert";
const char* deviceID = "AEGIS-001";

// ============================================================
//                 MPU6500 / VIBRATOR / ESP32
// ============================================================

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define I2C_SDA 8
#define I2C_SCL 9
#define VIBRATOR_PIN 4

// ============================================================
//                    SETTINGS
// ============================================================

#define SHAKE_THRESHOLD 1.0
#define SHAKE_DEBOUNCE_MS 1500
#define VIBRATE_DURATION_MS 20000

#define SAMPLE_INTERVAL_MS 10  // ~100 Hz sampling (was 10 Hz)
#define SERIAL_STATUS_MS 1000  // status print rate

#define HTTP_CONNECT_TIMEOUT_MS 3000
#define HTTP_TIMEOUT_MS 4000
#define RETRY_INTERVAL_MS 2000  // wait between failed attempts
#define WIFI_RETRY_INTERVAL_MS 5000

// ============================================================
//                    STATE
// ============================================================

unsigned long lastTriggerTime = 0;
bool isVibrating = false;
unsigned long vibrateStartTime = 0;

// Pending alert (stays pending until backend confirms with 2xx)
bool pendingAlert = false;
String pendingAlertId = "";
float pendingMagnitude = 0;
unsigned long pendingSince = 0;
unsigned long nextAttemptAt = 0;
int attemptCount = 0;

unsigned long lastWifiRetry = 0;
unsigned long lastSerialStatus = 0;

// ============================================================
//                    WIFI (NON-BLOCKING)
// ============================================================

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // power-save causes dropped/late packets
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi not connected yet (will keep retrying in background).");
  }
}

// Never blocks: just kicks off a reconnect every few seconds if needed.
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastWifiRetry < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiRetry = now;

  Serial.println("Wi-Fi down. Reconnecting...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
}

// ============================================================
//                    SEND ALERT (returns true on success)
// ============================================================

bool trySendAlert() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Send skipped: Wi-Fi not connected.");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);  // fresh connection each time, avoids stale sockets

  if (!http.begin(client, serverURL)) {
    Serial.println("http.begin() failed.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  // alertId lets the backend ignore duplicates when we retry.
  String json = "{";
  json += "\"alertId\":\"" + pendingAlertId + "\",";
  json += "\"deviceId\":\"" + String(deviceID) + "\",";
  json += "\"location\":{\"lat\":0,\"lng\":0,\"label\":\"Location unavailable\"},";
  json += "\"triggerType\":\"shake_detection\",";
  json += "\"attempt\":" + String(attemptCount) + ",";
  json += "\"delayMs\":" + String(millis() - pendingSince);
  json += "}";

  Serial.print("Attempt #");
  Serial.print(attemptCount);
  Serial.print(" -> ");
  Serial.println(serverURL);

  int code = http.POST(json);
  Serial.print("HTTP code: ");
  Serial.println(code);

  bool ok = (code >= 200 && code < 300);

  if (code > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(code));
  }

  http.end();
  return ok;
}

// Called every loop; sends/retries until the backend confirms.
void processPendingAlert() {
  if (!pendingAlert) return;

  unsigned long now = millis();
  if ((long)(now - nextAttemptAt) < 0) return;

  attemptCount++;
  if (trySendAlert()) {
    Serial.println(">>> SOS CONFIRMED BY BACKEND <<<");
    pendingAlert = false;
  } else {
    nextAttemptAt = millis() + RETRY_INTERVAL_MS;
    Serial.println("SOS not delivered yet. Will retry.");
  }
}

void queueAlert(float magnitude) {
  pendingAlert = true;
  pendingMagnitude = magnitude;
  pendingSince = millis();
  attemptCount = 0;
  nextAttemptAt = 0;  // send immediately
  pendingAlertId = String(deviceID) + "-" + String(millis()) + "-" + String(esp_random() & 0xFFFF, HEX);
}

// ============================================================
//                    MPU READ
// ============================================================

bool readAccel(float& ax, float& ay, float& az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((uint16_t)MPU_ADDR, (uint8_t)6, true) != 6) return false;

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  ax = rawX / 16384.0;
  ay = rawY / 16384.0;
  az = rawZ / 16384.0;
  return true;
}

// ============================================================
//                    SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000) { delay(10); }
  delay(1000);
  Serial.println("\n=== AEGIS DEVICE ===");

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.println("MPU6500 initialized.");

  pinMode(VIBRATOR_PIN, OUTPUT);
  digitalWrite(VIBRATOR_PIN, LOW);

  connectToWiFi();

  Serial.println("Setup complete. Watching for shake events...");
}

// ============================================================
//                    LOOP
// ============================================================

void loop() {
  maintainWiFi();

  float ax, ay, az;
  float magnitude = 0;

  if (readAccel(ax, ay, az)) {
    magnitude = sqrt(ax * ax + ay * ay + az * az);

    unsigned long now = millis();

    // ---- Shake detection ----
    if (!isVibrating && magnitude > SHAKE_THRESHOLD && (now - lastTriggerTime > SHAKE_DEBOUNCE_MS)) {

      Serial.println("\n>>> SHAKE DETECTED <<<");
      Serial.print("Magnitude: ");
      Serial.println(magnitude, 2);

      lastTriggerTime = now;
      isVibrating = true;
      vibrateStartTime = now;
      digitalWrite(VIBRATOR_PIN, HIGH);

      queueAlert(magnitude);  // only queues; actual send happens below
    }
  } else {
    Serial.println("MPU read failed.");
  }

  // ---- Send / retry pending alert ----
  processPendingAlert();

  // ---- Vibrator off after duration ----
  if (isVibrating && (millis() - vibrateStartTime >= VIBRATE_DURATION_MS)) {
    digitalWrite(VIBRATOR_PIN, LOW);
    isVibrating = false;
    Serial.println(">>> Vibration ended");
  }

  // ---- Slow status print (Serial spam slows the loop) ----
  if (millis() - lastSerialStatus >= SERIAL_STATUS_MS) {
    lastSerialStatus = millis();
    Serial.print("Mag: ");
    Serial.print(magnitude, 2);
    Serial.print("  Vib: ");
    Serial.print(isVibrating ? "YES" : "no");
    Serial.print("  Pending: ");
    Serial.println(pendingAlert ? "YES" : "no");
  }

  delay(SAMPLE_INTERVAL_MS);
}
