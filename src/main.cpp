#include <Arduino.h>
#include <DFRobot_LIDAR07.h>
//#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <sstream>
#include <iostream>
#include <queue>

// create this file and fill your wifi credentials
#include <wifi/credentials.h>

//to fix compile error apply this: https://github.com/esphome/AsyncTCP/pull/7/commits/f7882c3014bcb64a7188b4a0c94c566db80d2abd

int distance = 0;

bool event = true;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create websocket
AsyncWebSocket ws("/ws");

AsyncEventSource measurements("/measurements");

long msgTimeout = 0;
long cleanupTimeout = 0;

std::queue<String> msgq;


/**
 * @brief Defines needed for correct lidar init
 * 
 */
//If using IIC mode, please enable macro USE_IIC
DFRobot_LIDAR07_IIC  LIDAR07;

/**
 * @brief Do all initialization for Lidar
 * 
 */
void initLidar() {
  uint32_t version;
  while(!LIDAR07.begin()){
    Serial.println("The sensor returned data validation error");
    delay(1000);
  }

  version = LIDAR07.getVersion();
  Serial.print("VERSION: ");
  Serial.print((version>>24)&0xFF,HEX);
  Serial.print(".");Serial.print((version>>16)&0xFF,HEX);
  Serial.print(".");Serial.print((version>>8)&0xFF,HEX);
  Serial.print(".");Serial.println((version)&0xFF,HEX);

  //After enabling the filter, it can be stopped by calling LIDAR07.stopFilter()
  while(!LIDAR07.startFilter());

  /**
   * @brief  Configure the sensor to single acquisition mode
   * @param  mode The way data are collected
   * @n      eLidar07Single  A single collection
   * @n      eLidar07Continuous  Continuous acquisition
   * @return true (Successful) ， false (Failed)
   */
  while(!LIDAR07.setMeasureMode(LIDAR07.eLidar07Single));

  /**
   * @brief  Set specific measurement interval period
   * @param  frqe This parameter is valid only for continuous measurement, and the measurement period is set in ms. Minimum setting is 10MS (i.e. 100Hz)
   * @return true (Successful) ， false (Failed)
   */
  while(!LIDAR07.setConMeasureFreq(100));
  
  //Open measurement (in single measurement mode, it will automatically close after sampling).To stop collection, use stopMeasure()
  //LIDAR07.startMeasure();
}


/**
 * @brief filter out extreme values
 * 
 * @param newval 
 * @return int 
 */
int lidarFilter(int newval) {
  //lidar should only work up intil 12m
  if(newval<12000) {
    //distance = (3*distance+newval)/4;
    distance = newval;
  }
  return distance;
}

/**
 * @brief notify all listening clients
 *
 */
void notifyClients(String s)
{
  ws.textAll(s);
}

/**
 * @brief handle incoming websocket messages
 *
 * @param arg
 * @param data
 * @param len
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle") == 0)
    {
      //If we want to listen to incoming messages: parse and fill in logic here.
      //Currently not used.
      event = true;
      //notifyClients();
    }
  }
}

/**
 * @brief Listen to incoming websocket messages
 *
 * @param server
 * @param client
 * @param type
 * @param arg
 * @param data
 * @param len
 */
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
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

/**
 * @brief Initialize websocket and define the handler methods
 *
 */
void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void initEventSource() {
  Serial.println("initEventSource");
  measurements.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it gat is: %u\n", client->lastId());
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    //client->send("hello!",NULL,millis(),1000);
  });
  //HTTP Basic authentication
  //measurements.setAuthentication("user", "pass");
  server.addHandler(&measurements);
}

/**
 * @brief Group everything that is webrelated and needs to be in setup()
 *
 */
void webSetup()
{

  pinMode(LED_BUILTIN, OUTPUT);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html"); });
  // Route to load style.css file
  //server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
  //          { request->send(SPIFFS, "/style.css", "text/css"); });

  // Route to load script.js file
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/script.js", "application/javascript"); });

  server.begin();
}




void returnJSONDistance(int dist) {
  DynamicJsonDocument doc(24);

  doc["distance"] = dist;
  String res = "";
  serializeJson(doc, res);
  msgq.push(res);
  // notifyClients(res);
}

void returnEvent(int dist) {
  char str[8];
  itoa(dist,str,10);
  measurements.send(str,"distance",millis());
}

/**
 * @brief Testmethod to debug UI stuff. can generate artifical CAN JSON objects
 *
 */
void testJSON()
{
  returnJSONDistance(random(0, 5000));
}



void measureDistance() {
    LIDAR07.startMeasure();
  //Get the collected data
  if(LIDAR07.getValue()){
    //Serial.print("Distance:");Serial.print(LIDAR07.getDistanceMM());Serial.println(" mm");
    //Serial.print("Amplitude:");Serial.println(LIDAR07.getSignalAmplitude());
    //Serial.println(lidarFilter(LIDAR07.getDistanceMM()));
    //Serial.println(LIDAR07.getDistanceMM());
    lidarFilter(LIDAR07.getDistanceMM());
  }
  //returnJSONDistance(distance);
  returnEvent(distance);
}


void setup() {
  Serial.begin(115200);
  initLidar();
  webSetup();
  initEventSource();
}

void loop() {

  //Occasionally clean up old websocket clients to free up resources
  long t = millis();
  if (t > cleanupTimeout)
  {
    cleanupTimeout = t + 1000;
    ws.cleanupClients();
    //testJSON(); //hitch along on this method to create test messages when uncommented
  }

  if (event)
  {
    event = false;
    //Action to take on incoming message
  }

  //We can't send everymessage immediately through the websocket as the rate is too high
  //every half second we group everything that was in the queue and send them as a combined
  //JSON.
  

  delay(25);
  //testJSON();
  measureDistance();
}

