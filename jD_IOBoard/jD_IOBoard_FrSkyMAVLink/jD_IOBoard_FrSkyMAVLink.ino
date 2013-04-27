/*

 Copyright (c) 2012.  All rights reserved.
 An Open Source Arduino based jD_IOBoard driver for MAVLink
 
 Program  : jD_IOBoard
 Version  : V1.0, June 06 2012
 Author(s): Jani Hirvinen
 Coauthor(s):
   Sandro Beningo  (MAVLink routines)
   Mike Smith      (BetterStream and Fast Serial libraries)
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>
 
*/
 
/*
//////////////////////////////////////////////////////////////////////////
//  Description: 
// 
//  This is an Arduino sketch on how to use jD-IOBoard LED Driver board
//  that listens MAVLink commands and changes patterns accordingly.
//
//  If you use, redistribute this please mention original source.
//
//  jD-IOBoard pinouts
//
//             S M M G       R T R
//         5 5 C O I N D D D X X S
//         V V K S S D 7 6 5 1 1 T
//         | | | | | | | | | | | |
//      +----------------------------+
//      |O O O O O O O O O O O O O   |
// O1 - |O O   | | |                O| _ DTS 
// O2 - |O O   3 2 1                O| - RX  F
// O3 - |O O   1 1 1                O| - TX  T
// O4 - |O O   D D D                O| - 5V  D
// O5 - |O O                        O| _ CTS I
// O6 - |O O O O O O O O   O O O O  O| - GND
//      +----------------------------+
//       |   | | | | | |   | | | |
//       C   G 5 A A A A   S S 5 G
//       O   N V 0 1 2 3   D C V N
//       M   D             A L   D
//
// More information, check http://www.jdrones.com/jDoc
//
/* **************************************************************************** */
 
/* ************************************************************ */
/* **************** MAIN PROGRAM - MODULES ******************** */
/* ************************************************************ */

#undef PROGMEM 
#define PROGMEM __attribute__(( section(".progmem.data") )) 

#undef PSTR 
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];})) 

#define MAVLINK10     // Are we listening MAVLink 1.0 or 0.9   (0.9 is obsolete now)
#define HEARTBEAT     // HeartBeat signal
#define FRSER         // FrSky serial output, cannot be run same time with SERDB
#define ONOFFSW       // Do we have OnOff switch connected in pins 
#define membug

/* **********************************************/
/* ***************** INCLUDES *******************/

//#define membug 
//#define FORCEINIT  // You should never use this unless you know what you are doing 


// AVR Includes
#include <FastSerial.h>
#include <AP_Common.h>
#include <AP_Math.h>
#include <math.h>
#include <inttypes.h>
#include <avr/pgmspace.h>

// Get the common arduino functions
#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "wiring.h"
#endif
#include <EEPROM.h>
#include <SimpleTimer.h>
#include <GCS_MAVLink.h>


#ifdef membug
#include <MemoryFree.h>
#endif

#include <SoftwareSerial.h>

// Configurations
#include "IOBoard.h"
#include "IOEEPROM.h"

#define CHKVER 40
#define NEWPAT

/* *************************************************/
/* ***************** DEFINITIONS *******************/

#define VER "v2.1"

// These are not in real use, just for reference
//#define O1 8      // High power Output 1
//#define O2 9      // High power Output 2, PWM
//#define O3 10     // High power Output 3, PWM
//#define O4 4      // High power Output 4 
//#define O5 3      // High power Output 5, PWM
//#define O6 2      // High power Output 6

#define Circle_Dly 1000

//#define ledPin 13     // Heartbeat LED if any
#define LOOPTIME  50  // Main loop time for heartbeat

#define TELEMETRY_SPEED  57600  // How fast our MAVLink telemetry is coming to Serial port

#ifdef FRSER
#define FRSER_SPEED  9600
#endif

/* Patterns and other variables */
static byte LeRiPatt = NOMAVLINK; // default pattern is full ON

static long p_preMillis;
static long p_curMillis;
static int  p_delMillis = LOOPTIME;

static int curPwm;
static int prePwm;

int messageCounter;
static bool mavlink_active;
byte hbStatus;

byte voltAlarm;  // Alarm holder for internal voltage alarms, trigger 4 volts

float boardVoltage;
int i2cErrorCount;

byte ledState;
byte baseState;  // Bit mask for different basic output LEDs like so called Left/Right 

byte ANA;

// Objects and Serial definitions
FastSerialPort0(Serial);

SimpleTimer  mavlinkTimer;

#ifdef FRSER
SoftwareSerial frSerial(6,5,true);
#endif

/* **********************************************/
/* ***************** SETUP() *******************/

void setup() 
{

  // Initialize Serial port, speed
  Serial.begin(TELEMETRY_SPEED);
  
#ifdef FRSER
  frSerial.begin(FRSER_SPEED);
#endif

  if(digitalRead(11) == 0) {    
    writeFactorySettings();
  }

  // Check that EEPROM has initial settings, if not write them
  if(readEEPROM(CHK1) + readEEPROM(CHK2) != CHKVER) {
    // Write factory settings on EEPROM
    writeFactorySettings();
  }
    
  // Rear most important values from EEPROM to their variables  
  LEFT = readEEPROM(LEFT_IO_ADDR);
  RIGHT = readEEPROM(RIGHT_IO_ADDR);
  FRONT = readEEPROM(FRONT_IO_ADDR);
  REAR = readEEPROM(REAR_IO_ADDR);
  ledPin = readEEPROM(LEDPIN_IO_ADDR);
    
  // setup mavlink port
  mavlink_comm_0_port = &Serial;

  // Initializing output pins
  for(int looper = 0; looper <= 5; looper++) {
    pinMode(Out[looper],OUTPUT);
  }

  // Initial 
  for(int loopy = 0; loopy <= 5; loopy++) {
   SlowRoll(25); 
  }

  for(int loopy = 0; loopy <= 2 ; loopy++) {
    AllOn();
    delay(100);
    AllOff();
    delay(100);
  }

  // Activate Left/Right lights
  updateBase();

  // Jani's debug stuff  
#ifdef membug
  Serial.print("Freemem: ");
  Serial.println(freeMem());
#endif

  // Startup MAVLink timers, 50ms runs
  // this affects pattern speeds too.
  mavlinkTimer.Set(&OnMavlinkTimer, 50);

  // House cleaning, enable timers
  mavlinkTimer.Enable();
  
  // Enable MAV rate request, yes always enable it for in case.   
  // if MAVLink flows correctly, this flag will be changed to DIS
  enable_mav_request = EN;  
  
  // for now we are always active, maybe in future there will be some
  // additional features like light conditions that changes it.
  isActive = EN;  
  
  
} // END of setup();



/* ***********************************************/
/* ***************** MAIN LOOP *******************/

// The thing that goes around and around and around for ethernity...
// MainLoop()
void loop() 
{

#ifdef HEARTBEAT
  HeartBeat();   // Update heartbeat LED on pin = ledPin (usually D13)
#endif

  if(isActive) { // main loop
    p_curMillis = millis();
    if(p_curMillis - p_preMillis > p_delMillis) {
      // save the last time you blinked the LED 
      p_preMillis = p_curMillis;   

      // First we update pattern positions 
      patt_pos++;
      if(patt_pos == 16) patt_pos = 0;
    }

    // Update base lights if any
    updateBase();

    if(enable_mav_request == 1) {
      // LEFT/RIGHT outputs are HIGH
      digitalWrite(LEFT, EN);
      digitalWrite(RIGHT, EN);
      
      for(int n = 0; n < 3; n++) {
        request_mavlink_rates();   //Three times to certify it will be readed
        delay(50);
      }
      enable_mav_request = 0;

      // 2 second delay, during delay we still update PWM output
      for(int loopy = 0; loopy <= 2000; loopy++) {
        delay(1);
        updatePWM();
      }
      waitingMAVBeats = 0;
      lastMAVBeat = millis();    // Preventing error from delay sensing
    }  
 
    // Request rates again on every 10th check if mavlink is still dead.
    if(!mavlink_active && messageCounter == 10) {
      enable_mav_request = 1;
      messageCounter = 0;
      LeRiPatt = 6;
    } 
    
    read_mavlink();
    mavlinkTimer.Run();

    updatePWM(); 
    update_FrSky();
  } else {
    AllOff();
  }


}

/* *********************************************** */
/* ******** functions used in main loop() ******** */

// Function that is called every 120ms
void OnMavlinkTimer()
{
  if(millis() < (lastMAVBeat + 3000)) {
           
    // General condition checks starts from here
    //
      
    // Checks that we handle only if MAVLink is active
    if(mavlink_active) {
      if(iob_fix_type <= 2) LeRiPatt = ALLOK;
      if(iob_fix_type >= 3) LeRiPatt = ALLOK;
  
      // CPU board voltage alarm  
      if(voltAlarm) {
        LeRiPatt = LOWVOLTAGE;  
      }     
    }
        
    // If we are armed, run patterns on read output
    if(isArmed) RunPattern();
     else ClearPattern();
    
    // Update base LEDs  
    updateBase();
  
    if(messageCounter >= 3 && mavlink_active) {
      mavlink_active = 0;
      messageCounter = 0;
      LeRiPatt = NOMAVLINK;
    }
  
  // End of OnMavlinkTimer
  } else {
    waitingMAVBeats = 1;
    LeRiPatt = NOMAVLINK;
  }
}



