/*
  Modbus RS485 Soil Moisture Sensor with temp
   - read all the holding and register values for ID 1,11-16
   - method to change ID 1 to a new ID
   - Sends stream and status to (formerly)carriots IOT interface
        NOW https://www.altairsmartcore.com
  Circuit:
   - MKR 1000 board
   - MKR 485 shield
         pins A5(RE)and A6(DE), 13 and 14 and voltage
   - Modbus RS485 soil moisture sensor
        by Catnip electronics
        https://www.tindie.com/products/miceuz/modbus-rs485-soil-moisture-sensor-2/ 
   -  SparkFun 16x2 SerLCD - RGB on Black 3.3V   
        https://www.sparkfun.com/products/14073
        https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
   - 6 position relay board - generic
*/
//******************************************************
//******         VERSION 4.4g
//       added 1-6 sensors to the break readings
//    2.4 
//       changed time to easier ntp server
//    2.5 
//       fixed time for carriots
//       adjusted the sensor output
//    2.6
//       added relays for low moisture level
//    2.7a
//       changed delays to timer function to speed up operation
//       not all delays were removed
//       fixed timing issues - cleaned up some code
//    2.8
//       moved WIFI SSID/Password to the arduino secrets.h file
//       moved Carriots Device ID and API Key to the secrets.h file 
//     b)
//       added sensor 12-16, amd fixed serial output messages to reflect the true sensor ID
//       changed output to carriots for Data Stream to send sensors 11-16 
//    3.0 
//       added WebServer for access to pump Control
//       added moisture and temperature for 6 sensors
//    3.1 
//       added MDNS name http://ceres.local/ to connect to web server
//    3.1a  
//       added moisture is greater than 0 meaning if sensors are offline, dont turn pump onn
//    4.0 
//       added lcd display  Sparkfun I2C 
//       SparkFun 16x2 SerLCD - RGB on Black 3.3V
//       https://www.sparkfun.com/products/14073
//    4.1  
//       added watchdogtimer for auto reboot on code hang
//    4.2  
//       added checkPump function, removed delay while pump running
//    4.3b  
//       added boolean wifiUp - works with or without wifi
//       added LCD changes to ORANGE when wifi connection fails
//    4.4a 
//       cleaned up code
//    4.4b
//       fixed carriots address and rem errors
//    4.4c
//       fixed carriots info in arduino_secrets.h file
//    4.4e 
//       changed NTP time lookup to time.update
//       removed the 4 pings to google and replaced with one every ten minutes
//    4.4f pump timer 20 seconds
//    4.4g  fixed epoch time
//******************************************************
String softwareFirmware = "4.4g";
//#######################################################
//########### Ceres Helper ##############################
// watchdog timer
#include <Adafruit_SleepyDog.h>
// dns responder
#include <WiFiMDNSResponder.h>
#include <NTPClient.h>
//#include <SPI.h>
//#include <Adafruit_Sensor.h>
#include <WiFi101.h>
#include <Wire.h>
#include <ArduinoHttpClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoRS485.h> 
#include <ArduinoModbus.h>
#include "arduino_secrets.h" 
// Sparkpost library for the lcd display
//https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
#include <SerLCD.h>
// Initialize the lcd library with default I2C address 0x72
SerLCD lcd; 
///////////////////////
// Variables///////////
///////////////////////

//*****************************************
//*******Changable Variables***************
//   generally the pump timer will kick in before the highWaterThreshold
//   manual pump over ride timer
     const int pumpTimer = 20000; 
//   the level at which it will water
     const int moistureThreshold = 325;
//   the high water threshold
     const int highWaterThreshold = 400;
//****************************************

char mdnsName[] = "ceres"; // the MDNS name that the board will respond to
boolean watering = true; 
// Create a MDNS responder to listen and respond to MDNS name requests.
WiFiMDNSResponder mdnsResponder;
byte bssid[6];
byte encryption =0;
byte packetBuffer[48]; //buffer to hold incoming and outgoing packets
char params[32];
char serverName[] = "api.carriots.com";
/////// Wifi Settings ///////
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (wifi name)
char pass[] = SECRET_PASS;    // your network password (wifi password)
// ping google variables
float pingAverage = 0;
int ping1 = 0;
int ping2 = 0;
int ping3 = 0;
int pingResult;
int totalCount = 0; 
// wifi status
int status = WL_IDLE_STATUS; 
// temp and humidity sensor variables
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

//DHT22 temp/humidity
float temp_F=0;
int x=0;
int val = 0;
float temp = 0;
// change to your server's port
int serverPort = 80;
// the AP server port number
WiFiServer server(80);
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
int loopsensor = 11;
long pumpOn = 0;
//pump timer
int pumpTimer_11=0;
int pumpTimer_12=0;
int pumpTimer_13=0;
int pumpTimer_14=0;
int pumpTimer_15=0;
int pumpTimer_16=0;
//************shrekware carriot api key
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const String APIKEY = CARRIOTSAPIKEY; // Replace with your Carriots apikey
const String DEVICE = CARRIOTSDEVICE; // Replace with the id_developer of your device
String wifiFirmware = "*.*";
long rssi = 0;
long epoch=0;
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
unsigned long previousMillisLoop = 0;
boolean startUP = false;

//    time stuff
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
boolean wifiUp = false;
WiFiClient wifiClient;
// temp and humidity sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() 
  {
    Wire.begin();
    lcd.begin(Wire); //Set up the LCD for I2C communication
    Wire.setClock(400000); //Optional - set I2C SCL to High Speed Mode of 400kHz
    lcd.setBacklight(255, 255, 255); //Set backlight to bright white
    lcd.setContrast(4); //Set contrast. Lower to 0 for higher contrast.
    lcd.clear(); //Clear the display - this moves the cursor to home position as well
    lcd.print("The Ceres Helper");
    lcd.setCursor(0, 1);
    lcd.print("starting ...");
    
    // First a normal example of using the watchdog timer.
    // Enable the watchdog by calling Watchdog.enable() as below.
    // This will turn on the watchdog timer with a ~4 second timeout
    // before reseting the Arduino. The estimated actual milliseconds
    // before reset (in milliseconds) is returned.
    // Make sure to reset the watchdog before the countdown expires or
    // the Arduino will reset!
    int countdownMS = Watchdog.enable(8000);
    Serial.print("Enabled the watchdog with max countdown of ");
    Serial.print(countdownMS, DEC);
    Serial.println(" milliseconds!");
  
    dht.begin();
    
    pinMode(relay_1, OUTPUT);
    digitalWrite(relay_1, HIGH);
    pinMode(relay_2, OUTPUT);
    digitalWrite(relay_2, HIGH);
    pinMode(relay_3, OUTPUT);
    digitalWrite(relay_3, HIGH);
    pinMode(relay_4, OUTPUT);
    digitalWrite(relay_4, HIGH);
    pinMode(relay_5, OUTPUT);
    digitalWrite(relay_5, HIGH);
    pinMode(relay_6, OUTPUT);
    digitalWrite(relay_6, HIGH);
    
    Serial.begin(9600);
      
    Serial.println("Starting Modbus Client Moisture Sensor");
    // start the Modbus client, DEFAULT the moisture sensor runs at 19200 with a 500ms innterval
   if (!ModbusRTUClient.begin(19200)) 
     {
       Serial.println("Error, the modbus client did not start");
       while (1);
     } 
   Watchdog.reset();
   int m = 0;    
   while(status != WL_CONNECTED)
     {   
         m++;
         wifiClient.stop();
         if(m>5)
           {
             wifiUp=false;
             lcd.setBacklight(0xFF8C00);
             break;
           }
         wifiUp=true;
         Serial.print("Attempting to connect to WPA SSID: ");
         Serial.println(ssid);
        // unsuccessful, retry in 10 seconds
         status = WiFi.begin(ssid, pass);
         delay(3000);
         Watchdog.reset();
      }
     Watchdog.reset();
    if(wifiUp)
      {
         Serial.print("wifiUp is :");
         Serial.println(wifiUp);
         delay(3000);
         lcd.clear(); 
         lcd.print("You're connected to the network"); 
         Serial.print("You're connected to the network");
         Watchdog.reset();
         delay(3000);
         Watchdog.reset();
         // start time server
         timeClient.begin();
         // set offset for not daylight savings
         // cant set offset here, it interferes with the epoch time sent to carriots
         // timeClient.setTimeOffset(-14400);
      
         printCurrentNet();
         printWiFiData();
         server.begin();
         wifiFirmware = WiFi.firmwareVersion();
         Serial.println();
         currentMillis = millis();
         Watchdog.reset();
      
         // Setup the MDNS responder to listen to the configured name.
         // NOTE: You _must_ call this _after_ connecting to the WiFi network and
         // being assigned an IP address.
         if (!mdnsResponder.begin(mdnsName)) 
           {
              Serial.println("Failed to start MDNS responder!");
              while(1);
           }
         Serial.print("Server listening at http://");
         Serial.print(mdnsName);
         Serial.println(".local/");
         Watchdog.reset();
         lcd.clear(); 
         lcd.print("Server at       " );
         lcd.print(mdnsName);
         lcd.print(".local/");
         delay(5000);
         Watchdog.reset();

      }
          //no WIFI
      else
        {
           Serial.println("no WIFI detected");
           wifiFirmware = WiFi.firmwareVersion();
           Serial.println();
           currentMillis = millis();
           Watchdog.reset();
        }
  }
     // Main Program Loop
     
void loop() 
  {   
      Watchdog.reset();
    
     //wifiUp
    if(wifiUp)
     {
      // Watchdog.reset();
       // runs once on start up
       if(!startUP)
        {
          lcd.clear(); 
          lcd.print("The Ceres Helper" );
          lcd.print("reading sensors");
          Watchdog.reset();
          readSensors();
          Watchdog.reset();
          sendStatusStream();
          sendStream();
          startUP=true;
        } 
        
       Watchdog.reset();
        // check whether the reading should trip the relay
     
       if(loopsensor >16){loopsensor=11;}
       checkMoisture(loopsensor); 
       lcd.clear(); 
       lcd.print("Moisture " );
       lcd.print(loopsensor);
       lcd.print(":" );
       lcd.print(soilMoisture);
       lcd.setCursor(0,1);
       lcd.print("UpTime:" );
       // Print the number of seconds since reset:
       lcd.print(millis() / 1000);

       //  sendLcd("this is the send to LCD void");
       // Call the update() function on the MDNS responder every loop iteration to
       // make sure it can detect and respond to name requests.
       Watchdog.reset();
       mdnsResponder.poll();
       // get time
       timeClient.update();
       readDHT22();
       Watchdog.reset();

       Watchdog.reset();
       loopsensor++;
       // currently empty function, returns true   
       checkTempStatus();
       //records average ping to google over 4 pings
       pingGoogle();
       Watchdog.reset();
       
       // sends status every ten minutes
       Serial.println("10 Minute Check");
       tenMinuteCheck();
       
       // sends data every hour
       Serial.println("60 Minute Check");
       Watchdog.reset();
       oneHourCheck();
  
    //   pumpCheck();
     
       WiFiClient wifiClient = server.available();   // listen for incoming clients

       if (wifiClient) 
        {                             // if you get a client,
        Serial.println("new client");           // print a message out the serial port
        String currentLine = "";  // make a String to hold incoming data from the client
   
        while (wifiClient.connected()){          // loop while the client's connected
        if (wifiClient.available()) {             // if there's bytes to read from the wifiClient,
        char c = wifiClient.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') // if the byte is a newline character
        {                    
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the wifiClient HTTP request, so send a response:
         if (currentLine.length() == 0) 
         {
           // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
           // and a content-type so the wifiClient knows what's coming, then a blank line:
           wifiClient.println("HTTP/1.1 200 OK");
           wifiClient.println("Content-type:text/html");
           wifiClient.println();
           wifiClient.println("<html>");
           wifiClient.println("<head>");
           wifiClient.println("<style type=\"text/css\"> body {background-color: #d3e8c8; margin:50px; padding:20px; line-height: 400% } h2 {font-size: 52px;color: blue;text-align: center;}p {font-family: verdana;font-size:xx-large;}</style>");
           wifiClient.println("<title>Manual Pump Override</title>");
           wifiClient.println("</head>");
           wifiClient.println("<body>");
          // the content of the HTTP response follows the header:
           wifiClient.print("<h2>PUMP OVERRIDE</h2>");
           wifiClient.print("<p style=\"color:Crimson\">");
           wifiClient.print("Sensor 11 moisture reading : ");
           wifiClient.print(soilMoisture11);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/1\">here</a> to turn PUMP 11 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:Maroon\">");
            //wifiClient.println("<style type=\"text/css\"> body {background-color: #d3e8c8; margin:50px; padding:20px; line-height: 400% } h2 {font-size: 52px;color: blue;text-align: center;}p {font-family: verdana;font-size:xx-large;}</style>");
           wifiClient.print("Sensor 12 moisture reading : ");
           wifiClient.print(soilMoisture12);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/2\">here</a> to turn PUMP 12 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:DarkGoldenRod\">");
           wifiClient.print("Sensor 13 moisture reading : ");
           wifiClient.print(soilMoisture13);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/3\">here</a> to turn PUMP 13 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:SlateGray\">");
           wifiClient.print("Sensor 14 moisture reading : ");
           wifiClient.print(soilMoisture14);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/4\">here</a> to turn PUMP 14 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:Green\">");
           wifiClient.print("Sensor 15 moisture reading : ");
           wifiClient.print(soilMoisture15);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/5\">here</a> to turn PUMP 15 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:FireBrick\">");
           wifiClient.print("Sensor 16 moisture reading : ");
           wifiClient.print(soilMoisture16);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/6\">here</a> to turn PUMP 16 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<h2>Temperatures</h2>");
           wifiClient.print("<p style=\"color:Crimson\">");
           wifiClient.print("Sensor 11 temperature: ");
           wifiClient.print(soilTemp11F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:Maroon\">");
           wifiClient.print("Sensor 12 temperature: ");
           wifiClient.print(soilTemp12F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:DarkGoldenRod\">");
           wifiClient.print("Sensor 13 temperature: ");
           wifiClient.print(soilTemp13F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:SlateGray\">");
           wifiClient.print("Sensor 14 temperature: ");
           wifiClient.print(soilTemp14F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:Green\">");
           wifiClient.print("Sensor 15 temperature: ");
           wifiClient.print(soilTemp15F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:FireBrick\">");
           wifiClient.print("Sensor 16 temperature: ");
           wifiClient.print(soilTemp16F);
           wifiClient.print("<br>");
           wifiClient.print("<hr>");
           wifiClient.print("Software Version: ");
            //wifiClient.print(F("<input type='value' name=LowSetText value="));
           wifiClient.print (softwareFirmware);
           wifiClient.println("</p>");
           // The HTTP response ends with another blank line:
           wifiClient.println();
           // break out of the while loop:
           break;
          }
          else 
          {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') 
           {    // if you got anything else but a carriage return character,
              currentLine += c;      // add it to the end of the currentLine
           }
        // Check to see if the wifiClient request was "GET /H" or "GET /L":
           if (currentLine.endsWith("GET /1")) {
          digitalWrite(relay_1, LOW);               // GET /1 turns Pump 1 on
           Serial.println("Pin 1 Low");
           pumpTimer_11 = millis();
        }
        if (currentLine.endsWith("GET /2")) {
          digitalWrite(relay_2, LOW);                // GET /2 turns Pump 2 on
           Serial.println("relay_2  LOW");
              pumpTimer_12 = millis();
        }
         if (currentLine.endsWith("GET /3")) {
          digitalWrite(relay_3, LOW);        // GET /3 turns Pump 3 on
           Serial.println("relay_3 LOW");
              pumpTimer_13 = millis();
        }
        if (currentLine.endsWith("GET /4")) {
          digitalWrite(relay_4, LOW);                // GET /4 turns Pump 4 on
          Serial.println("relay_4 LOW");
             pumpTimer_14 = millis();
        }
              if (currentLine.endsWith("GET /5")) {
          digitalWrite(relay_5, LOW);               // GET /5 turns Pump 5 on
          Serial.println("Pin 5 LOW");
             pumpTimer_15 = millis();
   
        }
              if (currentLine.endsWith("GET /6")) {
          digitalWrite(relay_6, LOW);               // GET /6 turns Pump 6 on
          Serial.println("Pin 6 LOW");
             pumpTimer_16 = millis();

        }
      }
    }
       delay(1);
    // close the connection:
    wifiClient.stop();
    Serial.println("wifiClient disonnected");
  }



  }
//****************************************************
//****************************************************
//   when there is NO WIFI
//****************************************************
//****************************************************
  else
    {
      // sets lcd to ORANGE no wifi
      //orange
      Watchdog.reset();
      // runs once on start up
      if(!startUP)
      {
        lcd.clear(); 
        lcd.print("The Ceres Helper" );
        lcd.print("reading sensors");
        readSensors();
        Watchdog.reset();
        startUP=true;
       } 
       Watchdog.reset(); 
       if(loopsensor >16){loopsensor=11;}
       lcd.clear(); 
       lcd.print("Moisture " );
       lcd.print(loopsensor);
       lcd.print(":" );
       lcd.print(soilMoisture);
       lcd.setCursor(0,1);
       lcd.print("UpTime:" );
       // Print the number of seconds since reset:
       lcd.print(millis() / 1000);
  
       Watchdog.reset();
       
       readDHT22();
       // check whether the reading should trip the relay
       checkMoisture(loopsensor);
       Watchdog.reset();
       loopsensor++;
       // currently empty function, returns true   
       checkTempStatus();
       pumpCheck(); 
    }
 //end Void LOOP      
}

void pumpCheck()
  {
      Watchdog.reset();
      currentMillis = millis();
      Serial.print("pumpCheck - this is currentMillis: ");
      Serial.println(currentMillis); 
      Serial.println(); 
     if(currentMillis - pumpTimer_11>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_1, HIGH);        
     }
         if(currentMillis - pumpTimer_12>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_2, HIGH);        
     }
         if(currentMillis - pumpTimer_13>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_3, HIGH);        
     }      
          if(currentMillis - pumpTimer_14>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_4, HIGH);        
     }
         if(currentMillis - pumpTimer_15>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_5, HIGH);        
     }
         if(currentMillis - pumpTimer_16>= pumpTimer)
     {    //watered for 5 seconds
              digitalWrite(relay_6, HIGH);        
     } 
  }
  
void readDHT22()
   {
         Watchdog.reset();
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
      Watchdog.reset();
     //*******************UNCOMMENNT THE FUNCTIONS YOU WOULD LIKE TO RUN ***********************************
     //read ID of sensor 1
     readHoldingRegisterValues();
    delay(500);
     //read Baud Rate of sensor 1
     //  readHoldingRegisterValues2();
     //     delay(500);
     //read Parity of sensor 1
     //    readHoldingRegisterValues3();
     //      delay(500);
     //read Interval of sensor 1                              
     //      delay(500);
     //read Moisture value of sensor 11                               
     readInputRegisterValues(11);
     delay(500);
     //read temperature of sensor 11
     readInputRegisterValuesT(11);
     //delay(500);
     //read Moisture value of sensor 1
    // readInputRegisterValuesT(1);
      delay(500);
     //read temperature of sensor 1
   //   readInputRegisterValues(1);
     delay(500);
     Watchdog.reset();
     //read Moisture value of sensor 12
     readInputRegisterValues(12);
     delay(500);
     //read temperature of sensor 12
     readInputRegisterValuesT(12);
     delay(500);
     readInputRegisterValues(13);
     delay(500);
     readInputRegisterValuesT(13);
     delay(500);
     readInputRegisterValues(14);
     delay(500);
     Watchdog.reset();
     readInputRegisterValuesT(14);
     delay(500);
     readInputRegisterValues(15);
     delay(500);
     readInputRegisterValuesT(15);
     delay(500);
     readInputRegisterValues(16);
     delay(500);
     readInputRegisterValuesT(16);
     delay(500);
     Watchdog.reset();
    }
    
void tenMinuteCheck()
   {
        Serial.print("tenMinuteCheck- this is currentMillis: ");
        Serial.println(currentMillis); 
        Serial.println(); 
        if(currentMillis - tenMinutePreviousMillis >= tenMinutes)
        {
          tenMinutePreviousMillis=currentMillis; 
          sendStatusStream();
          readSensors();
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
      Serial.println(currentMillis); 
      if(currentMillis - previousMillis >= oneHourInterval)
       {
         Serial.print("this is currentMillis: ");
         Serial.print(currentMillis); 
         Serial.print("this is previousMillis: ");
         Serial.println(previousMillis);      
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
     else    // pings google on start up
     {
          pingTimedGoogle();
          previousMillisGoogle = currentMillis;
     }
   }
   
void pingTimedGoogle()
   {
     Watchdog.reset();
     Serial.println("ping average");
     pingG();
       pingAverage = pingResult; 
    
     Watchdog.reset();
  
     Serial.print("Average ping time to Google: ");
     Serial.println(pingAverage);
    }

void checkMoisture(int x)
   {
  
     //read Moisture value of sensor 11
     // for(int x = 11;x<17;x++)
     // {
     readInputRegisterValues(x);
     switch (x) 
          {
            currentMillis = millis();
           
             case 11:
                if(soilMoisture11<moistureThreshold and soilMoisture11>0)
                  {
                    if(currentMillis < pumpOn)
                    {    
                
                      digitalWrite(relay_1, LOW);   
                      Serial.println("less than 350 ");       
                       }else
                       { 
                         digitalWrite(relay_1, HIGH); 
                          // pumpTimer_11= currentMillis;
                            pumpOn = currentMillis + pumpTimer;
                       }
           }
                  else 
                     {
                    
                     
                              digitalWrite(relay_1, HIGH);   
                              Serial.println("soilMoisture_11 is watered");  
                          
                      }
                  break;
             case 12:
                if(soilMoisture12<moistureThreshold and soilMoisture12>0)
                {
                   digitalWrite(relay_2, LOW);   
                   Serial.println("less than 350 "); 
                      pumpTimer_12 = millis();    
                }
                else 
                   {
                        if(soilMoisture12>highWaterThreshold)
                      {
                         digitalWrite(relay_2, HIGH);   
                         Serial.println("watered to 450 ");  
                      }
                    }
                break;
              case 13:
                 if(soilMoisture13<moistureThreshold  and soilMoisture13>0)
                 {
                   digitalWrite(relay_3, LOW);   
                   Serial.println("less than 350 ");    
                      pumpTimer_13 = millis(); 
                 }
                 else 
                    {
                
                       if(soilMoisture13>highWaterThreshold)
                       {
                         digitalWrite(relay_3, HIGH);   
                         Serial.println("watered to 450 ");  
                        }
                      }  
                 break;
              case 14:
                 if(soilMoisture14<moistureThreshold and soilMoisture14>0)
                 {
                   digitalWrite(relay_4, LOW);   
                   Serial.println("less than 350 ");   
                      pumpTimer_14 = millis();  
                 }
                 else 
                    {
                 
                       if(soilMoisture14>highWaterThreshold)
                       {
                         digitalWrite(relay_4, HIGH);   
                         Serial.println("watered to 450 ");  
                       }
                 break;
              case 15:
                 if(soilMoisture15<moistureThreshold and soilMoisture15>0)
                 {
                   digitalWrite(relay_5, LOW);   
                   Serial.println("less than 350 ");  
                      pumpTimer_15 = millis();   
                 }
                 else 
                 {
                
                    if(soilMoisture15>highWaterThreshold){
                         digitalWrite(relay_5, HIGH);   
                         Serial.println("watered to 450 ");  
                    } 
                 }
                break;
              case 16:
                             if(soilMoisture16<moistureThreshold and soilMoisture16>0)
                   {
                     digitalWrite(relay_6, LOW);   
                     Serial.println("less than 350 ");  
                        pumpTimer_16 = millis();   
                   }
                   else 
                   {
                 
                      if(soilMoisture16>highWaterThreshold){
                           digitalWrite(relay_6, HIGH);   
                           Serial.println("watered to 450 ");  
                      } 
                   }
                   break; 
                default:
                  Serial.print("default switch statement for moisture reached");
                  break;   
            }    
     }
 
}

boolean checkTempStatus()
    {
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
void readHoldingRegisterValues() 
   {
       Serial.println("Reading ID value 1 ... ");
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
          Serial.print("moisture for ");
          Serial.print(x);
          Serial.print("is: ");
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
  
// READ Temp for ID X
void readInputRegisterValuesT(int x) 
   {
     Serial.print("Reading ID ");
     Serial.print(x);
     Serial.print(", Register 1, TEMP value... ");
     // read 1 input value from (slave) ID X for temp
     if (!ModbusRTUClient.requestFrom(x, INPUT_REGISTERS, 0x01,1)) 
      {
        Serial.print("failed to connect ");
        Serial.println(ModbusRTUClient.lastError());
      } 
      else 
        {
            Serial.print("temp for ");
            Serial.print(x);
            Serial.print(" is: ");
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
   {      
       //get time
       // sendNTPpacket(timeServer); // send an NTP packet to a time server
      timeClient.update();
      
       Serial.print("time client = ");
       Serial.println(timeClient.getEpochTime());
        epoch = timeClient.getEpochTime(); 
        if (wifiClient.connect("api.carriots.com",80)) 
          {   // If there's a successful connection
             Serial.println("");
             Serial.println("connected to api.carriots");
             // Build the data field
             rssi = WiFi.RSSI();
             rssi = 100-(-1*rssi);
             
             String json = "{\"protocol\":\"v2\",\"device\":\""+DEVICE+
             "\",\"at\":"+epoch+
             ",\"data\":{\"WIFI_RSSI\":\""+rssi+
             "\",\"Average_ping_time_to_Google\":\""+pingAverage+
             "\",\"soilMoisture_11\":\""+soilMoisture11+
             "\",\"soilMoisture_12\":\""+soilMoisture12+
             "\",\"soilMoisture_13\":\""+soilMoisture13+
             "\",\"soilMoisture_14\":\""+soilMoisture14+
             "\",\"soilMoisture_15\":\""+soilMoisture15+
             "\",\"soilMoisture_16\":\""+soilMoisture16+
             "\",\"soilTemp_11F\":\""+soilTemp11F+
             "\",\"soilTemp_12F\":\""+soilTemp12F+
             "\",\"soilTemp_13F\":\""+soilTemp13F+
             "\",\"soilTemp_14F\":\""+soilTemp14F+
             "\",\"soilTemp_15F\":\""+soilTemp15F+
             "\",\"soilTemp_16F\":\""+soilTemp16F+
             "\",\"Controller_Temp\":\""+temp_F+
             "\",\"Controller_Humidity\":\""+val+
             "\"}}";
            
             Serial.println(json);
             // Make a HTTP request
             wifiClient.println("POST /streams HTTP/1.1");
             wifiClient.println("Host: api.carriots.com");
             wifiClient.println("Accept: application/json");
             wifiClient.println("User-Agent: Arduino-Carriots");
             wifiClient.println("Content-Type: application/json"); 
             wifiClient.print("carriots.apikey: ");
             wifiClient.println(APIKEY);
             wifiClient.print("Content-Length: ");
             int thisLength = json.length();
             wifiClient.println(thisLength);
             wifiClient.println("Connection: close");
             wifiClient.println();
             wifiClient.println(json);
             wifiClient.stop();
        }
             else 
               {
                  // If you didn't get a connection to the server:
                  Serial.println("sendStream connection failed");                                                                        
                  while (WL_CONNECTION_LOST) 
                  {
                      // wifiClient.stop();
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
      timeClient.update();
       Serial.print("time client = ");
       Serial.println(timeClient.getFormattedTime());
       epoch =  timeClient.getEpochTime();
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
             wifiClient.println(json);
             wifiClient.stop();
        }
        else 
           {
                // If you didn't get a connection to the server:
                Serial.println("sendStream connection failed");                                                                        
                while (status != WL_CONNECTED)
                {
                  wifiClient.stop();
                  Serial.print("Attempting to connect to WPA SSID: ");
                  Serial.println(ssid);
                  // unsuccessful, retry in 10 seconds
                  status = WiFi.begin(ssid, pass);
                  delay(10000);
                }
            }
   }


  
