
/*
  Modbus RS485 Soil Moisture Sensor with temp
   - read all the holding and register values for ID 1,11-16
   - method to change ID 1 to a new ID
   - Sends stream and status to carriots IOT interface
  Circuit:
   - MKR 1000 board
   - MKR 485 shield
         pins A5(RE)and A6(DE), 13 and 14 and voltage
   - Modbus RS485 soil moisture sensor
        by Catnip electronics
        https://www.tindie.com/products/miceuz/modbus-rs485-soil-moisture-sensor-2/ 
               
*/
// ArduinoModbus depends on the ArduinoRS485 library

//******************************************************
//******         VERSION 2.8   ************************
//  2.3 
//      added 1-6 sensors to the break readings
//  2.4 
//      changed time to easier ntp server
//  2.5 
//      fixed time for carriots
//      adjusted the sensor output
//   2.6
//     added relays for low moisture level
//   2.7a
//     changed delays to timer function to speed up operation
//     not all delays were removed
//      a) fixed timing issues - cleaned up some code
//   2.8
//      moved WIFI SSID/Password to the arduino secrets.h file
//      moved Carriots Device ID and API Key to the secrets.h file 
//******************************************************
String softwareFirmware = "2.8";

// Rachaels soilmoisture sensor

#include <NTPClient.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <WiFi101.h>
#include <Wire.h>
#include <ArduinoHttpClient.h>
#include <WiFiUdp.h>
#include <RTCZero.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoRS485.h> 
#include <ArduinoModbus.h>
#include "arduino_secrets.h" 
///////////////////////
// Variables///////////
///////////////////////

boolean watering = true; 

byte bssid[6];
byte encryption =0;
byte packetBuffer[48]; //buffer to hold incoming and outgoing packets
char params[32];
char serverName[] = "api.carriots.com";
/////// Wifi Settings ///////
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (wifi name)
char pass[] = SECRET_PASS;    // your network password (wifi password)
//WiFiSSLClient client;
float pingAverage = 0;
int ping1 = 0;
int ping2 = 0;
int ping3 = 0;
int pingResult;
int totalCount = 0; 
int status = WL_IDLE_STATUS; 
// temp and humidity variables
// the level at which it will water
const int moistureThreshold = 350;
uint16_t soilMoisture = 0;
uint16_t soilMoisture11 = 0;
uint16_t soilMoisture12 = 0;
uint16_t soilMoisture13 = 0;
uint16_t soilMoisture14 = 0;
uint16_t soilMoisture15 = 0;
uint16_t soilMoisture16 = 0;
int16_t soilTempC = 0;
int16_t soilTemp11F = 0;
int16_t soilTemp12F= 0;
int16_t soilTemp13F= 0;
int16_t soilTemp14F = 0;
int16_t soilTemp15F= 0;
int16_t soilTemp16F= 0;

//controller temp
float temp_F=0;
int x=0;
int val = 0;
float temp = 0;
// change to your server's port
int serverPort = 80;
//pin names
const int relay_1 = 0; 
const int relay_2 = 3;
const int relay_3 = 4;
const int relay_4 = 5;
const int relay_5 = 6;
const int relay_6 = 7;
String hostName = "www.google.com";
// counter for one hour loop
int loopTimer = 0;
//************shrekware carriot api key
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const String APIKEY = CARRIOTSAPIKEY; // Replace with your Carriots apikey
const String DEVICE = CARRIOTSDEVICE; // Replace with the id_developer of your device
String wifiFirmware = "*.*";
long rssi = 0;
unsigned long epoch =0;
unsigned long millisSinceWatering = 0; 
unsigned long currentMillis = 0;
unsigned long thisMillis = 0;
unsigned long lastMillis = 0;
// used instead of the delay
const long oneHourInterval = 3599000;
unsigned long previousMillis = 0; 
unsigned long previousMillisGoogle = 0; 
unsigned long tenMinutePreviousMillis = 0; 
//one hour is 3600000 millis
const long pingGoogleTime = 3599000;
const long tenMinutes = 600000;
boolean startUP = false;
//time stuff
/////// time stuff //////////////////////////////////////////////////////////////
// time.nist.gov NTP server
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Defines pin number to which the sensor is connected
#define DHTPIN A1  
#define DHTTYPE DHT22   // DHT 22
#define delayMillis 30000UL
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
//WiFiSSLClient client;
WiFiClient wifiClient;
DHT dht(DHTPIN, DHTTYPE);
/* Create an rtc object */
//RTCZero rtc;

void setup() 
{
   // rtc.begin(); // initialize RTC
    Serial.begin(9600);  
    dht.begin();
 
    pinMode(relay_1, OUTPUT);
    pinMode(relay_2, OUTPUT);
    digitalWrite(relay_1, HIGH);
   
    
  Serial.println("Starting Modbus Client Moisture Sensor");
  // start the Modbus client, DEFAULT the moisture sensor runs at 19200 with a 500ms innterval
  if (!ModbusRTUClient.begin(19200)) 
  {
      Serial.println("Error, the modbus client did not start");
      while (1);
  }   
      while (status != WL_CONNECTED) {
      wifiClient.stop();
      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(ssid);
      // unsuccessful, retry in 4 seconds
      
      status = WiFi.begin(ssid, pass);
      delay(10000);}
  
      Serial.print("You're connected to the network");
      // start time server
      timeClient.begin();
      // set offset for not daylight savings
      // cannt set offset here, it interferes with the epoch time sent to carriots
     // timeClient.setTimeOffset(-14400);
      
      printCurrentNet();
      printWiFiData();
      wifiFirmware = WiFi.firmwareVersion();
      Serial.println();
     currentMillis = millis();
 
}
// Main Program Loop
void loop() 
{
     // get time
     timeClient.update();
  
     readDHT22();

 
       
        readSensors();

        checkMoisture();
           
        checkTempStatus();
        //records average ping to google over 4 pings
        pingGoogle();
       // runs once on start up
          if(!startUP){
                 sendStatusStream();
                 sendStream();
                 startUP=true;
            }
  
        
        // sends status every ten minutes
        tenMinuteCheck();
        
        Serial.println("Waiting for 60 minutes");
        oneHourCheck();
} 

  void readDHT22()
  {
    for (x=1;x<10; x++) 
    { 
      temp = dht.readTemperature();
      val = (int)dht.readHumidity();  
      delay(50);
    }
    //DHT22 sensor
     temp_F = (temp* 9 +2)/5+32;  
 }
   void readSensors()
   {
      //read ID of sensor 1
      readHoldingRegisterValues();
      delay(500);
      //read Baud Rate of sensor 1
      readHoldingRegisterValues2();
      delay(500);
      //read Parity of sensor 1
      readHoldingRegisterValues3();
      delay(500);
      //read Interval of sensor 1
      readHoldingRegisterValues4();
      delay(500);
      //read Moisture value of sensor 11
      readInputRegisterValues(11);
      delay(500);
      //read temperature of sensor 11
      readInputRegisterValuesT(11);
      delay(500);
      //read Moisture value of sensor 1
      readInputRegisterValuesT(1);
      delay(500);
      //read temperature of sensor 1
      readInputRegisterValues(1);
      delay(500);
    }
    
void tenMinuteCheck()
{
 
  
      Serial.print("tenMinuteCheck- this is currentMillis: ");
      Serial.print(currentMillis); 
      if(currentMillis - tenMinutePreviousMillis >= tenMinutes)
      {
        tenMinutePreviousMillis=currentMillis; 
        sendStatusStream();
      }
  
}

void oneHourCheck()

  {
      //this is only to print the number of times it loops, no real reason
      loopTimer++;
      Serial.print("this is the loop timer count: ");
      Serial.print(loopTimer);    
      Serial.print(", ");
      currentMillis = millis();
      Serial.print("this is currentMillis: ");
      Serial.print(currentMillis); 
    if(currentMillis - previousMillis >= oneHourInterval)
    {
         Serial.print("this is currentMillis: ");
         Serial.print(currentMillis); 
         Serial.print("this is previousMillis: ");
         Serial.print(previousMillis);      
         previousMillis = currentMillis;
         // sends data stream t0 carriots  
         sendStream(); 
      }
  }

void pingGoogle()
  {
     if(previousMillisGoogle>0)
     {
        currentMillis = millis();
      
        if (currentMillis - previousMillisGoogle >= pingGoogleTime) 
        {
          previousMillisGoogle = currentMillis;
          pingTimedGoogle();
        }
    }
     else
     {
          pingTimedGoogle();
          previousMillisGoogle = currentMillis;
     }
  
  }
  void pingTimedGoogle(){
    
                 Serial.println("ping average");

        pingG();
        ping1 = pingResult; 
        Serial.println("Waiting for 10 seconds");
        delay(10000);

        pingG();
        ping2 = pingResult;
        Serial.println("Waiting for 10 seconds");
        delay(10000);
        
        pingG();
        Serial.println("Waiting for 10 seconds");
        delay(10000);
        ping3 = pingResult;
        //4th ping = ping result
        pingG();

        pingAverage = (ping1+ping2+ping3+pingResult)/4;
        Serial.print("Average ping time to Google: ");
        Serial.println(pingAverage);
      
    
    
    }

void checkMoisture()
{
  
  //read Moisture value of sensor 11
  for(int x = 11;x<17;x++)
  {
      readInputRegisterValues(x);
    
          switch (x) 
              {
                case 11:
                   if(soilMoisture11<moistureThreshold)
                   {
                     digitalWrite(relay_1, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_1, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
                  break;
                case 12:
                  if(soilMoisture12<moistureThreshold)
                   {
                     digitalWrite(relay_2, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_2, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
                  break;
                case 13:
                    if(soilMoisture13<moistureThreshold)
                   {
                     digitalWrite(relay_3, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_3, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
                   break;
                case 14:
                       if(soilMoisture14<moistureThreshold)
                   {
                     digitalWrite(relay_4, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_4, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
                   break;
                case 15:
                           if(soilMoisture15<moistureThreshold)
                   {
                     digitalWrite(relay_5, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_5, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
           
                   break;
                case 16:
                             if(soilMoisture16<moistureThreshold)
                   {
                     digitalWrite(relay_6, LOW);   
                     Serial.println("less than 350 ");     
                   }
                   else 
                   {
                    digitalWrite(relay_6, HIGH);   
                    Serial.println("else more than 350 ");  
                   }
                   break; 
                default:
                  Serial.print("default switch statement for moisture reached");
                  break;   
                delay(500);  
            }    
  }
 
}
boolean checkTempStatus(){
    
  return true;
  }


    // call to change the ID of Sensor
void writeHoldingRegisterValues() 
{
     Serial.println("Write to Holding Register 1 to change ID ... ");
     //  the values are id number, holding register number 0(ID), and last is the new ID value
     ModbusRTUClient.holdingRegisterWrite(1, 0x00, 1);
     if (!ModbusRTUClient.endTransmission()) 
     {
       Serial.print("failed to connect ");
       // prints error of failure
       Serial.println(ModbusRTUClient.lastError());
     } 
     else 
        {
          Serial.println("changed to ID");
        }
}
//*********************************************************************
//  HOLDING REGISTER VALUES FOR THE MOISTURE SENSOR
// Register number  Size (bytes)  Valid values   Default value Description
//    0                 2             [1 - 247]     1         ID or Modbus address
//    1                 2             [0 - 7]       4         Baud rate
//    2                 2             [0 - 2]       0         Parity Note: most cheap ebay USB to RS485 dongles don't support parity properly!
//    3                 2             [1 - 65535]   500       Measurement interval in milliseconds
//    4                 2             [1 - 65535]   0         Time to sleep in seconds. Write to this register to put the sensor to sleep.
//*********************************************************************
// reads the ID of the moiture sensor
void readHoldingRegisterValues() {
  Serial.println("Reading ID value ... ");
  // read 1 Input Register value from (slave) ID 1, address 0x00
  if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x00, 1)) 
  {
    Serial.print("failed to connect ");
    Serial.println(ModbusRTUClient.lastError());
  }
  else 
      {
        Serial.println("the ID is ");
          while (ModbusRTUClient.available()) 
          {
          Serial.print(ModbusRTUClient.read());
          Serial.print(' ');
          }
        Serial.println();
      }
}
// reads the serial speed/baud rate to the sensor, default is 4 which is 19200
void readHoldingRegisterValues2()
{
     Serial.println("Reading Holding 2 Input Register values for Baud Rate ... ");
     delay(500);
     // read 1 Input Register value from (slave) id 42, address 0x00
  if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x01, 1))
  {
      Serial.print("failed to connect ");
      Serial.println(ModbusRTUClient.lastError());
  } 
  else 
     {
      Serial.println("the baud rate is ");
      while (ModbusRTUClient.available())  
      {
         Serial.print(ModbusRTUClient.read());
         Serial.print(' ');
      }
      Serial.println();
     }
}
// READ HOLDING REGISTER value for Parity
void readHoldingRegisterValues3() 
{
    Serial.println("Reading Holding 3 Input Register values for Parity ... ");
    delay(500);
    // read 1 Input Register value from (slave) id 42, address 0x00
  if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x02, 1)) 
  {
    Serial.print("failed to connect ");
    Serial.println(ModbusRTUClient.lastError());
  } 
  else 
     {
        Serial.println("the parity is ");
        while (ModbusRTUClient.available()) 
        {
          Serial.print(ModbusRTUClient.read());
          Serial.print(' ');
        }
        Serial.println();
     }
}
// READ HOLDING REGISTER value for Interval default is 500
void readHoldingRegisterValues4() 
{
   Serial.println("Reading Holding 4 Input Register values for Interval(500) ... ");
   delay(500);
   // read 1 Input Register value from (slave) id 1, address 0x03
  if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x03, 1))
  {
    Serial.print("failed to connect ");
    Serial.println(ModbusRTUClient.lastError());  
  } 
  else 
     {
        Serial.println("the interval delay is ");
        while (ModbusRTUClient.available()) 
        {
          Serial.print(ModbusRTUClient.read());
          Serial.print(' ');
        }
        Serial.println();
    }
}

// READ Moisture for ID (x)
void readInputRegisterValues(int x) 
{
  soilMoisture=0;
  Serial.print("Reading ID ");
  Serial.print(x);
  Serial.print(", Register 0, MOISTURE value... ");
  // read 1 input value from (slave) ID 11 for moisture
  if (!ModbusRTUClient.requestFrom(x, INPUT_REGISTERS, 0x00,1)) 
  {
      Serial.print("failed to connect ");
      Serial.println(ModbusRTUClient.lastError());
  } 
  else 
     {
        Serial.print("moisture for ID x is ");
    
        while (ModbusRTUClient.available()) 
        {
            soilMoisture = ModbusRTUClient.read();
            Serial.print(soilMoisture);
        }
        Serial.println();
        switch (x) {
          case 1:
            soilMoisture12 = soilMoisture;
            break;
          case 11:
            soilMoisture11= soilMoisture;
            break;
          case 12:
            soilMoisture12= soilMoisture;
            break;
          case 13:
            soilMoisture13= soilMoisture;
            break;
          case 14:
            soilMoisture14= soilMoisture;
            break;
          case 15:
            soilMoisture15= soilMoisture;
            break;
          case 16:
            soilMoisture16= soilMoisture;
            break; 
          default:
            Serial.print("default switch statement for moisture reached");
            break;
        }   
     }
}
// READ Temp for ID 11
void readInputRegisterValuesT(int x) 
{
  Serial.print("Reading ID 11, Register 1, TEMP value... ");
  // read 1 input value from (slave) ID 11 for temp
  if (!ModbusRTUClient.requestFrom(x, INPUT_REGISTERS, 0x01,1)) 
  {
      Serial.print("failed to connect ");
      Serial.println(ModbusRTUClient.lastError());
  } 
  else 
     {
        Serial.println("temp for ID 11 is ");
        while (ModbusRTUClient.available()) 
        {
          soilTempC= ModbusRTUClient.read();
          Serial.print(soilTempC);
        }
        Serial.println();
        
               switch (x) 
               {
                case 1:       
                   soilTemp12F = setDegrees(soilTempC);
                   break;
                case 11:
                  soilTemp11F = setDegrees(soilTempC);
                  break;
                case 12:
                  soilTemp12F = setDegrees(soilTempC);
                  break;
                case 13:
                   soilTemp13F = setDegrees(soilTempC);
                   break;
                case 14:
                   soilTemp14F = setDegrees(soilTempC);
                   break;
                case 15:
                   soilTemp15F = setDegrees(soilTempC);
                   break;
                case 16:
                   soilTemp16F = setDegrees(soilTempC);
                   break; 
                default:
                  Serial.print("default switch statement for moisture reached");
                  break;     
            }
            
     }
 
}
int16_t setDegrees(int16_t Celcius)
{
   Celcius/=10;
   return (Celcius* 9 +2)/5+32;
}
void printCurrentNet() 
{
          // print the SSID of the network you're attached to:
          Serial.print("SSID: ");
          Serial.println(WiFi.SSID());
        
          // print the MAC address of the router you're attached to:
        
          WiFi.BSSID(bssid);
          Serial.print("BSSID: ");
          Serial.print(bssid[5], HEX);
          Serial.print(":");
          Serial.print(bssid[4], HEX);
          Serial.print(":");
          Serial.print(bssid[3], HEX);
          Serial.print(":");
          Serial.print(bssid[2], HEX);
          Serial.print(":");
          Serial.print(bssid[1], HEX);
          Serial.print(":");
          Serial.println(bssid[0], HEX);
        
          // print the received signal strength:
          rssi = WiFi.RSSI();
          Serial.print("signal strength (RSSI):");
          Serial.println(rssi);
        
          // print the encryption type:
          encryption = WiFi.encryptionType();
          Serial.print("Encryption Type:");
          Serial.println(encryption, HEX);
          Serial.println();
}

void printWiFiData() 
{
          // print your WiFi shield's IP address:
          IPAddress ip = WiFi.localIP();
          // the IP address for the shield:
          IPAddress dns(8, 8, 8, 8);  //Google dns  
          Serial.print("IP Address: ");
          Serial.println(ip);
          Serial.println(ip);
}


//////////////////////  
/////Ping Google//////
//////////////////////
void pingG()
{
           //pinging
      Serial.print("Pinging ");
      Serial.print(hostName);
      Serial.print(": ");

      pingResult = WiFi.ping(hostName);

      if (pingResult >= 0) 
      {
            Serial.print("SUCCESS! RTT = ");
            Serial.print(pingResult);
            Serial.println(" ms");
      } 
          else 
          {
            Serial.print("FAILED! Error code: ");
            Serial.println(pingResult);
          }



           // Print date...
           timeClient.update();
         Serial.print("the time is: ");
         Serial.println(timeClient.getFormattedTime());  
           
}

 void print2digits(int number) 
{
        if (number < 10)
        {
            Serial.print("0"); // print a 0 before if the number is < than 10
        }
        Serial.print(number);
}
        ///////////////////////////////
        ////Send stream to Carriots///
        ///////////////////////////////
    
void sendStream() 
{        //get time
      // sendNTPpacket(timeServer); // send an NTP packet to a time server
       epoch =  timeClient.getEpochTime();
             Serial.print("epoch time = ");
            Serial.println(epoch);
       
        if (wifiClient.connect("api.carriots.com",80)) 
        {   // If there's a successful connection
          Serial.println("");
             Serial.println("connected to api.carriots");
             // Build the data field
             rssi = WiFi.RSSI();
             rssi = 100-(-1*rssi);
           
             String json = "{\"protocol\":\"v2\",\"device\":\""+DEVICE+"\",\"at\":"+epoch+",\"data\":{\"WIFI_RSSI\":\""+rssi+"\",\"Average_ping_time_to_Google\":\""+pingAverage+"\",\"JewelSoilMoisture\":\""+soilMoisture11+"\",\"SeaScapeSoilMoisture\":\""+soilMoisture12+"\",\"EarliglowSoilMoisture\":\""+soilMoisture13+"\",\"Controller_Humidity\":\""+val+"\",\"JewelSoilTemp\":\""+soilTemp11F+"\",\"SeaScapeSoilTemp\":\""+soilTemp12F+"\",\"EarliglowSoilTemp\":\""+soilTemp13F+"\",\"Controller_Temp\":\""+temp_F+"\"}}";
            
              Serial.println(json);
           // Make a HTTP request
             wifiClient.println("POST /streams HTTP/1.1");
                   // Serial.println(1);
             wifiClient.println("Host: api.carriots.com");
                   // Serial.println(2);
             wifiClient.println("Accept: application/json");
                   //Serial.println(3);
             wifiClient.println("User-Agent: Arduino-Carriots");
                   //  Serial.println(4);
             wifiClient.println("Content-Type: application/json"); 
                   // wifiClient.println("Content-Type: text/html");                     
             wifiClient.print("carriots.apikey: ");
                   // Serial.println(6);
             wifiClient.println(APIKEY);
                   // Serial.println(7);
             wifiClient.print("Content-Length: ");
               int thisLength = json.length();
             wifiClient.println(thisLength);
          
             wifiClient.println("Connection: close");
             wifiClient.println();
                    //Serial.println(12);
             wifiClient.println(json);
        }
             else 
             {
                        // If you didn't get a connection to the server:
                  Serial.println("sendStream connection failed");                                                                        
                  while (WL_CONNECTION_LOST) 
                  {
                          wifiClient.stop();
                          Serial.print("Attempting to connect to WPA SSID: ");
                          Serial.println(ssid);
                                // unsuccessful, retry in 4 seconds
                          status = WiFi.begin(ssid, pass);
                          delay(10000);
                  }
              }

}
void sendStatusStream() 
{        
      //get time
    //  timeClient.update();
       epoch = timeClient.getEpochTime();
      
             Serial.print("epoch time = ");
                Serial.println(epoch);  
                              
        if (wifiClient.connect("api.carriots.com",80)) 
        {   // If there's a successful connection
             Serial.println("");
             Serial.println("connected to api.carriots");
             // Build the data field
             rssi = WiFi.RSSI();
             rssi = 100-(-1*rssi);
           
             String json = "{\"protocol\":\"v2\",\"device\":\""+DEVICE+"\",\"at\":"+epoch+",\"data\":{\"WIFI_RSSI\":\""+rssi+"\",\"Wifi_Firmware_Version\":\""+wifiFirmware+"\",\"Config_Version\":\""+softwareFirmware+"\"}}";
            
              Serial.println(json);
           // Make a HTTP request
             wifiClient.println("POST /status HTTP/1.1");
                   // Serial.println(1);
             wifiClient.println("Host: api.carriots.com");
                   // Serial.println(2);
             wifiClient.println("Accept: application/json");
                   //Serial.println(3);
             wifiClient.println("User-Agent: Arduino-Carriots");
                   //  Serial.println(4);
             wifiClient.println("Content-Type: application/json"); 
                   // wifiClient.println("Content-Type: text/html");                     
             wifiClient.print("carriots.apikey: ");
                   // Serial.println(6);
             wifiClient.println(APIKEY);
                   // Serial.println(7);
             wifiClient.print("Content-Length: ");
               int thisLength = json.length();
             wifiClient.println(thisLength);
          
             wifiClient.println("Connection: close");
             wifiClient.println();
                    //Serial.println(12);
             wifiClient.println(json);
        }
             else 
             {
                        // If you didn't get a connection to the server:
                  Serial.println("sendStream connection failed");                                                                        
                  while (WL_CONNECTION_LOST) 
                  {
                          wifiClient.stop();
                          Serial.print("Attempting to connect to WPA SSID: ");
                          Serial.println(ssid);
                                // unsuccessful, retry in 4 seconds
                          status = WiFi.begin(ssid, pass);
                          delay(10000);
                  }
              }
}
