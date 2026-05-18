#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <MPU6050.h>
#include <math.h>

// WIFI SETTINGS
const char* ssid = "AndroidAP"; //chanege to your wifi ssid
const char* password = "MONEY2107"; //change to your wifi password

// WEB SERVER
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// MPU6050
MPU6050 mpu;

// SERVOS
Servo servoRoll;
Servo servoPitch;

#define ROLL_SERVO_PIN 18
#define PITCH_SERVO_PIN 19


// PID VARIABLES

// Target angles
float targetRoll = 0;
float targetPitch = 0;

bool calibrated = false;

// Current angles
float roll = 0;
float pitch = 0;

// PID errors
float rollError;
float pitchError;

float previousRollError = 0;
float previousPitchError = 0;

float rollIntegral = 0;
float pitchIntegral = 0;

// =====================================================
// PID TUNING
// =====================================================

float Kp = 3.5;
float Ki = 0.02;
float Kd = 1.2;

// SERVO POSITIONS

int rollServoPos = 90;
int pitchServoPos = 90;


// TIMING
unsigned long previousTime = 0;

// HTML PAGE
const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

  <title>ESP32 Self Leveling Spoon</title>

  <style>

    body {
      background: #111;
      color: white;
      font-family: Arial;
      text-align: center;
      margin-top: 40px;
    }

    h1 {
      font-size: 3em;
      color: #00ff88;
    }

    .data {
      font-size: 2em;
      margin: 20px;
    }

    .value {
      color: #00ccff;
    }

  </style>

</head>

<body>

<h1>ESP32 Spoon Stabilizer</h1>

<div class="data">
  Roll:
  <span class="value" id="roll">0</span>
</div>

<div class="data">
  Pitch:
  <span class="value" id="pitch">0</span>
</div>

<div class="data">
  Roll Servo:
  <span class="value" id="servoR">0</span>
</div>

<div class="data">
  Pitch Servo:
  <span class="value" id="servoP">0</span>
</div>

<script>

let gateway = `ws://${window.location.hostname}/ws`;

let websocket;

window.addEventListener('load', onLoad);

function onLoad() {
  initWebSocket();
}

function initWebSocket() {

  websocket = new WebSocket(gateway);

  websocket.onopen = () => {
    console.log("WebSocket Connected");
  };

  websocket.onclose = () => {

    console.log("WebSocket Disconnected");

    setTimeout(initWebSocket, 2000);
  };

  websocket.onmessage = (event) => {

    let data = JSON.parse(event.data);

    document.getElementById("roll").innerHTML =
      data.roll.toFixed(2);

    document.getElementById("pitch").innerHTML =
      data.pitch.toFixed(2);

    document.getElementById("servoR").innerHTML =
      data.servoR;

    document.getElementById("servoP").innerHTML =
      data.servoP;
  };
}

</script>

</body>
</html>

)rawliteral";

// =====================================================
// WEBSOCKET EVENT
// =====================================================

void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len) {

  if(type == WS_EVT_CONNECT) {

    Serial.println("Client Connected");
  }
}


void setup() {

  Serial.begin(9600);

  // I2C
  Wire.begin();

  // MPU6050 SETUP
  mpu.initialize();

  if (!mpu.testConnection()) {

    Serial.println("MPU6050 FAILED");

    while (1);
  }

  Serial.println("MPU6050 Connected");

  // SERVO SETUP
  servoRoll.attach(ROLL_SERVO_PIN);
  servoPitch.attach(PITCH_SERVO_PIN);

  // Center servos initially
  servoRoll.write(90);
  servoPitch.write(90);

  delay(1000);

  // AUTO CALIBRATION
  // PLACE SPOON FLAT DURING STARTUP

  Serial.println("");
  Serial.println("=================================");
  Serial.println("CALIBRATING...");
  Serial.println("KEEP SPOON FLAT AND STILL");
  Serial.println("=================================");

  float rollSum = 0;
  float pitchSum = 0;

  for(int i = 0; i < 200; i++) {

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    mpu.getMotion6(&ax, &ay, &az,
                   &gx, &gy, &gz);

    // Calculate accelerometer angles
    float accelRoll =
      atan2(ay, az) * 180 / PI;

    float accelPitch =
      atan2(-ax,
      sqrt(ay * ay + az * az))
      * 180 / PI;

    // Add to totals
    rollSum += accelRoll;
    pitchSum += accelPitch;

    delay(5);
  }

  // SET TARGET LEVEL POSITION
  targetRoll = rollSum / 200.0;
  targetPitch = pitchSum / 200.0;

  // RESET FILTER TO TARGET
  // PREVENTS STARTUP JUMP
  roll = targetRoll;
  pitch = targetPitch;

  calibrated = true;

  Serial.println("");
  Serial.println("Calibration Complete");

  Serial.print("Target Roll: ");
  Serial.println(targetRoll);

  Serial.print("Target Pitch: ");
  Serial.println(targetPitch);

  // =====================================================
  // WIFI CONNECTION
  // =====================================================

  WiFi.begin(ssid, password);

  Serial.println("");
  Serial.print("Connecting To WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");

  // Print IP address
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // WEBSOCKET SETUP
  ws.onEvent(onEvent);

  server.addHandler(&ws);


  // WEBPAGE ROUTE
  server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request) {

      request->send_P(200,
                      "text/html",
                      index_html);
    });


  // START SERVER
  server.begin();

  Serial.println("Web Server Started");


  // START TIMER

  previousTime = micros();

  Serial.println("");
  Serial.println("=================================");
  Serial.println("SELF LEVELING ACTIVE");
  Serial.println("=================================");
}

void loop() {

  // DELTA TIME
  unsigned long currentTime = micros();

  float dt =
    (currentTime - previousTime) / 1000000.0;

  previousTime = currentTime;

  // READ MPU6050
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az,&gx, &gy, &gz);

  // =====================================================
  // ACCEL ANGLES
  // =====================================================

  float accelRoll =
    atan2(ay, az) * 180 / PI;

  float accelPitch =
    atan2(-ax,
    sqrt(ay * ay + az * az))
    * 180 / PI;

  // =====================================================
  // GYRO RATES
  // =====================================================

  float gyroRollRate = gx / 131.0;
  float gyroPitchRate = gy / 131.0;

  // COMPLEMENTARY FILTER
  roll =
    0.98 * (roll + gyroRollRate * dt) +
    0.02 * accelRoll;

  pitch =
    0.98 * (pitch + gyroPitchRate * dt) +
    0.02 * accelPitch;

  // =====================================================
  // PID ROLL
  // =====================================================

  rollError = targetRoll - roll;

  rollIntegral += rollError * dt;

  float rollDerivative =
    (rollError - previousRollError) / dt;

  float rollOutput =
    (Kp * rollError) +
    (Ki * rollIntegral) +
    (Kd * rollDerivative);

  previousRollError = rollError;

  // =====================================================
  // PID PITCH
  // =====================================================

  pitchError = targetPitch - pitch;

  pitchIntegral += pitchError * dt;

  float pitchDerivative =
    (pitchError - previousPitchError) / dt;

  float pitchOutput =
    (Kp * pitchError) +
    (Ki * pitchIntegral) +
    (Kd * pitchDerivative);

  previousPitchError = pitchError;

  // =====================================================
  // SERVO OUTPUT
  // =====================================================

  rollServoPos = 90 + rollOutput;
  pitchServoPos = 90 + pitchOutput;

  rollServoPos =
    constrain(rollServoPos, 0, 180);

  pitchServoPos =
    constrain(pitchServoPos, 0, 180);

  servoRoll.write(rollServoPos);
  servoPitch.write(pitchServoPos);

  // =====================================================
  // CREATE JSON
  // =====================================================

  String json = "{";

  json += "\"roll\":" + String(roll) + ",";
  json += "\"pitch\":" + String(pitch) + ",";
  json += "\"servoR\":" + String(rollServoPos) + ",";
  json += "\"servoP\":" + String(pitchServoPos);

  json += "}";

  // SEND TO WEBPAGE
  ws.textAll(json);


  // SERIAL DEBUG
  Serial.print("Roll: ");
  Serial.print(roll);

  Serial.print("  Pitch: ");
  Serial.print(pitch);

  Serial.print("  ServoR: ");
  Serial.print(rollServoPos);

  Serial.print("  ServoP: ");
  Serial.println(pitchServoPos);

  // FAST LOOP

  delay(5);
}
