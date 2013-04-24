/*

 file     : frsky_telemetry_feeder
 version  : v1.0, 24.06.2012
 author   : Jani Hirvinen, jani@jd.....com
 
 Relies on: 
 - jD IOBoard
 - SoftwareSerial
 - FrSky modules rx/tx
 - jD IOBoard, MAVLink code
 
 Connection:
 Connect following cables/pins from IOBoard to FrSky D8R-II or similar telemetry receiver
 
 IOB   RX
 -----------
 GND - GND
 D5  - Rx
 
 Details:
 Program creates and populates FrSky HUB style protocol messages and feeds it out from SoftwareSerial pins on IOBoard.
 SoftwareSerial uses inverted signaling to output data correctly, if signal is non-inverted data will be corrupt due 
 XORing and shifthing one step to right process. 
 
 FrSky uses 3 frames on their HUB protocol
 
 Frame 1, every 200ms,  payload: accel-x, accel-y, accel-z, Altitude(Vario), Temp1, Temp2, Voltage (multiple), RPM
 Frame 2, every 1000ms, payload: course, lat, lon, speed, altitude (GPS), fuel level
 Frame 3, every 5000ms, payload: date, time
 
 */

void update_FrSky() {

  f_curMillis = millis();
  if(f_curMillis - f_preMillis > f_delMillis) {
    // save the last time you sent the messaga 
    f_preMillis = f_curMillis;   

    // 200ms payload, construct Frame 1 on every loop
    packetOpen = TRUE;
    payloadLen += addPayload(0x24); // accel-x
    payloadLen += addPayload(0x25); // accel-y
    payloadLen += addPayload(0x26); // accel-z

    payloadLen += addPayload(0x10); // alt (vario)
    
    payloadLen += addPayload(0x02); // temp1
    payloadLen += addPayload(0x05); // temp2
    
    payloadLen += addPayload(0x06); // battery data, injection not ready

    payloadLen += addPayload(0x28); // Ampere
    
    payloadLen += addPayload(0x3A); // Voltage , before "."
    payloadLen += addPayload(0x3B); // Voltage , after "."

    payloadLen += addPayload(0x03); // rpm
    
    packetOpen = FALSE;
    payloadLen = sendPayload(payloadLen);

    // 1000ms (1s) payload, construct Frame 2 on every 5th loop
    if((msCounter % 5) == 0) {
      second++;
      updateTime();
      packetOpen = TRUE;

      payloadLen += addPayload(0x14);   // Course, degree
      payloadLen += addPayload(0x1c);   // Course, after "."
    
      payloadLen += addPayload(0x13);   // Latitude dddmmm 
      payloadLen += addPayload(0x1b);   // Latitude .mmmm (after ".")
      payloadLen += addPayload(0x23);   // N/S

      payloadLen += addPayload(0x12);   // Longitude dddmmm
      payloadLen += addPayload(0x1a);   // Longitude .mmmm (after ".")
      payloadLen += addPayload(0x22);   // E/W
    
      payloadLen += addPayload(0x11);   // GPS Speed Knots
      payloadLen += addPayload(0x19);   // GPS Speed after "."

      payloadLen += addPayload(0x01);   // GPS Altitude
      payloadLen += addPayload(0x09);   // GPS Altitude "."
    
      payloadLen += addPayload(0x04);   // Fuel level % 0,25,50,75,100
      
      payloadLen += addPayload(0x18);   // secs
    
      packetOpen = FALSE;
      payloadLen = sendPayload(payloadLen);
    }  

    // 5000ms (5s) payload, contruct Frame 3 on every 25th loop and reset counters
    if(msCounter >= 25) {
      packetOpen = TRUE;
      payloadLen += addPayload(0x15);   // date/month      
      payloadLen += addPayload(0x16);   // year      
      
      payloadLen += addPayload(0x17);   // hour/min      
      payloadLen += addPayload(0x18);   // secs     

      packetOpen = FALSE;
      payloadLen = sendPayload(payloadLen);
      msCounter = 0;
    }
    // Update loop counter
    msCounter ++;
  }
}

byte addPayload(byte DataID) {
  
  byte addedLen;
  switch(DataID) {

//OK
    case 0x01:  // GPS Altitude
      outBuff[payloadLen + 0] = 0x01;
      outBuff[payloadLen + 1] = FixInt(int(iob_gps_alt), 1);
      outBuff[payloadLen + 2] = FixInt(int(iob_gps_alt), 2);
      addedLen = 3;      
      break;
    case 0x01+8:  // GPS Altitude
      {
      float tmp = (iob_gps_alt - int(iob_gps_alt)) * 10000.0f;
      outBuff[payloadLen + 0] = 0x01+8;
      outBuff[payloadLen + 1] = FixInt(int(tmp), 1);
      outBuff[payloadLen + 2] = FixInt(int(tmp), 2);
      addedLen = 3;      
      }
      break;
//OK
    case 0x02:  // Temperature 1
      outBuff[payloadLen + 0] = 0x02;
      outBuff[payloadLen + 1] = iob_temperature;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;
//OK
    case 0x03:  // RPM
      outBuff[payloadLen + 0] = 0x03;
      outBuff[payloadLen + 1] = FixInt(int(iob_throttle/30.0), 1);
      outBuff[payloadLen + 2] = FixInt(int(iob_throttle/30.0), 2);
      addedLen = 3;      
      break;

    case 0x04:  // Fuel Level
      outBuff[payloadLen + 0] = 0x04;
      outBuff[payloadLen + 1] = FixInt(iob_battery_remaining_A, 1);
      outBuff[payloadLen + 2] = FixInt(iob_battery_remaining_A, 2);
      addedLen = 3;      
      break;
//OK
    case 0x05:  // Temperature 2 are GPS satellite
      outBuff[payloadLen + 0] = 0x05;
      outBuff[payloadLen + 1] = iob_satellites_visible;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;

    //Little Endian exception
    case 0x06:  // Voltage, first 4 bits are cell number, rest 12 are voltage in 1/500v steps, scale 0-4.2v
      {
        int val = (int)(2100.0*(iob_vbat_A/3.0)/4.2); 
        int tmp1 = FixInt(val, 2);
        int tmp2 = FixInt(val, 1);
        
        outBuff[payloadLen + 0] = 0x06;
        outBuff[payloadLen + 1] = tmp1;
        outBuff[payloadLen + 2] = tmp2;
        outBuff[payloadLen + 3] = 0x06;
        outBuff[payloadLen + 4] = tmp1 + 0x10;
        outBuff[payloadLen + 5] = tmp2;
        outBuff[payloadLen + 6] = 0x06;
        outBuff[payloadLen + 7] = tmp1 + 0x20;
        outBuff[payloadLen + 8] = tmp2;
//      outBuff[payloadLen + 1] = (3 << 4) || FixInt(int(iob_vbat_A/3.0), 2);
//      outBuff[payloadLen + 2] = FixInt(int(iob_vbat_A/3.0), 1);
        
        addedLen = 9;
      }
      break;
//OK
    case 0x10:  // Altitude Baro
      outBuff[payloadLen + 0] = 0x10;
      outBuff[payloadLen + 1] = FixInt(iob_alt, 1);
      outBuff[payloadLen + 2] = FixInt(iob_alt, 2);
      addedLen = 3;      
      break;
//OK      
    case 0x11:  // GPS Speed, before "."
      outBuff[payloadLen + 0] = 0x11;
      outBuff[payloadLen + 1] = FixInt(iob_groundspeed, 1);
      outBuff[payloadLen + 2] = FixInt(iob_groundspeed, 2);
      addedLen = 3;      
      break;
    case 0x11+8:  // GPS Speed, after "."
      {
        float tmp = (iob_groundspeed - int(iob_groundspeed)) * 10000.0f;
        outBuff[payloadLen + 0] = 0x11+8;
        outBuff[payloadLen + 1] = FixInt(int(tmp), 1);
        outBuff[payloadLen + 2] = FixInt(int(tmp), 2);
        addedLen = 3;      
      }
      break;
//OK
    //Little Endian exception
    case 0x12:  // Longitude, before "."
      outBuff[payloadLen + 0] = 0x12;
      outBuff[payloadLen + 1] = FixInt(long(iob_lon),1);
      outBuff[payloadLen + 2] = FixInt(long(iob_lon),2);
      addedLen = 3;      
      break;
    case 0x12+8:  // Longitude, after "."
      outBuff[payloadLen + 0] = 0x12+8;
      outBuff[payloadLen + 1] = FixInt(long((iob_lon - long(iob_lon)) * 10000.0), 1);  // Only allow .0000 4 digits
      outBuff[payloadLen + 2] = FixInt(long((iob_lon - long(iob_lon)) * 10000.0), 2);  // Only allow .0000 4 digits after .
      addedLen = 3;      
      break;
    case 0x1A+8:  // E/W
      outBuff[payloadLen + 0] = 0x1A+8;
      outBuff[payloadLen + 1] = iob_lon_dir;
      outBuff[payloadLen + 2] = 0;
      addedLen = 3;      
      break;
//OK
    //Little Endian exception
    case 0x13:  // Latitude, before "."
      outBuff[payloadLen + 0] = 0x13;
      outBuff[payloadLen + 1] = FixInt(long(iob_lat),1);
      outBuff[payloadLen + 2] = FixInt(long(iob_lat),2);
      addedLen = 3;      
      break;
    case 0x13+8:  // Latitude, after "."
      outBuff[payloadLen + 0] = 0x13+8;
      outBuff[payloadLen + 1] = FixInt(long((iob_lat - long(iob_lat)) * 10000.0), 1);
      outBuff[payloadLen + 2] = FixInt(long((iob_lat - long(iob_lat)) * 10000.0), 2);      
      addedLen = 3;      
      break;  
    case 0x1B+8:  // N/S
      outBuff[payloadLen + 0] = 0x1B+8;
      outBuff[payloadLen + 1] = iob_lat_dir;
      outBuff[payloadLen + 2] = 0;      
      addedLen = 3;      
      break;
  
    case 0x14:  // course, before "."
      outBuff[payloadLen + 0] = 0x14;
      outBuff[payloadLen + 1] = FixInt(iob_heading, 1);
      outBuff[payloadLen + 2] = FixInt(iob_heading, 2);
      addedLen = 3;      
      break;
    case 0x14+8:  // course, after "."  .. check calculation
      outBuff[payloadLen + 0] = 0x14+8;
      outBuff[payloadLen + 1] = 0x00;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;
//OK     
    //Little Endian exception  
    case 0x15: // date/month
      outBuff[payloadLen + 0] = 0x15;
      outBuff[payloadLen + 1] = 0x00;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;
    case 0x16: // year
      outBuff[payloadLen + 0] = 0x16;
      outBuff[payloadLen + 1] = 0x00;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;
    case 0x17: // hour/minute
      outBuff[payloadLen + 0] = 0x17;
      outBuff[payloadLen + 1] = hour, DEC;
      outBuff[payloadLen + 2] = minute, DEC;
      addedLen = 3;      
      break;
    case 0x18: // second
      outBuff[payloadLen + 0] = 0x18;
      outBuff[payloadLen + 1] = second, DEC;
      outBuff[payloadLen + 2] = 0x00;
      addedLen = 3;      
      break;

    case 0x24:  // Roll
      outBuff[payloadLen + 0] = 0x24;      
      outBuff[payloadLen + 1] = FixInt(iob_roll * 100, 1);
      outBuff[payloadLen + 2] = FixInt(iob_roll * 100, 2);
      addedLen = 3;      
      break;
    case 0x25:  // Pitch
      outBuff[payloadLen + 0] = 0x25;
      outBuff[payloadLen + 1] = FixInt(iob_pitch * 100, 1);
      outBuff[payloadLen + 2] = FixInt(iob_pitch * 100, 2);
      addedLen = 3;      
      break;
    case 0x26:  // Yaw
      outBuff[payloadLen + 0] = 0x26;
      outBuff[payloadLen + 1] = FixInt(iob_yaw * 100, 1);
      outBuff[payloadLen + 2] = FixInt(iob_yaw * 100, 2);
      addedLen = 3;      
      break;

    case 0x3A:  // Volt 
      //iob_vbat_A o boardVoltage
      outBuff[payloadLen + 0] = 0x3A;
      outBuff[payloadLen + 1] = FixInt(int(iob_vbat_A), 1);
      outBuff[payloadLen + 2] = FixInt(int(iob_vbat_A), 2);
      addedLen = 3;      
      break;
    case 0x3B:
      //iob_vbat_A o boardVoltage
      outBuff[payloadLen + 0] = 0x3B;
      outBuff[payloadLen + 1] = FixInt(int((iob_vbat_A - int(iob_vbat_A)) * 1000.0), 1);
      outBuff[payloadLen + 2] = FixInt(int((iob_vbat_A - int(iob_vbat_A)) * 1000.0), 2);
      addedLen = 3;      
      break;

    case 0x28:
      outBuff[payloadLen + 0] = 0x28;
      outBuff[payloadLen + 1] = FixInt(int(iob_ampbatt_A), 1);
      outBuff[payloadLen + 2] = FixInt(int(iob_ampbatt_A), 2);
      addedLen = 3;      
      break;

    default:
      addedLen = 0;
  }
  return addedLen; 

}

byte addEnd() {
  return 1; 
}

byte sendPayload(byte len) {
  frSerial.write(0x5E);
  for(byte pos = 0; pos <= len - 1; pos = pos + 3) {
    frSerial.write(byte(outBuff[pos + 0]));

    switch  (outBuff[pos + 1]) {
      case 0x5E:
        frSerial.write(byte(0x5D));
        frSerial.write(byte(0x3E));
        break;
      case 0x5D:
        frSerial.write(byte(0x5D));
        frSerial.write(byte(0x3E));
        break;
        
      default:
        frSerial.write(byte(outBuff[pos + 1]));
    }

    switch  (outBuff[pos + 2]) {
      case 0x5E:
        frSerial.write(byte(0x5D));
        frSerial.write(byte(0x3E));
        break;
      case 0x5D:
        frSerial.write(byte(0x5D));
        frSerial.write(byte(0x3E));
        break;
        
      default:
        frSerial.write(byte(outBuff[pos + 2]));
    }

    frSerial.write(0x5E);
  }
  return 0;
}

// FrSky int handling
long FixInt(long val, byte mp) {  
  if(mp == 2) return long(val / 256);
  if (val >= 256 && mp == 1) 
    return val % 256;  
}

void updateTime() {
  if(second >= 60) {
    second = 0;
    minute++;
  }
  if(minute >= 60) {
    second = 0;
    minute = 0;
    hour++;
  } 
  if(hour >= 24) {
    second = 0;
    minute = 0;
    hour = 0;
  }
}

