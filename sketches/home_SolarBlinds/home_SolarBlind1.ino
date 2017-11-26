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
uint8_t ip_address[4]  = {192, 168, 3, 111};
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

#define	BLIND_WIND_IN		GROVE_PIN1
#define	BLIND_WIND			1										// Analog value, use two slots
volatile long interruptCounter=0;									// Measure the speed
long time_window=0;													// Time elapsed between two measures
long bounce=0;														// Time for bounce filtering
uint8_t autovelox=0;												// Number of times the speed is over the limit
#define	C_RS_KMH			1,5										// Conversion rate from revolution/s to km/h
#define SPEED_LIMIT_KMH		20										// Maximum allowed wind speed
#define	SPEED_HIGH			3										// If speed is over times this, should be far from 254.

#define AUTOCLOSE			3

uint8_t windhigh=0;													// This is an emergency mode. If this is set, blinds should close
#define	WINDHIGH_SET		1										// Once set, the device will continuously publish a WindStorm 							
#define WINDHIGH_RESTART	20										// After this number of publishes, the device will recover from the emergency mode

float average_speed=0;
float max_speed=0;
#define AVRSPEED			4
#define	AVRSPEEDNOTIFY		6
#define NEXT_AVAILABLE_SLOT	7

// Define broadcast alarms
#define	WindStorm   0xFF01,0x01
#define	SunTime	    0xFF01,0x02
#define	AutoClose   0xFF01,0x03
#define	AutoRestart 0xFF01,0x03

/*** All configuration includes should be above this line ***/ 
#include "Souliss.h"  

/* Counts the number of revolution */
void handleInterrupt() {
	// If interruptCounter goes too high, the ISR is overloading the MCU
	
	if(interruptCounter>300) {
		// Wind speed is too high, detach and close. This is an emergency mode, to recover
		// restart the device
		detachInterrupt(digitalPinToInterrupt(GROVE_PIN1));
		windhigh=WINDHIGH_SET;
	}
	
	// Bounce and count
	if((millis()-bounce)>50) interruptCounter++;
	bounce=millis();
}

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
	SetAddress(m3_NODE_2, m3_SUB, m3_GTW);

	// Define logic typicals
    Set_T22(BLIND1);
	Set_T51(BLIND_WIND);
	mInput(BLIND1)=Souliss_T2n_CloseCmd;
	
	// Define and enable by default the AUTOCLOSE logic
	Set_T11(AUTOCLOSE);
	mInput(AUTOCLOSE)=Souliss_T1n_OnCmd;

	// Define and enable by default the AVRSPEED logic
	Set_T51(AVRSPEED);
	Set_T11(AVRSPEEDNOTIFY);
	mInput(AVRSPEEDNOTIFY)=Souliss_T1n_OnCmd;

	// Define I/O
	pinMode(IN1, INPUT);
	pinMode(IN2, INPUT);
	pinMode(RELAY1, OUTPUT);
	pinMode(RELAY2, OUTPUT);
	pinMode(GROVE_PIN1, INPUT);
	attachInterrupt(digitalPinToInterrupt(GROVE_PIN1), handleInterrupt, FALLING);


    // Init the OTA
    ArduinoOTA.setHostname("home_SolarBlind1");    
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
            
			// If wind speed is too high, publish an alarm and close the local blind
			if(((autovelox>=SPEED_HIGH) && mOutput(AUTOCLOSE)) || windhigh) {
				if(mOutput(BLIND1)!=Souliss_T2n_State_Close) mInput(BLIND1)=Souliss_T2n_CloseCmd;
				pblsh(WindStorm);

				// Notify automatic closure
				if((autovelox==SPEED_HIGH) && (mOutput(AVRSPEEDNOTIFY) || (mOutput(BLIND1)!=Souliss_T2n_State_Close))) pblsh(AutoClose);

				// This is tricky but it works, increase the autovelox near the rollup value
				// so the publish alarm will be broadcasted more than once.
				// If the time required to rollup is near the next scan of the speed, things
				// may mess up.
				if(autovelox<230) autovelox=250;
				autovelox++;

				// We loop here for a while, then we restart the node to recover from this emergency mode
				if(windhigh) windhigh++;
				if(windhigh>WINDHIGH_RESTART) {
					pblsh(AutoRestart);
					ESP.restart();
				}
			}

            // Run the logic
            Logic_T22(BLIND1);
            
            // Control the output relays, the stop is with both the relays not active.
            // If you need to active the relays on stop, use nDigOut instead.
            DigOut(BLIND1_OPEN_OUT, Souliss_T2n_Coil_Open, BLIND1);    
            DigOut(BLIND1_CLOSE_OUT, Souliss_T2n_Coil_Close, BLIND1);

			// Used as ENABLE/DISABLE of the AUTO CLOSE logic
			Logic_T11(AUTOCLOSE);   

			// Used as ENABLE/DISABLE of the AVRSPEED logic
			Logic_T11(AVRSPEEDNOTIFY);        
        }

        // Auto Open
        FAST_x10ms(51) {                 
			if(sbscrb(SunTime)) {
				if(mOutput(BLIND1)!=Souliss_T2n_State_Open) {
					mInput(BLIND1)=Souliss_T2n_OpenCmd;
					mInput(AUTOCLOSE)=Souliss_T1n_OnCmd;
					mInput(AVRSPEEDNOTIFY)=Souliss_T1n_OnCmd;
					}
			}
        }

        // Timer to stop
        FAST_x10ms(171) {                 
            Timer_T22(BLIND1);
        }
       
        // Here we process all communication with other nodes
        FAST_PeerComms();    
    }
    
    EXECUTESLOW() { 
        UPDATESLOW();

        SLOW_10s() {  // Process the timer every 10 seconds  
		
			// Measure the speed
			long time_window_old = time_window;
			time_window = millis();
			long elapsed_milliseconds=time_window-time_window_old;

			// Get the speed in Km/h
			if(elapsed_milliseconds>0)
			{
				// Convert the speed
				float speed = (float)(C_RS_KMH*1000*interruptCounter/elapsed_milliseconds);
				interruptCounter = 0;

				// Record a moving average and max
				average_speed=(average_speed+speed)/2;
				if(average_speed<0.01) average_speed=0.01; 				//Cut-out
				if(average_speed>max_speed) max_speed=average_speed;

				// Record if speed is over the limit
				if ((speed>SPEED_LIMIT_KMH) && (autovelox<230)) autovelox++;
				else if ((speed<(SPEED_LIMIT_KMH*0.5)) && (autovelox<230) && (autovelox>0)) autovelox--;

				// Report the actual and average speed to SoulissApp
				ImportAnalog(BLIND_WIND, &average_speed);
				Read_T51(BLIND_WIND);
				ImportAnalog(AVRSPEED, &max_speed);
				Read_T51(AVRSPEED);
				
			}
        }

		SLOW_30m() { // Process the timer every 30 minutes

			// Reset the autovelox
			autovelox=0;
			max_speed=0;
		}
    }

    // Look for a new sketch to update over the air
    ArduinoOTA.handle();
} 