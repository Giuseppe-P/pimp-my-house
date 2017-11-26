/**************************************************************************

    home_SolarBlind1

***************************************************************************/

// Let the IDE point to the Souliss framework
#include "SoulissFramework.h"

#include "bconf/DINo_WiFi.h"   				// Define the board type
#include "conf/Gateway.h"                   // The main node is the Gateway, we have just one node
#include "conf/Webhook.h"                   // Enable DHCP and DNS

// Set WiFi SSID and Password
#include "myWifiConf.h"   

/*
#define	VNET_RESETTIME_INSKETCH
#define VNET_RESETTIME 0x000AFC80
#define VNET_HARDRESET ESP.restart()
*/

// Define the network configuration according to your router settings
uint8_t ip_address[4]  = {192, 168, 3, 112};
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
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Define pin and slot
#define	BLIND1_OPEN_IN		IN1
#define	BLIND1_CLOSE_IN		IN2
#define	BLIND1_OPEN_OUT		RELAY1
#define	BLIND1_CLOSE_OUT	RELAY2
#define	BLIND1				0

// Define pin and slot
#define	BLIND2_OPEN_IN		IN3
#define	BLIND2_CLOSE_IN		IN4
#define	BLIND2_OPEN_OUT		RELAY3
#define	BLIND2_CLOSE_OUT	RELAY4
#define	BLIND2				1

uint8_t auto_open=0;
#define AUTOOPEN_TIME		14
#define AUTOOPEN			2

#define NEXT_AVAILABLE_SLOT	3

// Define broadcast alarms
#define	WindStorm   0xFF01,0x01
#define	SunTime	    0xFF01,0x02
#define	AutoClose   0xFF01,0x03
#define	AutoRestart 0xFF01,0x03

// Include libraries for NTP synch
#include <TimeLib.h>
#include <NtpClientLib.h>

/*** All configuration includes should be above this line ***/ 
#include "Souliss.h"  

/***************************/
/* SETUP */
/***************************/
void setup()
{
    // Init the board
    Initialize();
    InitDINo();

     // Set network parameters
    SetIPAddress(ip_address, subnet_mask, ip_gateway);

	// Set address on MEDIA3
	SetAddress(m3_NODE_3, m3_SUB, m3_GTW);

	// Define logic typicals
    Set_T22(BLIND1);
    Set_T22(BLIND2);
	Set_T11(AUTOOPEN);
	mInput(BLIND1)=Souliss_T2n_CloseCmd;
	mInput(BLIND2)=Souliss_T2n_CloseCmd;

	// Define I/O
	pinMode(IN1, INPUT);
	pinMode(IN2, INPUT);
	pinMode(RELAY1, OUTPUT);
	pinMode(RELAY2, OUTPUT);

	pinMode(IN3, INPUT);
	pinMode(IN4, INPUT);
	pinMode(RELAY3, OUTPUT);
	pinMode(RELAY4, OUTPUT);

	// Set NTP
	NTP.begin("192.168.3.1", 1, true);
	NTP.setInterval(4000,4000);

    // Init the OTA
    ArduinoOTA.setHostname("home_SolarBlind2");    
    ArduinoOTA.begin();
}

/***************************/
/* LOOP */
/***************************/

void loop()
{ 
    // Here we start to play
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        // Process every 510ms the logic that control the solar blind
        SHIFT_510ms(1) {
            // Use OPEN and CLOSE Commands
            DigIn(BLIND1_OPEN_IN, Souliss_T2n_OpenCmd_Local, BLIND1);
            DigIn(BLIND1_CLOSE_IN, Souliss_T2n_CloseCmd_Local, BLIND1);

            // Run the logic
            Logic_T22(BLIND1);
            
            // Control the output relays, the stop is with both the relays not active.
            // If you need to active the relays on stop, use nDigOut instead.
            DigOut(BLIND1_OPEN_OUT, Souliss_T2n_Coil_Open, BLIND1);    
            DigOut(BLIND1_CLOSE_OUT, Souliss_T2n_Coil_Close, BLIND1);         
        }

        // Process every 510ms the logic that control the solar blind
        SHIFT_510ms(2) {
            // Use OPEN and CLOSE Commands
            DigIn(BLIND2_OPEN_IN, Souliss_T2n_OpenCmd_Local, BLIND2);
            DigIn(BLIND2_CLOSE_IN, Souliss_T2n_CloseCmd_Local, BLIND2);

            // Run the logic
            Logic_T22(BLIND2);
            
            // Control the output relays, the stop is with both the relays not active.
            // If you need to active the relays on stop, use nDigOut instead.
            DigOut(BLIND2_OPEN_OUT, Souliss_T2n_Coil_Open, BLIND2);    
            DigOut(BLIND2_CLOSE_OUT, Souliss_T2n_Coil_Close, BLIND2);

			// Used as ENABLE/DISABLE of the AUTO OPEN logic
			Logic_T11(AUTOOPEN);
			if(mOutput(AUTOOPEN) && auto_open) {
				if(mOutput(BLIND1)!=Souliss_T2n_State_Open) mInput(BLIND1)=Souliss_T2n_OpenCmd;
				if(mOutput(BLIND2)!=Souliss_T2n_State_Open) mInput(BLIND2)=Souliss_T2n_OpenCmd;
				pblsh(SunTime);
				auto_open--;

				// Once done, deactivate the AUTO OPEN
				if(auto_open==1) mInput(AUTOOPEN)=Souliss_T1n_OffCmd;
			}

        }

        // Auto close
        FAST_x10ms(51) {                 
			if(sbscrb(WindStorm)) {
				if(mOutput(BLIND1)!=Souliss_T2n_State_Close) mInput(BLIND1)=Souliss_T2n_CloseCmd;
				if(mOutput(BLIND2)!=Souliss_T2n_State_Close) mInput(BLIND2)=Souliss_T2n_CloseCmd;
				mInput(AUTOOPEN)=Souliss_T1n_OffCmd;
			}
        }

        // Timer to stop
        FAST_x10ms(151) {                 
            Timer_T22(BLIND1);
            Timer_T22(BLIND2);
        }
        
        // Here we process all communication with other nodes
        FAST_PeerComms();    
    }

   EXECUTESLOW() { 
        UPDATESLOW();

        SLOW_710s() {  // Process the timer every 710 seconds  
			time_t actualtime = NTP.getTime();
			
			// Set automatic open of the solar blinds at 14
			if(hour(actualtime)==AUTOOPEN_TIME) auto_open=3;
			else 					 			auto_open=0;
		}
	}

    // Look for a new sketch to update over the air
    ArduinoOTA.handle();
} 