/*
https://github.com/bennett-j/stupid-linear-actuator/blob/main/software/stupid-linear-actuator-firmware/stupid-linear-actuator-firmware.ino
https://github.com/me-no-dev/ESPAsyncWebServer
https://shawnhymel.com/1882/how-to-create-a-web-server-with-websockets-using-an-esp32-in-arduino/
https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiAccessPoint/WiFiAccessPoint.ino
https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples/WiFiClientEvents
https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
https://randomnerdtutorials.com/esp32-websocket-server-arduino/
https://github.com/teemuatlut/TMCStepper/blob/master/examples/TMC_AccelStepper/TMC_AccelStepper.ino

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
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <TMCStepper.h> //TODO: implement this functionality

//=======================//
//  Constants & Globals  //
//=======================//

// Network details
const char *ssid = "ESP32_AccessPoint";
const char *password = "letMe1nPlz";

const int HTTP_PORT = 80;

enum Status
{
    DISABLE,
    HOMING_OUT,
    HOMING_IN,
    IDLE,
    MOVING
};

Status status = DISABLE;

// TODO: pin assignment
const int LED_PIN = 2;
const int STEP_PIN = 18;
const int DIR_PIN = 5;
const int ENABLE_PIN = 23;
const int HOME_LIM_PIN = 33;
const int IDLE_LIM_PIN = 32;

const float steps_per_mm = 13.34;

int mm2step(int mm)
{
    return mm * steps_per_mm; // truncation
}

int step2mm(int step)
{
    return step / steps_per_mm; // TODO: deal with rounding
}

const int ACCEL = mm2step(100); // I think I can use this function here
const int HOME_SPEED = mm2step(60);
const int TRAVEL_SPEED = mm2step(250);

int velocity = 40;
int start_pos = 0;
int end_pos = 1000;

int maxSteps = 0;

long lastReportTime = millis();

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);

//=============//
//  Functions  //
//=============//

void sendReport()
{
    DynamicJsonDocument doc(1024);
    // TODO: optimise document size

    doc["type"] = "report";
    doc["status"] = getStatusText();
    doc["m_vel"] = velocity;
    doc["m_start"] = start_pos;
    doc["m_stop"] = end_pos;
    doc["m_pos"] = step2mm(stepper.currentPosition());

    String output;
    serializeJson(doc, output);
    Serial.println(output);

    ws.textAll(output);
}

void sendSerialWeb(const char* text)
{
    DynamicJsonDocument doc(1024);

    doc["type"] = "serial";
    doc["text"] = text;
    
    String output;
    serializeJson(doc, output);
    Serial.println(output);

    ws.textAll(output);
}

#define DEBUG // comment out to remove debugging
#ifdef DEBUG
    #define DEBUG_PRINT(x) Serial.println(x); delay(200); sendSerialWeb(x)
#else
    #define DEBUG_PRINT(x)
#endif

const char* getStatusText() {
    switch (status)
    {
    case DISABLE: return "Stepper Disabled";
    case IDLE: return "Idle";
    case HOMING_IN: return "Homing in";
    case HOMING_OUT: return "Homing out";
    case MOVING: return "Moving";
    default: return "Unknown";
    }
}

// Send stepper to end position then home position
void startHomeStepper()
{
    DEBUG_PRINT("Starting Homing");

    status = HOMING_OUT;
    // before first homing, stepper is disabled
    //stepper.enableOutputs(); not working
    digitalWrite(ENABLE_PIN, LOW);

    stepper.setMaxSpeed(HOME_SPEED);
    // start move plenty of steps to idle end
    stepper.move(mm2step(2 * 1200));
}

// WiFi event handler to provide notifications via Serial
void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event)
    {
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



void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {

        //  >data< is the pointer
        // >*data< dereferences to yield the value

        // https://arduinojson.org/
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, (char *)data);
        // nb (char*)data casts it to datatype char (not uint8_t)

        if (strcmp(doc["type"], "settings") == 0)
        {
            DEBUG_PRINT("Incoming settings message");
            DEBUG_PRINT((char *)data);

            velocity = doc["velocity"];
            start_pos = doc["start_pos"];
            end_pos = doc["end_pos"];
        }
        else if (strcmp(doc["type"], "button") == 0)
        {
            DEBUG_PRINT("Incoming button message");

            /* TODO: check which actions are permitted. 
             * Definitely need some validation on whether start and stop are within bounds
             */
            if (strcmp(doc["action"], "stp") == 0)
            {
                DEBUG_PRINT("Action: STOP");
                stepper.stop();
            }
            else if (strcmp(doc["action"], "home") == 0)
            {
                DEBUG_PRINT("Action: HOME");
                startHomeStepper();
            }
            else if (strcmp(doc["action"], "goHome") == 0)
            {
                DEBUG_PRINT("Action: GO HOME");
                stepper.setMaxSpeed(TRAVEL_SPEED);
                stepper.moveTo(0);
            }
            else if (strcmp(doc["action"], "goStart") == 0)
            {
                DEBUG_PRINT("Action: GOSTART");
                stepper.setMaxSpeed(TRAVEL_SPEED);
                stepper.moveTo(mm2step(start_pos));
            }
            else if (strcmp(doc["action"], "start") == 0)
            {
                DEBUG_PRINT("Action: START");
                stepper.setMaxSpeed(mm2step(velocity));
                stepper.moveTo(mm2step(end_pos));
            }
        }
        sendReport();
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        sendReport();
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

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest *request)
{
    IPAddress remote_ip = request->client()->remoteIP();
    Serial.println("[" + remote_ip.toString() +
                   "] HTTP GET request of " + request->url());
    request->send(SPIFFS, "/index.html", "text/html");
}

// Callback: send style sheet
void onCSSRequest(AsyncWebServerRequest *request)
{
    IPAddress remote_ip = request->client()->remoteIP();
    Serial.println("[" + remote_ip.toString() +
                   "] HTTP GET request of " + request->url());
    request->send(SPIFFS, "/style.css", "text/css");
}

// Callback: send javascript
void onJSRequest(AsyncWebServerRequest *request)
{
    IPAddress remote_ip = request->client()->remoteIP();
    Serial.println("[" + remote_ip.toString() +
                   "] HTTP GET request of " + request->url());
    request->send(SPIFFS, "/script.js", "text/js");
}

// Callback: send 404 if requested file does not exist
void onPageNotFound(AsyncWebServerRequest *request)
{
    IPAddress remote_ip = request->client()->remoteIP();
    Serial.println("[" + remote_ip.toString() +
                   "] HTTP GET request of " + request->url());
    request->send(404, "text/plain", "Not found");
}

//=========//
//  SETUP  //
//=========//

void setup()
{
    // Serial for debugging
    Serial.begin(115200);

    // Setup LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Make sure we can read the file system
    if (!SPIFFS.begin())
    {
        Serial.println("Error mounting SPIFFS");
        while (1)
            ;
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
    // On HTTP request for javascript, provide script.js
    server.on("/script.js", HTTP_GET, onJSRequest);
    // Handle requests for pages that do not exist
    server.onNotFound(onPageNotFound);

    // Start web server
    server.begin();

    // init Websocket
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // setup stepper
    pinMode(HOME_LIM_PIN, INPUT_PULLUP);
    pinMode(IDLE_LIM_PIN, INPUT_PULLUP);

    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH); //to disable stepper
    //stepper.setEnablePin(ENABLE_PIN); not working
    stepper.setAcceleration(mm2step(ACCEL));
    // wait to set speed and enable outputs in homing

    DEBUG_PRINT("Setup complete.");
}

//========//
//  LOOP  //
//========//

void loop()
{
    ws.cleanupClients();

    switch (status)
    {
    case HOMING_OUT:
        if (digitalRead(IDLE_LIM_PIN) == HIGH)
        {
            DEBUG_PRINT("REACHED IDLE END");
            // has reached idle end
            // temporarily save current pos but not right because we don't know start
            maxSteps = stepper.currentPosition();
            // start move plenty of steps to home end
            stepper.move(mm2step(2 * -1200));
            status = HOMING_IN;
            sendReport();
            //TODO: backoff
        }
        //TODO: else if reaches home pin first then flip direction
        else
        {
            stepper.run();
        }
        break;

    case HOMING_IN:
        if (digitalRead(HOME_LIM_PIN) == HIGH)
        {
            stepper.stop();
            // has reached home end
            DEBUG_PRINT("REACHED HOME END");
            
            stepper.move(50); // move steps away from limit switch
            while(stepper.run()){} //what is the blocking function for a relative move?!
            DEBUG_PRINT("moved away");

            // set current position to 0 and save max position
            int tmpMax = maxSteps;
            int nowPos = stepper.currentPosition();
            maxSteps = tmpMax - nowPos;
            stepper.setCurrentPosition(0);

            // DEBUG_PRINT("Max Steps (mm):");
            // DEBUG_PRINT((char*)step2mm(maxSteps)); //this line was breaking some stuff - execution was getting stuck here
            DEBUG_PRINT("Homed");

            // now homed, set status to IDLE
            status = IDLE;
            sendReport();
        }
        else
        {
            stepper.run();
        }
        break;
    
    case IDLE: // no break

        if ((millis() - lastReportTime) > 2000) {
            sendReport();
            lastReportTime = millis();
        }

        //tmp
        status = stepper.run() ? MOVING : IDLE;
        break;

    case MOVING:

        // if the stepper hits a pin (it shouldn't but just in case) then stop the motor
        if (digitalRead(IDLE_LIM_PIN) || digitalRead(HOME_LIM_PIN)) {
            stepper.stop();
        }

        // this is simple - perhaps overly simple (and inefficient) and may evolve with project
        status = stepper.run() ? MOVING : IDLE;
        // TODO: check limit pins
        break;

    default:
        break;
    }
}
