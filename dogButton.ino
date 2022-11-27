#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include "secrets.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>




#define pxlPin 25
#define NUMPIXELS 12
#define ledPin 2
#define buttonPin 13

//messages
#define DOGS_NEED_TO_BE_FED "Feed the poor doggos!"
#define DOGS_WERE_FED "The dogs have been fed!\nWhat good dogs!"
#define DOGS_NEED_CHEWYS "Those poor pups! They need their chewies!"
#define DOGS_GIVEN_CHEWYS "The dogs have been given chewies"


WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

Adafruit_NeoPixel pixels(NUMPIXELS, pxlPin, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -25200;
const int   daylightOffset_sec = 3600;


void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void IRAM_ATTR button(){
  pixels.clear();
  Serial.println("button pressed");
  bot.sendMessage(CHAT_ID, "Button pressed", "");

  //sendTelegramMessage("Button pressed");
}

void setup() {
  pixels.begin();
  pinMode(ledPin, OUTPUT);
  pinMode(pxlPin, OUTPUT);
	pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(buttonPin, button, FALLING);
  Serial.begin(115200);
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  //telegram setup:
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  bot.sendMessage(CHAT_ID, "Bot started up", "");



  //disconnect WiFi as it's no longer needed
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
  // Serial.println("Wifi Disconnected");
}


void loop() {
  digitalWrite(ledPin, HIGH);
	delay(500);
	digitalWrite(ledPin, LOW);
	delay(500);
  pixels.clear();

  for(int i=0; i<NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(0, 15, 0));
    pixels.show();
    delay(DELAYVAL);
  }
  printLocalTime();
}