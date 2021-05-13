String deviceName = "Lamp";
#define ID 210
#define LED_COUNT 30
double SEG_COUNT = 1;
double SPEED = -1;
String MODE = "rainbow"; //Starting State
#define RGB_ORDER GRB //5v = GRB, 12v = BRG#include <FastLED.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <math.h>

WebSocketsClient webSocket;

//210 Lamp  - 30
//211 Bar   - 61
//212 Door  - 100
//213 Cubby - 300
//214 Youtube - 67
String deviceName = "Door";
#define ID 212
#define LED_COUNT 100
double SEG_COUNT = 2.25;
double SPEED = -1;
String MODE = "rainbow"; //Starting State
#define RGB_ORDER BRG //5v = GRB, 12v = BRG

int custom_r = 0;
int custom_g = 0;
int custom_b = 0;
int WHITE_BRIGHT = 255;
#define LED_TYPE WS2812B
#define PIN_NUM 23

const char* ssid = "SSID"; //Enter SSID
const char* password = "PASSWORD"; //Enter Password
const char* IP_ADDR = "192.168.1.25"; //server address and port
int PORT = 213;


long double timeOffset = 0;
long double initalTime = 0;

String getValue(String data, char separator, int index) { //https://stackoverflow.com/a/29673158
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

double getTimeSinceStart() {
  return millis() / 1000.0;
}

void sendText(String str) {
  Serial.println("Sent message: " + str);
  webSocket.sendTXT(str);
}

void resetTimer() {
  initalTime = getTimeSinceStart();
  sendText("getTime");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected.");
      break;
    case WStype_CONNECTED:
      Serial.println("\nConnected.");
      resetTimer();
      break;
    case WStype_TEXT:
      String msg = (char*)payload;
      Serial.println("Got Message: "+msg);
      if(msg.equals("rainbow") || msg.equals("white") || msg.equals("off") || msg.equals("staticRGB")) {
        MODE = msg;
      }else if(msg.startsWith("RGB")) {
        MODE = "RGB";
        custom_r = getValue(msg, '|', 1).toInt();
        custom_g = getValue(msg, '|', 2).toInt();
        custom_b = getValue(msg, '|', 3).toInt();
      }else if(msg.startsWith("setBrightness")) {
        WHITE_BRIGHT = getValue(msg, '|', 1).toInt();
      }else if(msg.startsWith("TIME")) {
        timeOffset = getValue(msg, '|', 1).toDouble() + (getTimeSinceStart() - initalTime) - initalTime;
        Serial.println("Delay of "+String((double)(getTimeSinceStart() - initalTime))+" seconds.");
      }else if(msg.equals("resetTime")) {
        resetTimer();
      }else if(msg.startsWith("setSegments")) {
        SEG_COUNT = getValue(msg, '|', 1).toDouble();
      }else if(msg.startsWith("setSpeed")) {
        SPEED = getValue(msg, '|', 1).toDouble();
      }else if(msg.equals("getDevice")) {
        sendText("device|"+deviceName);
      }
      break;
  }
}

#define PI 3.1415926535897932384626433832795
double fmod(double x, double y);
CRGB leds[LED_COUNT];

float colorFromVal(float timer, float seg, float x) {
  return 255.0 * (1.0-cos(PI*fmod(seg*x/LED_COUNT+timer,1)))/2.0;
}

IPAddress local_IP(192, 168, 1, ID);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(1, 1, 1, 1);
IPAddress secondaryDNS(1, 0, 0, 1);
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, PIN_NUM, RGB_ORDER>(leds, LED_COUNT);

  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
      delay(1000);
  }
  Serial.println("Connected to WiFi, IP: " + WiFi.localIP().toString());

  webSocket.onEvent(webSocketEvent);
  webSocket.begin(IP_ADDR, PORT, "");
  webSocket.setReconnectInterval(1000);
}

long wifiTimer = 25;
void loop() {
  long double syncedTime = getTimeSinceStart() + timeOffset;
  if(!WiFi.isConnected()) {
    if(wifiTimer < 0) {
      Serial.println("Reconnecting to WiFi.");
      WiFi.begin(ssid, password);
      wifiTimer = 120;
    }else{
      delay(10);
      wifiTimer--;
    }
  }
  webSocket.loop();
  
  if(MODE.equals("rainbow")) {
    for(int i = 0; i < LED_COUNT; i++) {
      float value = fmod(syncedTime*SPEED/SEG_COUNT, 1.0);
      if(value < 0) {
        value++;
      }
      leds[LED_COUNT - 1 - i] = CHSV(colorFromVal(value, SEG_COUNT, i), 255, WHITE_BRIGHT);
    }
  }else if(MODE.equals("staticRGB")) {
    Serial.println((double)syncedTime);
    for(int i = 0; i < LED_COUNT; i++) {
      leds[i] = CHSV(colorFromVal(fmod(syncedTime*abs(SPEED)/3.0,1.0),0,1),255,WHITE_BRIGHT);
    }
  }else if(MODE.equals("RGB")) {
    for(int i = 0; i < LED_COUNT; i++) {
      leds[i].setRGB(custom_r, custom_g, custom_b);
    }
  }else if(MODE.equals("off")) {
    for(int i = 0; i < LED_COUNT; i++) {
      leds[i].setRGB(0, 0, 0);
    }
  }else if(MODE.equals("white")) {
    for(int i = 0; i < LED_COUNT; i++) {
      leds[i].setRGB(WHITE_BRIGHT, WHITE_BRIGHT, WHITE_BRIGHT);
    }
  }
  FastLED.show();
}
