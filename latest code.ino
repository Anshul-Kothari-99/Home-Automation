/******************************************************* Adding Libraries *************************************************************************/
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

/******************************************************* Routing Switches to mcu pins *************************************************************************/
#define Relay0            D1  //GPIO5 --> SWITCH 1
#define Relay1            D2  //GPIO4 --> SWITCH 2
#define Relay2            D5  //GPIO14 --> SWITCH 3
#define Relay3            D6  //GPIO12 --> SWITCH 4
#define Relay4            D7  //GPIO13 --> SWITCH 5
#define Relay5            D8  //GPIO15 --> SWITCH 6

/******************************************************* Adafruit.io Setup *************************************************************************/
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "Anshul_k"            // Replace it with your username
#define AIO_KEY         "aio_VjOD41G8l9BtwfwpWd38LILio3GJ"   // Replace with your Project Auth Key

/******************************************************* Set AP Mode Credentials  *************************************************************************/
#define AP_SSID "AK_NodeMCU"
#define AP_PASS "1234567890"

/******************************************************* callback functions *************************************************************************/

void MQTT_connect();  //Function for MQTT----must be related to WebSocket

void schedulein(String para);   //Function declaration for Scheduling task

String getValue(String data, char separator, int index); //Function declaration for Splitting string

void firstLightChanged(uint8_t brightness);   //Function called when alexa is told to do something
void secondLightChanged(uint8_t brightness);
void thirdLightChanged(uint8_t brightness);
void fourthLightChanged(uint8_t brightness);
void fifthLightChanged(uint8_t brightness);
void sixthLightChanged(uint8_t brightness);

/******************************************************* Device Names *************************************************************************/
String Device_1_Name = "Device one";
String Device_2_Name = "Device two";
String Device_3_Name = "Device three";
String Device_4_Name = "Device four";
String Device_5_Name = "Device five";
String Device_6_Name = "Device six";

String WIFI_SSID = "Dhruv K";           // Your SSID
String WIFI_PASS = "1234567890";      // Your password

String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};    //Week Days
String temp;  //temporary(temp) variable  //for scheduling use
String formattedTime;   //used in schedulein() function
String weekDay; //used in schedulein() function
String currentMonthName;  //used in schedulein() function
String currentDate;     //Print complete date: //used in schedulein() function

String channel_id = "lr";

int time_counter;  //updated regularly...if in case it does not gets updates for a certain period of time this will be an indication that the system is not conneected online
int stat = 0; //(1 if there is a pending schedules and 0 if no pending schedules)
int currentHour, currentMinute, currentSecond, monthDay, currentMonth, currentYear; //used in schedulein() function

uint addr = 0; //possibly the initial reference address for EEPROM
int8_t ret; //possibly variable to count the number of times ESP fails to connect to Wifi   //used in MQTT_connect() function

unsigned long epochTime;  //used in schedulein() function

struct tm *ptm; //Get a time structure  //used in schedulein() function

//structure for the eeprom variable
struct {
  int check_bit_wifi = 0; // 1-if wifi credentials are updated
  int check_bit_sch = 0;  // 1-if sceduling data is updated
  int check_bit_switch_names = 0;   // 1-if device names are updated
  String sch = "";
  //String switch_names = "";
  String switch_1 = "";
  String switch_2 = "";
  String switch_3 = "";
  String switch_4 = "";
  String switch_5 = "";
  String switch_6 = "";
  String id = "";
  String pass = "";
} eeprom_data;    //this is the name of the struct variable used throughout

IPAddress ip (1, 1, 1, 1); // The remote ip to ping cloudfare.....to ping google use --> 8.8.8.8

/************************************** Global State (you don't need to change this!) **************************************************************/
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
// WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
/************************************************************ Feeds *********************************************************************************/

// Setup a feed called 'HomeAutomation' for subscribing to changes.
Adafruit_MQTT_Subscribe HomeAutomation = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME"/feeds/lr"); // FeedName ///////////////////////////////////////////////////////// Here feed name needs to be generalized

//Setting Up for fetching RTC
WiFiUDP ntpUDP; // FOR FETCHING REAL TIME
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); //For India It is offset to 19800 (3600*5.5)

//Setting Up for Alexa Communication
Espalexa espalexa;

void setup() {
  Serial.begin(115200);

  pinMode(Relay0, OUTPUT);
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  pinMode(Relay4, OUTPUT);
  pinMode(Relay5, OUTPUT);

  pinMode(D4, OUTPUT); //D4 pin is for InBuilt_LED    //remember it works on acttive low logic
  digitalWrite(D4, HIGH);

  EEPROM.begin(512);

  EEPROM.get(addr, eeprom_data);
  Serial.println("");
  Serial.println("Data Received From EEPROM is -->");
  Serial.println("check_bit_wifi: " + String(eeprom_data.check_bit_wifi) + " , check_bit_sch: " + String(eeprom_data.check_bit_sch) + " , check_bit_switch_names: " + String(eeprom_data.check_bit_switch_names));
  Serial.println("ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass));
  Serial.println("sch data: " + String(eeprom_data.sch));
  Serial.print("Device rename data: ");
  Serial.print("Switch_1: " + String(eeprom_data.switch_1));
  Serial.print(" , Switch_2: " + String(eeprom_data.switch_2));
  Serial.print(" , Switch_3: " + String(eeprom_data.switch_3));
  Serial.print(" , Switch_4: " + String(eeprom_data.switch_4));
  Serial.print(" , Switch_5: " + String(eeprom_data.switch_5));
  Serial.println(" , Switch_6: " + String(eeprom_data.switch_6));
  Serial.println("");

  //if (eeprom_data.check_bit == 1 || eeprom_data.check_bit == )
  if (eeprom_data.check_bit_wifi == 1)
  {
    Serial.println("Wifi credentials updated successfully as ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass));
    WIFI_SSID = eeprom_data.id;
    WIFI_PASS = eeprom_data.pass;
  }

  EEPROM.get(addr, eeprom_data);
  if (eeprom_data.check_bit_sch == 1)
  {
    stat = 1;
    temp = eeprom_data.sch;
    Serial.println("updated Scheduling DATA in Temprory(Temp) variable successfully as: " + String(temp));
  }

  EEPROM.get(addr, eeprom_data);
  if (eeprom_data.check_bit_switch_names == 1)
  {
    Device_1_Name = eeprom_data.switch_1;
    Device_2_Name = eeprom_data.switch_2;
    Device_3_Name = eeprom_data.switch_3;
    Device_4_Name = eeprom_data.switch_4;
    Device_5_Name = eeprom_data.switch_5;
    Device_6_Name = eeprom_data.switch_6;
  }

  WiFi.mode(WIFI_AP_STA);
  // Begin Access Point
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("IP address for AP_Network ");
  Serial.print(AP_SSID);
  Serial.print(" : ");
  Serial.println(WiFi.softAPIP());

  // Connecting to WiFi in STA Mode.
  Serial.println("Connecting to ");
  Serial.println(WIFI_SSID);

  //Begin WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(D4, LOW); //After connecting to WiFi Indicate through InBuilt_LED

  Serial.println("WiFi connected");
  Serial.print("IP address for STA mode is: ");
  Serial.println(WiFi.localIP());
  Serial.println("");

  // Define your devices here.
  espalexa.addDevice(Device_1_Name, firstLightChanged); //simplest definition, default state off
  espalexa.addDevice(Device_2_Name, secondLightChanged);
  espalexa.addDevice(Device_3_Name, thirdLightChanged);
  espalexa.addDevice(Device_4_Name, fourthLightChanged);
  espalexa.addDevice(Device_5_Name, fifthLightChanged);
  espalexa.addDevice(Device_6_Name, sixthLightChanged);
  espalexa.begin();

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
      time_counter = 0;
      Serial.println(s);
      //int HomeAutomation_State = atoi((char *)HomeAutomation.lastread);

      if (s[0] == 'r')
        ESP.restart();

      if (s[0] == 'x')
      {
        stat = 1;
        temp = s;
        ////////////*******************************************************************///this check bit is not logically correct.....go through this again
        /*
          if (eeprom_data.check_bit != 2)
          {
          Serial.println("check bit updated to 51");
          //eeprom_data.check_bit = 51;
          eeprom_data.check_bit = 2;
          }
          else
          {
          Serial.println("check bit updated to 2");
          eeprom_data.check_bit = 2;
          }
        */
        eeprom_data = eeprom_data;
        eeprom_data.sch = temp;
        eeprom_data.check_bit_sch = 1;
        EEPROM.put(addr, eeprom_data);
        EEPROM.commit();
        delay(1000);
        Serial.println("EEPROM COMMIT Done for updating Scheduling data.....lets recheck if its updated on EEPROM");
        EEPROM.get(addr, eeprom_data);
        Serial.println("Data Received From EEPROM is --> Sch_Data: " + String(eeprom_data.sch) + " , check_bit_sch: " + String(eeprom_data.check_bit_sch));
      }

      if (s[0] == 's')
      {
        Device_1_Name = getValue(s, ';', 1);
        Device_2_Name = getValue(s, ';', 2);
        Device_3_Name = getValue(s, ';', 3);
        Device_4_Name = getValue(s, ';', 4);
        Device_5_Name = getValue(s, ';', 5);
        Device_6_Name = getValue(s, ';', 6);
        /*
                if (eeprom_data.check_bit != 3)
                {
                  Serial.println("check bit updated to 51");
                  eeprom_data.check_bit = 3;
                }
                else
                {
                  Serial.println("check bit updated to 3");
                  eeprom_data.check_bit = 3;   ///this check bit is not logically correct.....go through this again
                }
        */
        eeprom_data = eeprom_data;
        eeprom_data.switch_1 = getValue(s, ';', 1);
        eeprom_data.switch_2 = getValue(s, ';', 2);
        eeprom_data.switch_3 = getValue(s, ';', 3);
        eeprom_data.switch_4 = getValue(s, ';', 4);
        eeprom_data.switch_5 = getValue(s, ';', 5);
        eeprom_data.switch_6 = getValue(s, ';', 6);
        eeprom_data.check_bit_switch_names = 1;
        EEPROM.put(addr, eeprom_data);
        EEPROM.commit();
        delay(1000);
        Serial.println("EEPROM COMMIT Done for updating Switch renaming data.....lets recheck if its updated on EEPROM");
        EEPROM.get(addr, eeprom_data);
        //        Serial.println("Data Received From EEPROM is --> switch_names: " + String(eeprom_data.switch_1) + String(eeprom_data.switch_2) + String(eeprom_data.switch_3) + String(eeprom_data.switch_4) + String(eeprom_data.switch_5) + String(eeprom_data.switch_6) + " , check_bit_switch_names: " + String(eeprom_data.check_bit_switch_names));
        Serial.println("Data Received From EEPROM is -->");
        Serial.println(" , check_bit_switch_names: " + String(eeprom_data.check_bit_switch_names));
        Serial.print("Device rename data: ");
        Serial.print("Switch_1: " + String(eeprom_data.switch_1));
        Serial.print(" , Switch_2: " + String(eeprom_data.switch_2));
        Serial.print(" , Switch_3: " + String(eeprom_data.switch_3));
        Serial.print(" , Switch_4: " + String(eeprom_data.switch_4));
        Serial.print(" , Switch_5: " + String(eeprom_data.switch_5));
        Serial.println(" , Switch_6: " + String(eeprom_data.switch_6));
        Serial.println("");
      }

      if (s[0] == 'w')  //"wifiCredentialsReset;new_login_id;new_password"
      { digitalWrite(D4, HIGH);
        WIFI_SSID = getValue(s, ';', 1);
        WIFI_PASS = getValue(s, ';', 2);
        /*
                if (eeprom_data.check_bit != 1)
                {
                  Serial.println("Updating the check bit to 51");
                  eeprom_data.check_bit = 1;
                }
                else
                {
                  Serial.println("Updating the check bit to 1");
                  eeprom_data.check_bit = 1;   ///this check bit is not logically correct.....go through this again
                }
        */
        eeprom_data = eeprom_data;
        eeprom_data.id = getValue(s, ';', 1);
        eeprom_data.pass = getValue(s, ';', 2);
        eeprom_data.check_bit_wifi = 1;
        EEPROM.put(addr, eeprom_data);
        EEPROM.commit();
        delay(100);
        Serial.println("EEPROM COMMIT Done for WIFI Credentials.....lets recheck if its updated on EEPROM");
        EEPROM.get(addr, eeprom_data);
        Serial.println("Data Received From EEPROM is --> ID: " + String(eeprom_data.id) + " , Password: " + String(eeprom_data.pass) + " , check_bit_wifi: " + String(eeprom_data.check_bit_wifi));

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
        Serial.println("Switching Relay0 HIGH");
      }
      if (s[0] == '0')
      {
        digitalWrite(Relay0, LOW);
        Serial.println("Switching Relay0 LOW");
      }

      if (s[1] == '1')
      {
        digitalWrite(Relay1, HIGH);
        Serial.println("Switching Relay1 HIGH");
      }
      if (s[1] == '0')
      {
        digitalWrite(Relay1, LOW);
        Serial.println("Switching Relay1 LOW");
      }

      if (s[2] == '1')
      {
        digitalWrite(Relay2, HIGH);
        Serial.println("Switching Relay2 HIGH");
      }
      if (s[2] == '0')
      {
        digitalWrite(Relay2, LOW);
        Serial.println("Switching Relay2 LOW");
      }

      if (s[3] == '1')
      {
        digitalWrite(Relay3, HIGH);
        Serial.println("Switching Relay3 HIGH");
      }
      if (s[3] == '0')
      {
        digitalWrite(Relay3, LOW);
        Serial.println("Switching Relay3 LOW");
      }

      if (s[4] == '1')
      {
        digitalWrite(Relay4, HIGH);
        Serial.println("Switching Relay4 HIGH");
      }
      if (s[4] == '0')
      {
        digitalWrite(Relay4, LOW);
        Serial.println("Switching Relay4 LOW");
      }

      if (s[5] == '1')
      {
        digitalWrite(Relay5, HIGH);
        Serial.println("Switching Relay5 HIGH");
      }
      if (s[5] == '0')
      {
        digitalWrite(Relay5, LOW);
        Serial.println("Switching Relay5 LOW");
      }
    }
  }
}

void MQTT_connect() {
  // Stop if already connected.
  if (mqtt.connected()) {
    digitalWrite(D4, LOW);
    if (time_counter >= 20)               // 1=2.7 secs -- multiply and set accordingly...after this much time used to checks whether it is also connected to internet or not by sending ping to server
    {
      if (Ping.ping(ip))
        time_counter = 0;
      else
        goto notconnected;
      Serial.println("connected to internet");
      digitalWrite(D4, LOW);
      return;
    }
    time_counter++;
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

    //mode - 1 - enters a repeating schedule as per week days --> "xxxxxxxxxxx;GPIOno;1/0;0;7_letter_no.(for weekend use 1-->Sunday to 7-->Saturday else use 9);yy;hour;min;epoch_time"
    //mode - 2 - enters date - format should be --> "xxxxxxxxxxx;GPIOno;1/0;dd;mm;yy;hour;min;epoch_time"

    int tc = getValue(para, ';', 4).toInt(), wd = timeClient.getDay() + 1;
    if (getValue(para, ';', 5).toInt() == (ptm->tm_year - 100))
    {
      if (getValue(para, ';', 3).toInt() == 0 && ((tc % 10 == wd) || ((tc / 10) % 10 == wd) || ((tc / 100) % 10 == wd) || ((tc / 1000) % 10 == wd) || ((tc / 10000) % 10 == wd) || ((tc / 100000) % 10 == wd) || ((tc / 1000000) % 10 == wd)) && getValue(para, ';', 6).toInt() == timeClient.getHours() && getValue(para, ';', 7).toInt() == timeClient.getMinutes())
      {
        if (getValue(para, ';', 2).toInt() == 0)
        {
          digitalWrite(getValue(para, ';', 1).toInt(), LOW);
          Serial.println("Switched off for repetition");
          time_counter = 0;
          stat--;
        }
        else if (getValue(para, ';', 2).toInt() == 1)
        {
          digitalWrite((getValue(para, ';', 1)).toInt(), HIGH);
          Serial.println("Switched ON for repetition");
          time_counter = 0;
          stat--;
        }
      }
      else if (getValue(para, ';', 4).toInt() == (ptm->tm_mon + 1) && getValue(para, ';', 3).toInt() == ptm->tm_mday && getValue(para, ';', 6).toInt() == timeClient.getHours() && getValue(para, ';', 7).toInt() == timeClient.getMinutes()) // dd!=0 (user entered specific date)
      {
        if (getValue(para, ';', 2).toInt() == 0)
        {
          digitalWrite((getValue(para, ';', 1)).toInt(), LOW);
          eeprom_data = eeprom_data;
          eeprom_data.check_bit_sch = 0;
          EEPROM.put(addr, eeprom_data);
          EEPROM.commit();
          delay(1000);
          Serial.println("Switched off once");
          time_counter = 0;
          stat--;
        }
        else if (getValue(para, ';', 2).toInt() == 1)
        {
          digitalWrite((getValue(para, ';', 1)).toInt(), HIGH);
          eeprom_data = eeprom_data;
          eeprom_data.check_bit_sch = 0;
          EEPROM.put(addr, eeprom_data);
          EEPROM.commit();
          delay(1000);
          Serial.println("Switched ON once");
          time_counter = 0;
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
