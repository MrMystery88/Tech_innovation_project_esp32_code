#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

Adafruit_MPU6050 mpu;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML webpage
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Gyroscope</title>
  <style>
    body {
      font-family: Arial;
      background: #111;
      color: white;
      text-align: center;
      margin-top: 50px;
    }

    .data {
      font-size: 2em;
      margin: 20px;
    }
  </style>
</head>
<body>

<h1>ESP32 Gyroscope Data</h1>

<div class="data">X: <span id="gx">0</span></div>
<div class="data">Y: <span id="gy">0</span></div>
<div class="data">Z: <span id="gz">0</span></div>

<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;

  window.addEventListener('load', onLoad);

  function onLoad() {
    initWebSocket();
  }

  function initWebSocket() {
    websocket = new WebSocket(gateway);

    websocket.onmessage = function(event) {
      let data = JSON.parse(event.data);

      document.getElementById('gx').innerHTML = data.x.toFixed(2);
      document.getElementById('gy').innerHTML = data.y.toFixed(2);
      document.getElementById('gz').innerHTML = data.z.toFixed(2);
    };
  }
</script>

</body>
</html>
)rawliteral";

void notifyClients(String data) {
  ws.textAll(data);
}

void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len) {

  if(type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin();

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }

  Serial.println("MPU6050 Found!");

  // Connect WiFi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Create JSON string
  String json = "{";
  json += "\"x\":" + String(g.gyro.x) + ",";
  json += "\"y\":" + String(g.gyro.y) + ",";
  json += "\"z\":" + String(g.gyro.z);
  json += "}";

  notifyClients(json);

  delay(50); // 20Hz update
}