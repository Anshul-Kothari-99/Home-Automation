// GPIO5-D1, GPIO4-D2, GPIO14-D5, GPIO12-D6, GPIO13-D7, GPIO15-D8
// Worked perfectly for 3 devices... Tested on 13-12-20 at 1:08AM
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266Ping.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <NTPClient.h>
#include<String.h>
#include <EEPROM.h>
#include <Espalexa.h>

#define Relay0            D1  //GPIO5 --> SWITCH 1
#define Relay1            D2  //GPIO4 --> SWITCH 2
#define Relay2            D5  //GPIO14 --> SWITCH 3
#define Relay3            D6  //GPIO12 --> SWITCH 4
#define Relay4            D7  //GPIO13 --> SWITCH 5
#define Relay5            D8  //GPIO15 --> SWITCH 6

//callback functions
void firstLightChanged(uint8_t brightness);
void secondLightChanged(uint8_t brightness);
void thirdLightChanged(uint8_t brightness);
void fourthLightChanged(uint8_t brightness);
void fifthLightChanged(uint8_t brightness);
void sixthLightChanged(uint8_t brightness);

// device names
String Device_1_Name = "Device one";
String Device_2_Name = "Device two";
String Device_3_Name = "BULB";
String Device_4_Name = "Device four";
String Device_5_Name = "FAN";
String Device_6_Name = "Device six";

String WIFI_SSID = "Dhruv K";           // Your SSID
String WIFI_PASS = "1234567890";      // Your password

/******************************************************* Adafruit.io Setup *************************************************************************/
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "Anshul_k"            // Replace it with your username
#define AIO_KEY         "aio_VjOD41G8l9BtwfwpWd38LILio3GJ"   // Replace with your Project Auth Key

/************************************** Global State (you don't need to change this!) **************************************************************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
// WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/************************************************************ Feeds *********************************************************************************/

// Setup a feed called 'HomeAutomation' for subscribing to changes.
Adafruit_MQTT_Subscribe HomeAutomation = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME"/feeds/lr"); // FeedName

//Week Days
String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void MQTT_connect();
int c;

String temp;//for scheduling use
int stat = 0; //(1 if no pending schedules and 0 if pending schedules)

//Setting Up for fetching RTC
WiFiUDP ntpUDP; // FOR FETCHING REAL TIME
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); //For India It is offset to 19800 (3600*5.5)


uint addr = 0;

// fake data
struct {
  int check_bit = 0; // 1-if wifi credentials are updated, 2-if sceduling data is updated, 3-if both are updated
  int sch_stat = 0;
  String id = "";
  String pass = "";
  String sch = "";
} eeprom_data;

Espalexa espalexa;

void setup() {
  Serial.begin(115200);

  pinMode(Relay0, OUTPUT);
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  pinMode(Relay4, OUTPUT);
  pinMode(Relay5, OUTPUT);

  pinMode(D4, OUTPUT); //remember it works on acttive low logic
  digitalWrite(D4, HIGH);
  EEPROM.begin(512);

  EEPROM.get(addr, eeprom_data);
  Serial.println("Data Received From EEPROM is --> ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass) + " , Check_Bit: " + String(eeprom_data.check_bit) + " , sch data: " + String(eeprom_data.sch));
  if (eeprom_data.check_bit == 1 || eeprom_data.check_bit == 3)
  {
    Serial.println("Wifi credentials updated successfully as ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass));
    WIFI_SSID = eeprom_data.id;
    WIFI_PASS = eeprom_data.pass;
  }
  EEPROM.get(addr, eeprom_data);
  if (eeprom_data.check_bit == 2 || eeprom_data.check_bit == 3)
  {
    stat = 1;
    temp = eeprom_data.sch;
    Serial.println("updated Temp DATA successfully as: " + String(temp));
  }

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(D4, LOW);
  Serial.println();

  // Define your devices here.
  espalexa.addDevice(Device_1_Name, firstLightChanged); //simplest definition, default state off
  espalexa.addDevice(Device_2_Name, secondLightChanged);
  espalexa.addDevice(Device_3_Name, thirdLightChanged);
  espalexa.addDevice(Device_4_Name, fourthLightChanged);
  espalexa.addDevice(Device_5_Name, fifthLightChanged);
  espalexa.addDevice(Device_6_Name, sixthLightChanged);
  espalexa.begin();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup MQTT subscription for HomeAutomation feed.
  mqtt.subscribe(&HomeAutomation);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  /*
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  */

  timeClient.begin(); //For RTC

}


void loop() {
  espalexa.loop();
  MQTT_connect();
  ArduinoOTA.handle(); // For OTA code communication
  schedulein(temp);
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(2000))) {
    if (subscription == &HomeAutomation) {
      Serial.print(F("Got: "));
      char* s = (char *)HomeAutomation.lastread;
      Serial.println(s);
      //int HomeAutomation_State = atoi((char *)HomeAutomation.lastread);
      if (s[0] == 'r')
        ESP.restart();

      if (s[0] == 'x')
      {
        stat = 1;
        temp = s;

        if (eeprom_data.check_bit == 1 || eeprom_data.check_bit == 3)
        {
          Serial.println("check bit updated to 3");
          eeprom_data.check_bit = 3;
        }
        else
        {
          Serial.println("check bit updated to 2");
          eeprom_data.check_bit = 2;   ///this check bit is not logically correct.....go through this again
        }
        eeprom_data.id = eeprom_data.id;
        eeprom_data.pass = eeprom_data.pass;
        eeprom_data.sch = temp;
        eeprom_data.sch_stat = 1;
        EEPROM.put(addr, eeprom_data);
        EEPROM.commit();
        delay(1000);
        Serial.println("EEPROM COMMIT Done for updating Scheduling data.....lets recheck if its updated on EEPROM");
        EEPROM.get(addr, eeprom_data);
        Serial.println("Data Received From EEPROM is --> Sch_Data: " + String(eeprom_data.sch) + " , Check_Bit: " + String(eeprom_data.check_bit));
      }
      if (s[0] == 'w')  //"wifiCredentialsReset new_login_id new_password"
      { digitalWrite(D4, HIGH);
        WIFI_SSID = getValue(s, ' ', 1);
        WIFI_PASS = getValue(s, ' ', 2);

        if (eeprom_data.check_bit == 2 || eeprom_data.check_bit == 3)
        {
          Serial.println("Updating the check bit to 3");
          eeprom_data.check_bit = 3;
        }
        else
        {
          Serial.println("Updating the check bit to 1");
          eeprom_data.check_bit = 1;   ///this check bit is not logically correct.....go through this again
        }
        eeprom_data.id = getValue(s, ' ', 1);
        eeprom_data.pass = getValue(s, ' ', 2);
        eeprom_data.sch = eeprom_data.sch;
        eeprom_data.sch_stat = eeprom_data.sch_stat;
        EEPROM.put(addr, eeprom_data);
        EEPROM.commit();
        delay(100);
        Serial.println("EEPROM COMMIT Done for WIFI Credentials.....lets recheck if its updated on EEPROM");
        EEPROM.get(addr, eeprom_data);
        Serial.println("Data Received From EEPROM is --> ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass) + " , Check_Bit: " + String(eeprom_data.check_bit));

        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
        }
        digitalWrite(D4, LOW);
        Serial.println("Connected to: " + String(WIFI_SSID));

      }
      if (s[0] == '1')
      {
        digitalWrite(Relay0, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[0] == '0')
      {
        digitalWrite(Relay0, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }

      if (s[1] == '1')
      {
        digitalWrite(Relay1, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[1] == '0')
      {
        digitalWrite(Relay1, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }
      if (s[2] == '1')
      {
        digitalWrite(Relay2, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[2] == '0')
      {
        digitalWrite(Relay2, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }

      if (s[3] == '1')
      {
        digitalWrite(Relay3, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[3] == '0')
      {
        digitalWrite(Relay3, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }

      if (s[4] == '1')
      {
        digitalWrite(Relay4, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[4] == '0')
      {
        digitalWrite(Relay4, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }

      if (s[5] == '1')
      {
        digitalWrite(Relay5, HIGH);
        c = 0;
        Serial.println("Switching Relay HIGH");
      }
      if (s[5] == '0')
      {
        digitalWrite(Relay5, LOW);
        c = 0;
        Serial.println("Switching Relay LOW");
      }
    }
  }
}

IPAddress ip (1, 1, 1, 1); // The remote ip to ping cloudfare.....to ping google use - 8.8.8.8
int8_t ret;

void MQTT_connect() {
  // Stop if already connected.
  if (mqtt.connected()) {
    digitalWrite(D4, LOW);
    if (c >= 20) ///////////      1=2.7 secs -- multiply and set accordingly...after this much time it checks whether it is also connected to internet or not by sending ping to server
    {
      if (Ping.ping(ip))
        c = 0;
      else
        goto notconnected;
      Serial.println("connected to internet");
      digitalWrite(D4, LOW);
      return;
    }
    c++;
    return;
  }

notconnected:
  digitalWrite(D4, HIGH);
  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(4000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  digitalWrite(D4, LOW);
  Serial.println("MQTT Connected!");
}

unsigned long epochTime;
String formattedTime;
int currentHour, currentMinute, currentSecond, monthDay, currentMonth, currentYear;
String weekDay;
struct tm *ptm; //Get a time structure
String currentMonthName;
String currentDate;     //Print complete date:

void schedulein(String para)
{
  if (stat <= 0)
    void exit();   //exit function
  else
  {
    timeClient.update();
    epochTime = timeClient.getEpochTime();
    //  formattedTime = timeClient.getFormattedTime();
    //currentHour = timeClient.getHours();
    //currentMinute = timeClient.getMinutes();
    //currentSecond = timeClient.getSeconds();
    weekDay = weekDays[timeClient.getDay()];
    tm *ptm = gmtime ((time_t *)&epochTime); //Get a time structure
    //monthDay = ptm->tm_mday;
    //currentMonth = ptm->tm_mon + 1;
    // currentMonthName = months[currentMonth - 1];
    //currentYear = ptm->tm_year - 100;
    // currentDate = String(monthDay) + "-" + String(currentMonth) + "-" + String(currentYear);     //Print complete date:
    /*
        comp[0] = ptm->tm_mday; //date
        comp[1] = ptm->tm_mon + 1; //month number
        comp[2] = ptm->tm_year - 100; //year number
        comp[3] = timeClient.getHours(); //hour in 24 hour format (0-23)
        comp[4] = timeClient.getMinutes(); //minutes (0-59)
        comp[5] = timeClient.getDay(); // (0->Sunday to 6->Saturday)
    */

    //mode - 1 - enters a repeating schedule as per week days --> "xxxxxxxxxxx GPIOno. 1/0 0 7_letter_no.(for weekend use 1-->Sunday to 7-->Saturday else use 9) yy hour min epoch_time"
    //mode - 2 - enters date - format should be --> "xxxxxxxxxxx GPIOno. 1/0 dd mm yy hour min epoch_time"

    int tc = getValue(para, ' ', 4).toInt(), wd = timeClient.getDay() + 1;
    if (getValue(para, ' ', 5).toInt() == (ptm->tm_year - 100))
    {
      if (getValue(para, ' ', 3).toInt() == 0 && ((tc % 10 == wd) || ((tc / 10) % 10 == wd) || ((tc / 100) % 10 == wd) || ((tc / 1000) % 10 == wd) || ((tc / 10000) % 10 == wd) || ((tc / 100000) % 10 == wd) || ((tc / 1000000) % 10 == wd)) && getValue(para, ' ', 6).toInt() == timeClient.getHours() && getValue(para, ' ', 7).toInt() == timeClient.getMinutes())
      {
        if (getValue(para, ' ', 2).toInt() == 0)
        {
          digitalWrite(getValue(para, ' ', 1).toInt(), LOW);
          Serial.println("Switched off for repetition");
          c = 0;
          stat--;
        }
        else if (getValue(para, ' ', 2).toInt() == 1)
        {
          digitalWrite((getValue(para, ' ', 1)).toInt(), HIGH);
          Serial.println("Switched ON for repetition");
          c = 0;
          stat--;
        }
      }
      else if (getValue(para, ' ', 4).toInt() == (ptm->tm_mon + 1) && getValue(para, ' ', 3).toInt() == ptm->tm_mday && getValue(para, ' ', 6).toInt() == timeClient.getHours() && getValue(para, ' ', 7).toInt() == timeClient.getMinutes()) // dd!=0 (user entered specific date)
      {
        if (getValue(para, ' ', 2).toInt() == 0)
        {
          digitalWrite((getValue(para, ' ', 1)).toInt(), LOW);
          Serial.println("Switched off once");
          c = 0;
          stat--;
        }
        else if (getValue(para, ' ', 2).toInt() == 1)
        {
          digitalWrite((getValue(para, ' ', 1)).toInt(), HIGH);
          Serial.println("Switched ON once");
          c = 0;
          stat--;
        }
      }
    }
  }
}


String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {
    0, -1
  };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//our callback functions
void firstLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay0, HIGH);
    Serial.println("Device1 ON");
  }
  else
  {
    digitalWrite(Relay0, LOW);
    Serial.println("Device1 OFF");
  }
}

void secondLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay1, HIGH);
    Serial.println("Device2 ON");
  }
  else
  {
    digitalWrite(Relay1, LOW);
    Serial.println("Device2 OFF");
  }
}

void thirdLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay2, HIGH);
    Serial.println("Device3 ON");
  }
  else
  {
    digitalWrite(Relay2, LOW);
    Serial.println("Device3 OFF");
  }
}

void fourthLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay3, HIGH);
    Serial.println("Device4 ON");
  }
  else
  {
    digitalWrite(Relay3, LOW);
    Serial.println("Device4 OFF");
  }
}

void fifthLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay4, HIGH);
    Serial.println("Device5 ON");
  }
  else
  {
    digitalWrite(Relay4, LOW);
    Serial.println("Device5 OFF");
  }
}

void sixthLightChanged(uint8_t brightness)
{
  //Control the device
  if (brightness == 255)
  {
    digitalWrite(Relay5, HIGH);
    Serial.println("Device6 ON");
  }
  else
  {
    digitalWrite(Relay5, LOW);
    Serial.println("Device6 OFF");
  }
}
