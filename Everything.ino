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
#include <WiFiManager.h>

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
#define redPin 25
#define greenPin 26
#define bluePin 27

// setting PWM properties
const int freq = 5000;
const int redChannel = 0;
const int blueChannel = 1;
const int greenChannel = 2;
const int resolution = 8;

// Timer for the touch capacitor
float timer = 10000;
float timerRef = 10000;
float previousMillis = 0;
bool currentlyDisplaying = true;

// Waiting for the display
bool messageReceived = false;
bool senderReceived = false;

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
    const String input = "{\"identity\":\"" + user + "\",\"password\":\""+passwordUser+"\"}";
    Serial.println(input);
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

void manageColor(String hex){
    //to RGB
    long number = (long) strtol( &hex[1], NULL, 16);
    // Serial.println(hex);
    // Serial.println(number);
    int r = number >> 16;
    int g = number >> 8 & 0xFF;
    int b = number & 0xFF;
    
    // In case we need it to be between 0 and 1
    // float red = float(r)/255;
    // float green = float(g)/255;
    // float blue = float(b)/255;

    // Serial.print("red is ");
    // Serial.println(red);
    // Serial.print("green is ");
    // Serial.println(g);
    // Serial.print("blue is ");
    // Serial.println(b);
    ledcWrite(redChannel, r);
    ledcWrite(blueChannel, b);
    ledcWrite(greenChannel, g);
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
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result
    wm.setConfigPortalTimeout(300);
    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect(accessPoint, accessPointPassword); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }

    // Connect to Wifi using the traditional way of providing SSID and Password
    // Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
    // WiFi.mode(WIFI_STA);
    // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.println("trying to connect");}
    // Serial.println("WiFi connected.");


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
    // pinMode(redPin, OUTPUT);
    // pinMode(greenPin, OUTPUT);
    //pinMode(bluePin, OUTPUT);

     // configure LED PWM functionalitites
    ledcSetup(redChannel, freq, resolution);
    ledcSetup(blueChannel, freq, resolution);
    ledcSetup(greenChannel, freq, resolution);
  
    // attach the channel to the GPIO to be controlled
    ledcAttachPin(redPin, redChannel);
    ledcAttachPin(bluePin, blueChannel);
    ledcAttachPin(greenPin, greenChannel);

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

    // Subscribe to a topic pattern and attach a callback
    mqtt.subscribe("color", [](const char * topic, const char * payload) {
        // Serial.printf(payload);
        manageColor(String(payload));
    });

    mqtt.subscribe("message", [](const char * topic, const char * payload) {
        // Serial.printf(payload);
        message(String(payload));
      });

    mqtt.subscribe("sender", [](const char * topic, const char * payload) {
        sender(String(payload));
      });

    // Start the client
    mqtt.begin();
}



// Display Message
void displaySenderAndMessage(){
    display.display();
    senderReceived = false;
    messageReceived = false;

    previousMillis=millis();
    currentlyDisplaying = true;
    timer = timerRef;
}

void message(String msg){
  display.setCursor(0,32);
  display.println(msg);
  if(senderReceived){
    displaySenderAndMessage();
  }
  else{
    messageReceived = true;
  }
}

void sender(String exp){
  display.setCursor(0,0);
  display.println(exp);
  if(messageReceived){
    displaySenderAndMessage();
  }
  else{
    senderReceived = true;
  }
}

void loop() {
  //  // increase the LED brightness
  // for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
  //   // changing the LED brightness with PWM
  //   ledcWrite(redChannel, dutyCycle);
  //   delay(15);
  // }

  // // decrease the LED brightness
  // for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
  //   // changing the LED brightness with PWM
  //   ledcWrite(redChannel, dutyCycle);   
  //   delay(15);
  // }
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
        ledcWrite(redChannel, 0);
        ledcWrite(greenChannel, 0);
        ledcWrite(blueChannel, 0);
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