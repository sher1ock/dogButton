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
#define buttonPin2 12

//messages
#define DOGS_NEED_FED "Feed the poor doggos!"
#define DOGS_WERE_FED "The dogs have been fed!\nWhat good dogs!"
#define DOGS_NEED_CHEWIES "Those poor pups! They need their chewies!"
#define DOGS_GIVEN_CHEWIES "The dogs have been given chewies"

#define morningDeadline 10  //deadline for feeding the dogs in the morning
#define eveningDeadline 18  //deadline for feeding the dogs in the evening
#define chewiesDeadline 22 //deadline for chewies

#define preheat 4  //how many hours before the deadline the ring starts lighting up.

//int singlepixeldelay = preheat/12*60*60*1000;

int singlepixeldelay = (5 * 60) / 12;

int pixel = 0;
int pixel2 = 0;

AlarmId fedTimer;

AlarmId chewieTimer;

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);



WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);
const unsigned long BOT_MTBS = 1000; // mean time between scan messages
unsigned long bot_lasttime;          // last time messages' scan has been done

Adafruit_NeoPixel pixels(NUMPIXELS, pxlPin, NEO_GRB + NEO_KHZ800);


static const char ntpServerName[] = "us.pool.ntp.org";

const int timeZone = -7;  // may be working with DST, we'll find out in another 3 months...

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t prevDisplay = 0;         // when the digital clock was displayed


time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

bool btnPress = false;
bool btnPress2 = false;

bool fed = false;
bool chewie = false;


void IRAM_ATTR button() {
  pixels.clear();
  //pixels.show();
  Serial.println("button pressed");
  btnPress = true;
}
void IRAM_ATTR button2() {
  pixels.clear();
  Serial.println("button pressed");
  btnPress2 = true;
}

const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress ntpServerIP;  // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
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
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0;  // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void handleNewMessages(int numNewMessages){

  for (int i = 0; i < numNewMessages; i++)
  {

    // Inline buttons with callbacks when pressed will raise a callback_query message
    if (bot.messages[i].type == "callback_query"){
      Serial.print("Call back button pressed by: ");
      Serial.println(bot.messages[i].from_id);
      Serial.print("Data on the button: ");
      Serial.println(bot.messages[i].text);
      bot.sendMessage(bot.messages[i].from_id, bot.messages[i].text, "");
    }
    else{
      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      String from_name = bot.messages[i].from_name;
      // if (from_name == "")
      //   from_name = "Guest";

      if (text == "/options"){
        String keyboardJson = "[[{ \"text\" : \"Go to Google\", \"url\" : \"https://www.google.com\" }],[{ \"text\" : \"Send\", \"callback_data\" : \"This was sent by inline\" }]]";
        bot.sendMessageWithInlineKeyboard(chat_id, "Choose from one of the following options", "", keyboardJson);
      }

      if (text == "/fed_dogs@SmudgeandFloofbot"){
        // String welcome = "Welcome to Universal Arduino Telegram Bot library, " + from_name + ".\n";
        // welcome += "This is Inline Keyboard Markup example.\n\n";
        // welcome += "/options : returns the inline keyboard\n";

        // bot.sendMessage(chat_id, welcome, "Markdown");
        btnPress = true;
      }
    }
  }
}

void setup() {
  pixels.begin(); //neopixel setup
  pinMode(ledPin, OUTPUT);
  pinMode(pxlPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(buttonPin, button, FALLING);
  pinMode(buttonPin2, INPUT_PULLUP);
  attachInterrupt(buttonPin2, button, FALLING);
  Serial.begin(115200);
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");    
    digitalWrite(ledPin, HIGH);
    Alarm.delay(500);
    digitalWrite(ledPin, LOW);
  }
  Serial.println(" CONNECTED");
  //telegram setup:
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  bot.sendMessage(CHAT_ID, "Bot started up", "");
  //NTP setup:
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  int morningAlarmTime = morningDeadline - preheat;
  Alarm.alarmRepeat(/*morningAlarmTime*/17, 0, 0, MorningAlarm);    // Setup for the morning alarm
  Alarm.alarmRepeat(/*morningDeadline*/17, 5, 0, MorningDeadline);  

  int eveningAlarmTime = eveningDeadline - preheat;
  Alarm.alarmRepeat(/*eveningAlarmTime*/17, 10, 0, EveningAlarm);           // Setup for the evening alarm
  Alarm.alarmRepeat(/*eveningDeadline*/17, 15, 0, EveningDeadline);

  int chewiesAlarmTime = chewiesDeadline - preheat;
  Alarm.alarmRepeat(/*chewiesAlarmTime*/17, 20, 0, ChewiesAlarm);           // Setup for the chewies alarm
  Alarm.alarmRepeat(/*chewiesDeadline*/17, 2, 0, ChewiesDeadline);

  pixels.clear();
  pixels.show();

}

void MorningAlarm() {
  Serial.println("morning alarm");
  fed = false;
  pixel = 0;
  pixels.clear();
  fedTimer = Alarm.timerRepeat(singlepixeldelay, PixelAdvance);
  PixelAdvance();
}

void MorningDeadline() {
  if (fed == 0) {
    bot.sendMessage(CHAT_ID, DOGS_NEED_FED);
  }
}

void EveningAlarm() {
  Serial.println("evening alarm");
  fed = false;
  pixel = 0;
  pixels.clear();
  fedTimer = Alarm.timerRepeat(singlepixeldelay, PixelAdvance);
  PixelAdvance();
}

void EveningDeadline() {
  if (fed == 0) {
    bot.sendMessage(CHAT_ID, DOGS_NEED_FED);
  }
}

void ChewiesAlarm() {
  Serial.println("evening alarm");
  chewie = false;
  pixel = 0;
  pixels.clear();
  chewieTimer = Alarm.timerRepeat(singlepixeldelay, PixelAdvance2);
  PixelAdvance();
}

void ChewiesDeadline() {
  if (chewie == 0) {
    bot.sendMessage(CHAT_ID, DOGS_NEED_CHEWIES);
  }
}



void PixelAdvance() {
  Serial.println("pixeladvance");
  if(pixel<12){
  pixels.setPixelColor(pixel, pixels.Color(15, 0, 0));
  pixels.show();
  pixel++;
  }
}

void PixelAdvance2() {
  Serial.println("pixeladvance");
  if(pixel2<12){
  pixels.setPixelColor(pixel2+12, pixels.Color(15, 0, 0));
  pixels.show();
  pixel++;
  }
}

void digitalClockDisplay() {
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

void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void loop() {
  if (btnPress == true) {
    bot.sendMessage(CHAT_ID, DOGS_WERE_FED);
    btnPress = false;
    fed = true;
    for (int i = 0; i < NUMPIXELS; i++) {

      pixels.setPixelColor(i, pixels.Color(0, 5, 0));
      pixels.show();
      delay(5);
    }
  }
  if (btnPress2 == true) {
    bot.sendMessage(CHAT_ID, DOGS_GIVEN_CHEWIES);
    btnPress2 = false;
    chewie = true;
    for (int i = 12; i < NUMPIXELS*2; i++) {

      pixels.setPixelColor(i, pixels.Color(0, 5, 0));
      pixels.show();
      delay(25);
    }
  }
    if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);
    Alarm.delay(500);
    digitalWrite(ledPin, LOW);
    Alarm.delay(500);
    }  //pixels.clear();
  digitalClockDisplay();
  Alarm.delay(250);

  if (fed == true) {
    Alarm.free(fedTimer);
  }
  if (millis() - bot_lasttime > BOT_MTBS){
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages){
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

}