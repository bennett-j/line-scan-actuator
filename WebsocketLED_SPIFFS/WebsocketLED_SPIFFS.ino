/*
https://github.com/me-no-dev/ESPAsyncWebServer
https://shawnhymel.com/1882/how-to-create-a-web-server-with-websockets-using-an-esp32-in-arduino/
https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiAccessPoint/WiFiAccessPoint.ino
https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples/WiFiClientEvents
https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
https://randomnerdtutorials.com/esp32-websocket-server-arduino/

VS code IntelliSense
https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino#intellisense
1. ensure Arduino: Initialize (don't forget access palette by Ctrl + Shift + P)
2. press Ctrl + Alt + I (after new inclues) to rebuild the c_cpp_properties.json and have it find the right includes
*/

// Import libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>

//=======================//
//  Constants & Globals  //
//=======================//

// Network details
const char* ssid = "ESP32";
const char* password = "5pud5";

const int HTTP_PORT = 80;
const int LED_PIN = 2;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool ledState = 0;

//=============//
//  Functions  //
//=============//

// WiFi event handler to provide notifications via Serial
void WiFiEvent(WiFiEvent_t event){
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
    
        default:
            break;
    }
}

void notifyClients() {
  ws.textAll(String(ledState));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      ledState = !ledState;
      
      // maybe this is too slow?
      digitalWrite(LED_PIN, ledState);

      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  return String();
}

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");
}

// Callback: send style sheet
void onCSSRequest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}

// Callback: send 404 if requested file does not exist
void onPageNotFound(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}

//=========//
//  SETUP  //
//=========//

void setup(){
    // Serial for debugging
    Serial.begin(115200);

    // Setup LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Make sure we can read the file system
    if( !SPIFFS.begin()){
        Serial.println("Error mounting SPIFFS");
        while(1);
    }

    // Connect WiFi event handler
    WiFi.onEvent(WiFiEvent);
    // Start access point
    WiFi.softAP(ssid, password);

    // Print our IP address
    Serial.println();
    Serial.println("AP running");
    Serial.print("My IP address: ");
    Serial.println(WiFi.softAPIP());

    // Set callbacks for HTTP requests
    // On HTTP request for root, provide index.html file
    server.on("/", HTTP_GET, onIndexRequest);
    // On HTTP request for style sheet, provide style.css
    server.on("/style.css", HTTP_GET, onCSSRequest);
    // Handle requests for pages that do not exist
    server.onNotFound(onPageNotFound);

    // Start web server
    server.begin();

    // init Websocket
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

//========//
//  LOOP  //
//========//

void loop() {
    ws.cleanupClients();
    // digitalWrite(ledPin, ledState);
}
