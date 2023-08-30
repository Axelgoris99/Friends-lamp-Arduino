/*===============================================================================
|| YOU MUST CHANGE THE ESP32 CLIENT ID FOR EACH ESP32 - ELSE THEY DON'T WORK
||
||=============================AXEL GORIS========================================
*/
#include <Arduino.h>
#include <HTTPClient.h>
#include <PicoMQTT.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "credentials.h"

// Screen Declaration
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET -1

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Touch Capacitor
#define touchPin 34

// Led
#define redPin 18
#define greenPin 5
#define bluePin 19


// Timer for the touch capacitor
float timer = 10000;
float timerRef = 10000;
float previousMillis = 0;
bool currentlyDisplaying = true;


// CHANGE IT FOR EVERY ESP32
// MQTT Client to sub/pub
PicoMQTT::Client mqtt;

// Serial reading
int incomingByte = 0; // for incoming serial data

// http request to the server to get datas
String httpRequest(){
    String payload = "{}";     
    HTTPClient http;
    // If you need Node-RED/server authentication, insert user and password below
    http.begin(authPath.c_str());
    http.addHeader("Content-Type", "application/json");
    const String input = "{\"identity\":\"" + user + "\",\"password\":\""+password+"\"}";

    JSONVar myObject = JSON.parse(input);
    int test = http.POST(input);
    if(test>0){
      // Serial.println(test);
      payload = http.getString();
      //Serial.println(payload);
    }
    else{
      Serial.println("Error Code : ");
      Serial.println(test);
    }
    http.end();
    return payload;
}

void manageColor(){
    //to RGB
    // long int rgb=atol(hex+1);
    // char r=(char)(rgb>>16);
    // char g=(char)(r>>8);
    // char b=(char)rgb;
    // Serial.println(hex);
    // Serial.println(r);
    // Serial.println(g);
    // Serial.println(b);

    digitalWrite(redPin, HIGH); // turn the LED on
}

// publish color and message after fetching them from the db
void publishColorAndMessage(){
      JSONVar object = JSON.parse(httpRequest());
      //Serial.println(object);
      //Serial.println(object["record"]["message"]);
      //Serial.println(JSON.typeof(object["record"]["color"]));
      Serial.println(object);
      String color = object["record"]["color"];
      mqtt.publish("color", color);
      String message = object["record"]["message"];
      mqtt.publish("message", message);
      mqtt.publish("sender", user);
}

// Setup everything
void setup() {
    // mqtt auth
    mqtt.host = mqttHost;
    mqtt.port = mqttPort;
    mqtt.client_id = clientId;
    mqtt.username = user;
    mqtt.password = password;

    // Timer reset
    timer = timerRef;

    // Usual setup
    Serial.begin(115200);

    // Touch Capacitor Pin
    pinMode(touchPin, INPUT);

    // Led Pins
    // initialize digital pin GIOP18 as an output.
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);

    // Display
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
      }
      delay(2000);
      display.clearDisplay();

      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      // Display static text
      display.println("Bon anniversaire!");
      display.display(); 
      //delay(1000);

    // Connect to Wifi
    Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.println("trying to connect");}
    Serial.println("WiFi connected.");

    // Subscribe to a topic pattern and attach a callback
    mqtt.subscribe("color", [](const char * topic, const char * payload) {
        // Serial.printf(payload);
        manageColor();
    });

    mqtt.subscribe("message", [](const char * topic, const char * payload) {
        // Serial.printf(payload);
        previousMillis=millis();
        message(String(payload));
        currentlyDisplaying = true;
        timer = timerRef;
      });

    mqtt.subscribe("sender", [](const char * topic, const char * payload) {
        sender(String(payload));
      });

    // Start the client
    mqtt.begin();
}

// Display Message
void message(String msg){
  display.clearDisplay();
  display.setCursor(0,32);
  display.println(msg);
  display.display();
}

void sender(String exp){
  display.setCursor(0,0);
  display.println(exp);
  display.display();
}

void loop() {
    // Serial.println(millis());
    if(timer >= 0){
      timer = timer - (millis()-previousMillis);
      previousMillis = millis();
      // Serial.println(timer);
    }
    else{
      if(currentlyDisplaying == true){
        Serial.println("clear");
        display.clearDisplay();
        display.display();
        currentlyDisplaying = false;
        digitalWrite(18, LOW);
      }
    }
    // This will automatically reconnect the client if needed.  Re-subscribing to topics is never required.
    mqtt.loop();

    // Touch Capacitor read value and react to it
    int touchValue = digitalRead(touchPin);
    if (touchValue == HIGH) {
      if(currentlyDisplaying == false){
        Serial.println("TOUCHED");
        publishColorAndMessage();
      }
      else{
        timer = timerRef;
      }
    }
    
    // control and logz through serial monitor
    if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();
    if(incomingByte=48){
      // Serial.println("0");
      publishColorAndMessage();
    }
  }
}