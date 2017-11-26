/**************************************************************************
   Souliss - LYT8266 WiFi RGBW LED Bulb
 
***************************************************************************/
// Let the IDE point to the Souliss framework
#include "SoulissFramework.h"

// Include framework code and libraries
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Configure the framework
#include "bconf/LYT8266_LEDBulb.h"              // Load the code directly on the ESP8266
#include "conf/IPBroadcast.h"

// Set WiFi SSID and Password
#include "myWifiConf.h"   

/*
#define	VNET_RESETTIME_INSKETCH
#define VNET_RESETTIME 0x000AFC80
#define VNET_HARDRESET ESP.restart()
*/ 

// Define the network configuration according to your router settings
uint8_t ip_address[4]  = {192, 168, 3, 115};
uint8_t subnet_mask[4] = {255, 255, 255, 0};
uint8_t ip_gateway[4]  = {192, 168, 3, 1};
#define myvNet_address  ip_address[3]

// Define addresses on IPBroadcast MEDIA3
#define m3_NODE_1	0xAB01		// Node that controls the antitheft
#define m3_NODE_2	0xAB02		// Node that controls solar blind  1
#define m3_NODE_3	0xAB03		// Node that controls solar blinds 2 and 3
#define m3_NODE_4	0xAB04		// Node that control bedroom LYT8266
#define m3_NODE_5	0xAB05		// Node that control livingroom LYT8266
#define m3_NODE_6	0xAB06
#define m3_NODE_7	0xAB07
#define m3_NODE_8	0xAB08
#define m3_NODE_9	0xAB09
#define m3_SUB		0xFF00
#define	m3_GTW		0xAB01

// Include framework code and libraries
#include "Souliss.h"
   
// Define logic slots, multicolor lights use four slots
#define LIGHTMODE       0
#define LYTLIGHT1       1    

#define RED_LOWMOOD     0x2A
#define GREEN_LOWMOOD   0x10
#define BLUE_LOWMOOD    0x00

void setup()
{   
    // Init the network stack and the bulb, turn on with a warm amber
    Initialize();
    InitLYT();

	/****
		Generally set a PWM output before the connection will lead the 
		ESP8266 to reboot for a conflict on the FLASH write access.
		Here we do the configuration during the WebConfig and so we don't
		need to write anything in the FLASH, and the module can connect
		to the last used network.
	****/
	LYTWhite(0xFF, 1);
	SetWhite(LYTLIGHT1, 0xFF);

     // Set network parameters
    SetIPAddress(ip_address, subnet_mask, ip_gateway);

	// Set address on MEDIA3
	SetAddress(m3_NODE_5, m3_SUB, m3_GTW);

    // Define a logic to handle the bulb
    Set_T14(LIGHTMODE);
    SetLYTLamps(LYTLIGHT1);

    // Init the OTA
    ArduinoOTA.setHostname("home_livingrLYT");    
    ArduinoOTA.begin();
}

void loop()
{  
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        // Is an unusual approach, but to get fast response to color change we run the LYT logic and
        // basic communication processing at maximum speed.
        LogicLYTLamps(LYTLIGHT1);       
        ProcessCommunication();
      
        FAST_PeerComms(); 

        FAST_510ms() {

            // Set a low light mode
            if(Logic_T14(LIGHTMODE))
					SetColor(LYTLIGHT1, RED_LOWMOOD, GREEN_LOWMOOD, BLUE_LOWMOOD);
        }
    }

    // Look for a new sketch to update over the air
    ArduinoOTA.handle(); 
} 