/**************************************************************************

    home_Antitheft

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
uint8_t ip_address[4]  = {192, 168, 3, 88};
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
#define	KEY					IN1
#define	BALCONY_SENSOR		IN2
#define	PIR_INDOOR			IN3
#define	PIR_OUTDOOR			IN4
#define	ARMED				RELAY1
#define	ACTIVATED			RELAY2
#define	HORN_MUTE			RELAY3	// Mute the internal horn, wire as NC

#define	ANTITHEFT			0
#define	ENABLE_BALCONY		1
#define	ENABLE_INDOOR		2
#define	ENABLE_OUTDOOR		3
#define INPUT_STATES		4
#define MORNING_OFF			5
#define LOGGING				6

#define NEXT_AVAILABLE_SLOT	7

// Define broadcast alarms
#define	WindStorm   0xFF01,0x01
#define	SunTime	    0xFF01,0x02
#define	AutoClose   0xFF01,0x03
#define	AutoRestart 0xFF01,0x03

/* ######################################## */

// Set Telegram BOT ID
#include "myTelegramBot.h"

// Initialize Telegram BOT
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

/* ######################################## */

// Include libraries for NTP synch
#include <TimeLib.h>
#include <NtpClientLib.h>
#define MORNING_OFF_TIME	5

time_t 	armtime=0;
time_t  actualtime=0;
uint8_t armdelay=0;
uint8_t armonoff=0;
#define ARM_DELAY	50

uint8_t sensortrig=0;

// Save the public ip address and publish on Telegram
String ip;

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
    SetAsGateway(myvNet_address);   // Set this node as gateway for SoulissApp       

	// Set address on MEDIA3
	SetAddress(m3_NODE_1, m3_SUB, m3_GTW);
	SetAsPeerNode(m3_NODE_2, 1);
	SetAsPeerNode(m3_NODE_3, 2);
	SetAsPeerNode(m3_NODE_4, 3);
	SetAsPeerNode(m3_NODE_5, 4);

	// Define logic typicals
    Set_T41(ANTITHEFT);
	Set_T11(ENABLE_BALCONY);
	Set_T11(ENABLE_INDOOR);
	Set_T11(ENABLE_OUTDOOR);
	Set_T1A(INPUT_STATES);
	Set_T11(MORNING_OFF);
	Set_T11(LOGGING);
	mInput(ENABLE_BALCONY) =Souliss_T1n_OnCmd;
	mInput(ENABLE_INDOOR)  =Souliss_T1n_OnCmd;
	mInput(ENABLE_OUTDOOR) =Souliss_T1n_OffCmd;
	mInput(MORNING_OFF)    =Souliss_T1n_OffCmd;
	mInput(LOGGING)        =Souliss_T1n_OffCmd;

	// Define I/O
	pinMode(IN1, INPUT);
	pinMode(IN2, INPUT);
	pinMode(IN3, INPUT);
	pinMode(IN4, INPUT);
	pinMode(RELAY1, OUTPUT);
	pinMode(RELAY2, OUTPUT);
	pinMode(RELAY3, OUTPUT);
	pinMode(RELAY4, OUTPUT);

	// Set NTP
	NTP.begin("192.168.3.1", 1, true);
	NTP.setInterval(4000,4000);

    // Init the OTA
    ArduinoOTA.setHostname("home_Antitheft");    
    ArduinoOTA.begin();

	// Welcome message
	bot.sendMessage(group_id, "home_Antitheft avviato/riavviato", "");  
}

/***************************/
/* LOOP */
/***************************/

void loop()
{ 
    // Here we start to play
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        // Process every 510ms the logic
        SHIFT_510ms(1) {
            // Arm the antitheft
            if(DigIn(KEY, Souliss_T4n_Armed, 0)) {
				if(mOutput(ANTITHEFT)==(Souliss_T4n_NoAntitheft))
					mInput(ANTITHEFT)=Souliss_T4n_Armed;
				else if (mOutput(ANTITHEFT)==(Souliss_T4n_InAlarm))
					mInput(ANTITHEFT)=Souliss_T4n_ReArm;
				else
					mInput(ANTITHEFT)=Souliss_T4n_NotArmed;
			}
            // Run the logic
            if(Logic_T41(ANTITHEFT))
			{
				// The first cycle in alarm, send an alarm message
				if(mOutput(ANTITHEFT)==Souliss_T4n_InAlarm) {
					String alarm_string = "Antifurto in allarme. ";
					if(sensortrig & 0x01) alarm_string += "Sensore balcone. ";
					if(sensortrig & 0x02) alarm_string += "Sensore PIR interno. ";
					if(sensortrig & 0x04) alarm_string += "Sensore PIR esterno. ";

					// Send the alarm notification
					bot.sendMessage(group_id, alarm_string, "");
					sensortrig = 0;
				}
			}

			// Identify a trig NoAntitheft->Antitheft to run the delay activation
			if((mOutput(ANTITHEFT)==Souliss_T4n_NoAntitheft) || (mOutput(ANTITHEFT)==Souliss_T4n_InReArm)) {
				armonoff=1;
				armdelay=0;
			}
			else if((mOutput(ANTITHEFT)==Souliss_T4n_Antitheft) && (armonoff==1)) {
				armonoff=2;

				// Record the last activation/deactivation time
				armtime = NTP.getTime();
				armdelay= 1;

				// Activate/deactivate the zone based on time
				if(((hour(armtime)*60+minute(armtime))>=(22*60)) || ((hour(armtime)*60+minute(armtime))<=(5*60))) // Between 22:00 and 5:00
				{
					mInput(ENABLE_BALCONY) =Souliss_T1n_OnCmd;
					mInput(ENABLE_INDOOR)  =Souliss_T1n_OffCmd;
					mInput(ENABLE_OUTDOOR) =Souliss_T1n_OffCmd;

					// If enabled in the night, auto disable in the morning
					mInput(MORNING_OFF) =Souliss_T1n_OnCmd;					
				}
				else											// From 5:00 to 22::00
				{
					mInput(ENABLE_BALCONY) =Souliss_T1n_OnCmd;
					mInput(ENABLE_INDOOR)  =Souliss_T1n_OnCmd;
					mInput(ENABLE_OUTDOOR) =Souliss_T1n_OffCmd;	
				}
			}

            // Control the output relays
            LowDigOut(ACTIVATED, Souliss_T4n_InAlarm, ANTITHEFT); 
            nDigOut(ARMED, Souliss_T4n_Antitheft, ANTITHEFT);            
        }

		// Enable/disable sensors
        SHIFT_510ms(2) {
            Logic_T11(ENABLE_BALCONY);
            Logic_T11(ENABLE_INDOOR);
            Logic_T11(ENABLE_OUTDOOR);

            Logic_T11(MORNING_OFF);
            Logic_T11(LOGGING);
		}

		// Process inputs from sensors
        SHIFT_510ms(4) {
			if(mOutput(ANTITHEFT) && !armdelay)		// If the armdelay is set, skip this
			{
				if(mOutput(ENABLE_BALCONY)) {
					if(LowDigIn(BALCONY_SENSOR, Souliss_T4n_Alarm, ANTITHEFT)) {
						sensortrig |= 0x01;
						if(mOutput(LOGGING))
							bot.sendMessage(group_id, "Sensore balcone.", "");
					}
				}
				if(mOutput(ENABLE_INDOOR))  {
					if(LowDigIn(PIR_INDOOR, Souliss_T4n_AlarmDelay, ANTITHEFT)) {
						sensortrig |= 0x02;
						if(mOutput(LOGGING))
							bot.sendMessage(group_id, "Sensore PIR interno.", "");
					}
				}
				if(mOutput(ENABLE_OUTDOOR))	{
					if(LowDigIn(PIR_OUTDOOR, Souliss_T4n_Alarm, ANTITHEFT)) {
						sensortrig |= 0x04;
						if(mOutput(LOGGING))
							bot.sendMessage(group_id, "Sensore PIR esterno.", "");
					}
				}
			}
		}

		// Show input state
        SHIFT_510ms(5) {
            if(dRead(IN1))  (mInput(INPUT_STATES) |= 0x01);
			else			(mInput(INPUT_STATES) &= 0xFE);

            if(dRead(IN2))  (mInput(INPUT_STATES) |= 0x02);
			else			(mInput(INPUT_STATES) &= 0xFD);

            if(dRead(IN3))  (mInput(INPUT_STATES) |= 0x04);
			else			(mInput(INPUT_STATES) &= 0xFB);

            if(dRead(IN4))  (mInput(INPUT_STATES) |= 0x08);
			else			(mInput(INPUT_STATES) &= 0xF7);

			Logic_T1A(INPUT_STATES);
		}

        // Auto close
        FAST_x10ms(51) { 
			if(sbscrb(AutoClose))   bot.sendMessage(group_id, "Allarme: Vento forte, chiusura automatica.", "");                
			if(sbscrb(AutoRestart)) bot.sendMessage(group_id, "Allarme: Vento oltre limite massimo, riavvio di emergenza.", "");
			if(sbscrb(SunTime)) 	bot.sendMessage(group_id, "Notifica: SunTime, apertura tende automatica.", "");
        }

        // Timer to activate
        FAST_x10ms(71) {                 
            Timer_T41(ANTITHEFT);
        }

        // Here we process all communication with other nodes
        FAST_GatewayComms();    
    }

   EXECUTESLOW() { 
        UPDATESLOW();

        SLOW_10s() {  // Process the timer every 10 seconds
			if(armdelay) {
				actualtime = NTP.getTime();

				// Reset the armdelay
				if((actualtime-armtime)>ARM_DELAY)	armdelay=0;
			}
		}

        SLOW_710s() {  // Process the timer every 710 seconds  
			
			if(mOutput(MORNING_OFF)) {
				actualtime = NTP.getTime();
				
				// Disable the antitheft
				if(hour(actualtime)==MORNING_OFF_TIME) {
					mInput(ANTITHEFT)   =Souliss_T4n_NotArmed;
					mInput(MORNING_OFF) =Souliss_T1n_OffCmd;
				}
			}
		}

        SLOW_30m() {  // Process the timer twice per hour  

			// Open a connection to Ipify to get the public IP address
			WiFiClient client;
			if (client.connect("api.ipify.org", 80)) 
			{
				client.println("GET / HTTP/1.0\r\nHost: api.ipify.org\r\n\r\n");
			}
 
			// Retrieve the response string
			String new_ip;
			while(client.available()) new_ip = client.readStringUntil('\n');

			// If the IP address has changed, notify via Telegram
			if (new_ip!=ip) {
				ip=new_ip;
				
				String ipmessage="Indirizzo IP cambiato: "+ip;
				bot.sendMessage(group_id, ipmessage, "");
			}
		}
	}

    // Look for a new sketch to update over the air
    ArduinoOTA.handle();
} 