#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include "secrets.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <WiFiUdp.h>

#define pxlPin 25
#define NUMPIXELS 12
#define ledPin 2
#define buttonPin 13

//messages
#define DOGS_NEED_FED "Feed the poor doggos!"
#define DOGS_WERE_FED "The dogs have been fed!\nWhat good dogs!"
#define DOGS_NEED_CHEWYS "Those poor pups! They need their chewies!"
#define DOGS_GIVEN_CHEWYS "The dogs have been given chewies"

#define morningDeadline 10 //deadline for feeding the dogs in the morning
#define eveningDeadline 18 //deadline for feeding the dogs in the evening

#define preheat 4 //how many hours before the deadline the ring starts lighting up.

int singlepixeldelay = (5*60)/12; //preheat/12*60*60*1000;

int pixel = 0;

AlarmId timerID;

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);


WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

Adafruit_NeoPixel pixels(NUMPIXELS, pxlPin, NEO_GRB + NEO_KHZ800);


static const char ntpServerName[] = "us.pool.ntp.org";

const int timeZone = -7;     // may be work with DST

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t prevDisplay = 0; // when the digital clock was displayed


time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

bool btnPress = false;
bool buttonArmed = true;
bool buttonTriggered = false;

bool fed = false;
bool chewie = false;


void IRAM_ATTR button(){
  pixels.clear();
  Serial.println("button pressed");
  btnPress = true;
}

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime(){
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
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
  
  //telegram setup:
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  bot.sendMessage(CHAT_ID, "Bot started up", "");
  //NTP setup:
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  //Alarm.timerRepeat(150, Repeats);           // timer for every 15 seconds


  int morningAlarmTime = morningDeadline-preheat;
  Alarm.alarmRepeat(morningAlarmTime,0,0,MorningAlarm);  // Setup for the morning alarm
  Alarm.alarmRepeat(morningDeadline,0,0,MorningDeadline);  // Setup for the morning alarm  

  int eveningAlarmTime = eveningDeadline-preheat;
  Alarm.alarmRepeat(18,3,0,EveningAlarm);  // Setup for the evening alarm
  Alarm.alarmRepeat(18,8,0,EveningDeadlineCheck);  // Setup for the morning alarm


}

void MorningAlarm(){
  fed = false;
  pixels.clear();  
  

}

void MorningDeadline(){
  if(fed == 0){
    bot.sendMessage(CHAT_ID, DOGS_NEED_FED);
  }
}

void EveningAlarm() {
  Serial.println("evening alarm");
  fed = false;
  pixel = 0;
  pixels.clear();
  timerID = Alarm.timerRepeat(singlepixeldelay, PixelAdvance);
  PixelAdvance();
            
    
}

void PixelAdvance(){
  Serial.println("pixeladvance");
  pixels.setPixelColor(pixel, pixels.Color(15, 0, 0));
  pixels.show();
  pixel++;
}

void EveningDeadlineCheck(){
  if(fed == 0){
    bot.sendMessage(CHAT_ID, DOGS_NEED_FED);
  }
}


void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void loop(){
  if(btnPress == true){
    bot.sendMessage(CHAT_ID, "Button Pressed");
    btnPress = false;
    fed = true;
    for(int i=0; i<NUMPIXELS; i++) {

      pixels.setPixelColor(i, pixels.Color(0, 15, 0));
      pixels.show();
      delay(50);
    }

  }

  digitalWrite(ledPin, HIGH);
	Alarm.delay(500);
	digitalWrite(ledPin, LOW);
	Alarm.delay(500);
  //pixels.clear();
  digitalClockDisplay();

  if(fed == true){
    Alarm.disable(timerID);    
  }  //Alarm.delay(1000); // wait one second between clock display



  // for(int i=0; i<NUMPIXELS; i++) {

  //   pixels.setPixelColor(i, pixels.Color(15, 0, 0));
  //   pixels.show();
  //   delay(DELAYVAL);

  // }
}