#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================
//                    WI-FI CONFIGURATION
// ============================================================

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_PASSWORD";

// Your laptop running the AEGIS backend
const char* serverURL = "http://YOUR_BACKEND_HOST:3000/alert";

// Unique ID for this AEGIS device
const char* deviceID = "AEGIS-001";

// ============================================================
//                    MPU6500 SETUP
// ============================================================

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

// ============================================================
//                    VIBRATOR
// ============================================================

// Vibrator controlled through IRLZ44N MOSFET
#define VIBRATOR_PIN 25

// ============================================================
//                    SHAKE SETTINGS
// ============================================================

#define SHAKE_THRESHOLD 1.0
#define SHAKE_DEBOUNCE_MS 1500
#define VIBRATE_DURATION_MS 20000

unsigned long lastTriggerTime = 0;

bool isVibrating = false;

unsigned long vibrateStartTime = 0;


// ============================================================
//                    WIFI CONNECTION
// ============================================================

void connectToWiFi() {

  Serial.println();
  Serial.println("================================");
  Serial.println("       AEGIS Wi-Fi");
  Serial.println("================================");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {

    delay(500);
    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("Wi-Fi connection failed!");

  }
}


// ============================================================
//              RECONNECT WIFI IF CONNECTION DROPS
// ============================================================

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Wi-Fi disconnected. Attempting reconnection...");

  WiFi.disconnect();
  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 10) {

    delay(500);
    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi reconnected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("Wi-Fi reconnection failed.");

  }
}


// ============================================================
//                    SEND SOS ALERT
// ============================================================

void sendSOSAlert(float magnitude) {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println();
    Serial.println("!!! SOS DETECTED !!!");
    Serial.println("Wi-Fi unavailable.");
    Serial.println("SOS could NOT be sent to backend.");

    return;
  }

  HTTPClient http;

  Serial.println();
  Serial.println("================================");
  Serial.println("      SENDING SOS ALERT");
  Serial.println("================================");

  Serial.print("Backend: ");
  Serial.println(serverURL);

  http.begin(serverURL);

  http.addHeader("Content-Type", "application/json");

  // ----------------------------------------------------------
  // NOTE:
  // This hardware sketch currently has no GPS.
  // Therefore we do NOT send a fake location.
  // GPS can be integrated later.
  // ----------------------------------------------------------

  String jsonData = "{";

  jsonData += "\"deviceId\":\"";
  jsonData += deviceID;
  jsonData += "\",";

  jsonData += "\"location\":{";
  jsonData += "\"lat\":0,";
  jsonData += "\"lng\":0,";
  jsonData += "\"label\":\"Location unavailable\"";
  jsonData += "},";

  jsonData += "\"triggerType\":\"shake_detection\"";

  jsonData += "}";

  Serial.println("Sending data:");
  Serial.println(jsonData);

  int httpResponseCode = http.POST(jsonData);

  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {

    String response = http.getString();

    Serial.println("Backend response:");
    Serial.println(response);

    if (httpResponseCode == 201) {

      Serial.println();
      Serial.println(">>> SOS SUCCESSFULLY SENT TO BACKEND <<<");

    }

  } else {

    Serial.println();
    Serial.print("Backend communication failed: ");
    Serial.println(http.errorToString(httpResponseCode));

  }

  http.end();
}


// ============================================================
//                         SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("        AEGIS DEVICE");
  Serial.println("================================");

  // ----------------------------------------------------------
  // MPU6500
  // ----------------------------------------------------------

  Wire.begin(21, 22);

  Wire.beginTransmission(MPU_ADDR);

  Wire.write(PWR_MGMT_1);

  Wire.write(0);

  Wire.endTransmission(true);

  Serial.println("MPU6500 initialized.");


  // ----------------------------------------------------------
  // Vibrator
  // ----------------------------------------------------------

  pinMode(VIBRATOR_PIN, OUTPUT);

  digitalWrite(VIBRATOR_PIN, LOW);

  Serial.println("Vibrator initialized.");


  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  connectToWiFi();


  Serial.println();
  Serial.println("================================");
  Serial.println(" Setup complete.");
  Serial.println(" Watching for shake events...");
  Serial.println("================================");

  delay(500);
}


// ============================================================
//                          LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // Maintain Wi-Fi connection
  // ----------------------------------------------------------

  maintainWiFi();


  // ----------------------------------------------------------
  // Read MPU6500 accelerometer
  // ----------------------------------------------------------

  Wire.beginTransmission(MPU_ADDR);

  Wire.write(ACCEL_XOUT_H);

  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, 6, true);


  int16_t rawX = (Wire.read() << 8) | Wire.read();

  int16_t rawY = (Wire.read() << 8) | Wire.read();

  int16_t rawZ = (Wire.read() << 8) | Wire.read();


  float ax = rawX / 16384.0;

  float ay = rawY / 16384.0;

  float az = rawZ / 16384.0;


  float magnitude = sqrt(
    ax * ax +
    ay * ay +
    az * az
  );


  unsigned long now = millis();


  // ----------------------------------------------------------
  // SHAKE DETECTION
  // ----------------------------------------------------------

  if (
    !isVibrating &&
    magnitude > SHAKE_THRESHOLD &&
    (now - lastTriggerTime > SHAKE_DEBOUNCE_MS)
  ) {

    Serial.println();
    Serial.println(">>> SHAKE DETECTED <<<");

    Serial.print("Acceleration magnitude: ");
    Serial.print(magnitude, 2);
    Serial.println(" g");


    // --------------------------------------------------------
    // Start vibrator
    // --------------------------------------------------------

    lastTriggerTime = now;

    isVibrating = true;

    vibrateStartTime = now;

    digitalWrite(VIBRATOR_PIN, HIGH);

    Serial.println(">>> VIBRATOR ON — 20 seconds");


    // --------------------------------------------------------
    // Send SOS to backend
    // --------------------------------------------------------

    sendSOSAlert(magnitude);
  }


  // ----------------------------------------------------------
  // Turn vibrator OFF after 20 seconds
  // ----------------------------------------------------------

  if (
    isVibrating &&
    (now - vibrateStartTime >= VIBRATE_DURATION_MS)
  ) {

    digitalWrite(VIBRATOR_PIN, LOW);

    isVibrating = false;

    Serial.println(">>> Vibration ended");
  }


  // ----------------------------------------------------------
  // Serial monitoring
  // ----------------------------------------------------------

  Serial.print("Magnitude: ");

  Serial.print(magnitude, 2);

  Serial.print("   Vibrating: ");

  Serial.println(
    isVibrating ? "YES" : "no"
  );


  delay(100);
}