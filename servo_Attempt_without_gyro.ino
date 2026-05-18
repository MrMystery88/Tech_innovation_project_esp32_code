#include <WiFi.h>              // WiFi functionality for ESP32
#include <AsyncTCP.h>          // Required for async web server
#include <ESPAsyncWebServer.h> // Web server + WebSocket library
#include <math.h>              // Used for sin() and cos()

// =========================
// WIFI SETTINGS
// =========================

// Your WiFi network name
const char* ssid = "AndroidAP";

// Your WiFi password
const char* password = "MONEY2107";

// =========================
// CREATE SERVER OBJECTS
// =========================

// Creates a web server on port 80
// Port 80 is standard HTTP
AsyncWebServer server(80);

// Creates a WebSocket endpoint
// Laptop connects to:
// ws://IP_ADDRESS/ws
AsyncWebSocket ws("/ws");

// =========================
// FAKE GYRO VARIABLES
// =========================

// Used to animate fake gyro values
float t = 0;

// =========================
// HTML WEBPAGE
// =========================

// PROGMEM stores the webpage in flash memory
// instead of using precious RAM
const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

  <title>ESP32 Gyro Test</title>

  <style>

    /* Page background + default text */
    body {
      background: #111;
      color: white;
      font-family: Arial;
      text-align: center;
      margin-top: 50px;
    }

    /* Main heading */
    h1 {
      font-size: 3em;
    }

    /* Style for sensor value sections */
    .data {
      font-size: 2em;
      margin: 20px;
    }

    /* Green numbers */
    .value {
      color: #00ff88;
    }

  </style>

</head>

<body>

<!-- Page title -->
<h1>ESP32 Fake Gyroscope</h1>

<!-- X axis value -->
<div class="data">
  X: <span class="value" id="gx">0</span>
</div>

<!-- Y axis value -->
<div class="data">
  Y: <span class="value" id="gy">0</span>
</div>

<!-- Z axis value -->
<div class="data">
  Z: <span class="value" id="gz">0</span>
</div>

<script>

// =========================
// JAVASCRIPT SECTION
// =========================

// Creates websocket address automatically
// Example:
// ws://192.168.1.45/ws
let gateway = `ws://${window.location.hostname}/ws`;

let websocket;

// When webpage fully loads
window.addEventListener('load', onLoad);

// Runs once page loads
function onLoad() {
  initWebSocket();
}

// Connect to ESP32 websocket
function initWebSocket() {

  websocket = new WebSocket(gateway);

  // Runs when connected
  websocket.onopen = () => {
    console.log("WebSocket Connected");
  };

  // Runs if disconnected
  websocket.onclose = () => {
    console.log("WebSocket Disconnected");

    // Try reconnecting after 2 seconds
    setTimeout(initWebSocket, 2000);
  };

  // Runs every time ESP32 sends data
  websocket.onmessage = (event) => {

    // Convert JSON text into usable data
    let data = JSON.parse(event.data);

    // Update webpage numbers
    document.getElementById("gx").innerHTML = data.x.toFixed(2);
    document.getElementById("gy").innerHTML = data.y.toFixed(2); 
    document.getElementById("gz").innerHTML = data.z.toFixed(2);
  };
}

</script>

</body>
</html>

)rawliteral";

// =========================
// WEBSOCKET EVENT FUNCTION
// =========================

// This runs when websocket events occur
void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len) {

  // Detect new connection
  if(type == WS_EVT_CONNECT) {

    Serial.println("Client Connected");
  }
}

// =========================
// SETUP FUNCTION
// =========================

void setup() {

  // Start serial monitor
  Serial.begin(115200);

  // =========================
  // CONNECT TO WIFI
  // =========================

  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  // Wait until connected
  while(WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");

  // Print ESP32 IP address
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // =========================
  // WEBSOCKET SETUP
  // =========================

  // Attach websocket event function
  ws.onEvent(onEvent);

  // Add websocket handler to server
  server.addHandler(&ws);

  // =========================
  // WEBPAGE ROUTE
  // =========================

  // When user opens:
  // http://ESP_IP_ADDRESS
  // send webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {

    request->send_P(200, "text/html", index_html);
  });

  // Start web server
  server.begin();

  Serial.println("Server Started");
}

// =========================
// MAIN LOOP
// =========================

void loop() {

  // =========================
  // GENERATE FAKE GYRO DATA
  // =========================

  // Creates smooth wave motion
  float gx = sin(t) * 100;

  // Cosine wave for Y
  float gy = cos(t) * 100;

  // Slower sine wave for Z
  float gz = sin(t * 0.5) * 50;

  // Increase time variable
  t += 0.1;

  // =========================
  // CREATE JSON DATA
  // =========================

  // Create JSON string manually
  String json = "{";

  json += "\"x\":" + String(gx) + ",";
  json += "\"y\":" + String(gy) + ",";
  json += "\"z\":" + String(gz);

  json += "}";

  // Example result:
  // {
  //   "x":52.3,
  //   "y":18.2,
  //   "z":7.1
  // }

  // =========================
  // SEND DATA TO LAPTOP
  // =========================

  // Send JSON to all connected webpages
  ws.textAll(json);

  // =========================
  // UPDATE SPEED
  // =========================

  // Delay controls refresh speed

  // 50ms = 20 updates/sec
  // 20ms = 50 updates/sec
  // 10ms = 100 updates/sec

  delay(20);
}