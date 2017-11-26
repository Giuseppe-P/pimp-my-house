/**************************************************************************
    Home Thermostat - Base example for a Thermostat based on 
    SonOff TH16 board from Itead Studio
        
***************************************************************************/

// Let the IDE point to the Souliss framework
#include "SoulissFramework.h"

// Configure the framework
#include "bconf/MCU_ESP8266.h"              // Load the code directly on the ESP8266
#include "conf/Gateway.h"                   // The main node is the Gateway, we have just one node
#include "conf/IPBroadcast.h"
#include "conf/DynamicAddressing.h"         // Use dynamically assigned addresses

// Set WiFi SSID and Password
#include "myWifiConf.h"   

// Include framework code and libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Define DHT Sensor Libraries
#include "DHT.h"
#define DHTTYPE     DHT21                   // Define the temperature sensor type 
#define DHTPIN      14
DHT dht(DHTPIN, DHTTYPE, 30);

/*** All configuration includes should be above this line ***/ 
#include "Souliss.h"

// Logics (Typicals)
#define TEMPERATURE             0
#define HUMIDITY                2
#define ANALOGDAQ               4

#define DEADBAND                0.05        // Deadband value 5%
#define RELAY                   12
#define LED                     13

// Temperature and humidity
float h=0,t=0;

void setup()
{   
    Initialize();

    // Define output pins
    pinMode(RELAY, OUTPUT);     // Heater (ON/OFF)
    pinMode(LED, OUTPUT);       // Heartbeat
    digitalWrite(LED,LOW);

    // Connect to the WiFi network and get an address from DHCP
    GetIPAddress();                           
    SetAsGateway(myvNet_dhcp);       // Set this node as gateway for SoulissApp  

    // This is the vNet address for this node, used to communicate with other
    // nodes in your Souliss network
    SetAddress(0xAB01, 0xFF00, 0x0000);
    
    // Set node logics         
    Set_Temperature(TEMPERATURE);
    Set_Humidity(HUMIDITY);
    Set_Thermostat(ANALOGDAQ);

    // Default setpoint
    t=18;
    ImportAnalog(ANALOGDAQ+3, &t);

    // Init the OTA
    ArduinoOTA.setHostname("souliss-thermostat");    
    ArduinoOTA.begin();
}

void loop()
{ 
    // Here we start to play
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        FAST_110ms() {  
            // The logic is executed faster than the data acquisition just to have a 
            // prompt answer to the user commands, but the real rate is defined by
            // the temperature measure rate.
        
            // Compare the setpoint from the user interface with the measured temperature
            Logic_Thermostat(ANALOGDAQ);
            
            // Start the heater and the fans
            nDigOut(RELAY, Souliss_T3n_HeatingOn, ANALOGDAQ);   // Heater
            
            // We are not handling the cooling mode, if enabled by the user, force it back
            // to disable
            //if(mOutput(ANALOGDAQ) & Souliss_T3n_CoolingOn)
            //    mOutput(ANALOGDAQ) &= ~Souliss_T3n_CoolingOn;
        }
              
        FAST_9110ms()    {    
            // Turn the LED on for a while
            digitalWrite(LED,LOW);
 
            // Reading temperature or humidity takes about 250 milliseconds!
            h = dht.readHumidity();
            t = dht.readTemperature();
            ImportAnalog(HUMIDITY, &h);
            ImportAnalog(TEMPERATURE, &t);
            ImportAnalog(ANALOGDAQ+1, &t);

            Logic_Humidity(HUMIDITY);
            Logic_Temperature(TEMPERATURE);
        }  
        // Turn the LED off
        digitalWrite(LED,HIGH);  

        // Here we handle here the communication with Android
        FAST_GatewayComms();                                        
    }

    // Look for a new sketch to update over the air
    ArduinoOTA.handle();
} 