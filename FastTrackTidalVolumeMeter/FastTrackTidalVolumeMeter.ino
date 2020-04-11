/*
Copyright (c) 2020 Mechatroniks
This software is licensed under the CERN CERN Open Hardware Licence Version 2 - Permissive. 
Source: github.com/mechatroniks-git
*/

#include <Wire.h>  
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <Time.h>
#include "SSD1306Wire.h"
#include "ESPAsyncWebServer.h"
#include "esp32config.h"


HardwareSerial hwSerial(1); 

SSD1306Wire display(0x3c, esp32I2CSDA, esp32I2CSCL);  // i2c # 1

extern TwoWire Wire1; // i2c #2

// Replace with your network credentials
const char* ssid     = "tidalWave";
const char* password = "";
double lastTidalVolume=0.0;
String sTidalVolume;
String ipAddress;
  
// Set web server port number to 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta http-equiv="refresh" content="2">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.13.0/css/all.css">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>TidalVolume</h2>
  <p>
    <i class="fas fa-lungs" style="color:#059e8a;"></i> 
    <span class="tv-labels">TidalVolume</span> 
    <span id="temperature">%TIDALVOLUME%</span>
    <sup class="units">mL</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("tidalvolume").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/tidalvolume", true);
  xhttp.send();
}, 10000 ) ;

</script>
</html>)rawliteral";

String readTidalVolume() {
  return String(lastTidalVolume);
}

String readTimeElapsed() {
  return String(lastTidalVolume);
}


String toString(const IPAddress& address){
  return String(address[0]) + "." + address[1] + "." + address[2] + "." + address[3];
}

// Replaces placeholder with TidalVolume values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TIDALVOLUME"){
    return readTidalVolume();
  }
  return String();
}

void setupOled(){
  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth()/2, display.getHeight()/2, "TidalVolume");
  display.display();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void setupSerial() {
  Serial.begin(115200);
  Serial.println("FastTrackTidalFlow");
  hwSerial.begin(115200, SERIAL_8N1, esp32Uart2Rx, esp32Uart2Tx);
  hwSerial.println("FastTrackTidalFlow");
}

void setupFlowSensor() {
  pinMode(SDA_2,INPUT_PULLUP); // SDA
  pinMode(SCL_2,INPUT_PULLUP); // SCL
  Wire1.begin(SDA_2, SCL_2, 100000); //begin(int sdaPin, int sclPin, uint32_t frequency)

  delay(500);

  Wire1.beginTransmission(byte(0x40)); // transmit to device #064 (0x40)
  Wire1.write(byte(0x10));      //
  Wire1.write(byte(0x00));      //
  Wire1.endTransmission(); 

  delay(5);

  Wire1.requestFrom(0x40, 3); //
  int a = Wire1.read(); // first received byte stored here
  int b = Wire1.read(); // second received byte stored here
  int c = Wire1.read(); // third received byte stored here
  Wire1.endTransmission();
  hwSerial.print(a);
  hwSerial.print(b);
  hwSerial.println(c);

  delay(5);
 
  Wire1.requestFrom(0x40, 3); //
  a = Wire1.read(); // first received byte stored here
  b = Wire1.read(); // second received byte stored here
  c = Wire1.read(); // third received byte stored here
  Wire1.endTransmission();
  hwSerial.print(a);
  hwSerial.print(b);
  hwSerial.println(c);

  delay(5);
}

void setupWifi(){
  hwSerial.print("Connecting to ");
  hwSerial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    hwSerial.print(".");
  }
  // Print local IP address and start web server
  hwSerial.println("");
  hwSerial.println("WiFi connected.");
  hwSerial.println("IP address: ");
  ipAddress = toString(WiFi.localIP());
  //hwSerial.println(WiFi.localIP());
  hwSerial.println(ipAddress);
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/tidalvolume", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readTidalVolume().c_str());
  });
  // Start server
  server.begin();
  hwSerial.println("Server started");
}

void setup() {
  setupOled();
  setupSerial();
  setupFlowSensor();  
  setupWifi();
}

//CRC
#define POLYNOMIAL 0x131 //P(x)=x^8+x^5+x^4+1 = 100110001

uint8_t crc8(uint8_t data[], uint8_t nbrOfBytes)  {
  uint8_t crc = 0;
  uint8_t byteCtr;
  //calculates 8-Bit checksum with given polynomial
  for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr) { 
    crc ^= (data[byteCtr]);
    for (uint8_t bit = 8; bit > 0; --bit) { 
      if (crc & 0x80) {
        crc = (crc << 1) ^ POLYNOMIAL;
      }
      else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
} 

double getFlow() {
  uint8_t iicData[3];
  uint8_t mycrc;
  uint16_t measuredValue;
  uint16_t offsetFlow = 32768; // Offset for the sensor
  double Flow;
  double scaleFactorFlow = 120.0; // Scale factor for Air and N2 is 120.0
  double calibrationOffset = 0.0;    // device 1
  //double calibrationOffset = 0.07; // device 0
  Wire1.requestFrom(0x40, 3); // read 3 bytes from device with address 0x40
  iicData[0] = Wire1.read(); // first received byte stored here. The variable "uint16_t" can hold 2 bytes, this will be relevant later
  iicData[1] = Wire1.read(); // second received byte stored here
  measuredValue = (iicData[0] * 0x100) + iicData[1];
  iicData[2] = Wire1.read(); // crc value stored here
  if ( (iicData[0] == 0xff) && (iicData[1] == 0xff) && (iicData[2] = 0xff) ) {
    // invalid data
    return(0);
  }
  else {
    mycrc = crc8(iicData, 2);  // calc CRC
    if (mycrc != iicData[2]) { // check if the calculated and the received CRC byte matches
      hwSerial.print("CRC error | ");
      hwSerial.print(iicData[0], HEX);
      hwSerial.print(" | ");
      hwSerial.print(iicData[1], HEX);
      hwSerial.print(" | ");
      hwSerial.print(iicData[2], HEX);
      hwSerial.print(" > ");
      hwSerial.println(mycrc, HEX);    
      Flow = 0.00;
    }
    else {
      Flow = -(((double)(measuredValue - offsetFlow) / scaleFactorFlow) + calibrationOffset); // flow in liters/min
    }
    return(Flow);
  }
}

void updateDisplay(double elapsedSeconds) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "TVol = " + sTidalVolume +" mL");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 25, "Last = " + String(elapsedSeconds) +" sec");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 45, ipAddress );
  display.display();
}

void loop() {
  String currentLine = "";
  int skip = 0; 
  double flow = 0.0;
  double lastFlow =0.0;
  String sFlow;
  uint32_t nowUs = micros();
  uint32_t lastUs = 0;
  uint32_t elapsedUs = 0; 
  uint32_t startUs = 0;
  double tidalVolume = 0.0;
  uint32_t lastMilliSec = 0;
  const double flowTrigger = .3;
  const double conversionFactor = 60000;
  const int  skipCount = 50;
  const double minTidalVolume = 10.0;
  
  while (true) {
    flow = getFlow();
    lastUs = micros();
    if ((flow + lastFlow)/2 > flowTrigger) {
      tidalVolume = 0.0;
      startUs = lastUs;
      while ((flow + lastFlow)/2 > flowTrigger) {
        flow = getFlow();
        nowUs = micros();
        elapsedUs = nowUs - lastUs;
        tidalVolume = tidalVolume + ( elapsedUs * (flow + lastFlow)/2);
        skip++;
        if (++skip > skipCount) {
          Serial.print(flow); // print the calculated flow to the serial interface
          Serial.print(' ');
          Serial.println(tidalVolume/conversionFactor);
          skip = 0;
        }
        //Serial.print(' ');
        //hwSerial.print(flow);
        //hwSerial.print("\t");
        //hwSerial.println(nowUs);
        lastUs = nowUs;
        lastFlow = flow;
        delay(1);
      }
      tidalVolume = tidalVolume / conversionFactor; // volume in mL (factor out microsec & minutes)
      if (tidalVolume > minTidalVolume) {
        lastTidalVolume = tidalVolume;
        lastMilliSec = millis();
        hwSerial.print("Duration = ");
        hwSerial.println((double)(lastUs - startUs)/1000000);
        hwSerial.print("TidalVolume = ");
        hwSerial.println(lastTidalVolume);
        sTidalVolume = String(tidalVolume,0);
        updateDisplay(double(millis() - lastMilliSec)/1000.0);
      }
    } 
    else {
      skip++;
      if (++skip > skipCount) {
        Serial.print(flow); // print the calculated flow to the serial interface
        Serial.print(' ');
        Serial.println('0');
 
        skip = 0;
        updateDisplay(double(millis() - lastMilliSec)/1000.0);
      }
      sFlow = String(flow, 1);
      //hwSerial.println(sFlow);
      delay(1);
    }  
    lastFlow = flow;
  }
}
