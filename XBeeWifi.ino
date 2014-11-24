/* this is the main XBee receiving module and parser */

// this is the data to be received in an IO packet
struct XBeePacket {
  unsigned char api;
  unsigned char xid[8];
  unsigned short mid;
  unsigned char ptype;
  boolean dio[13];
  boolean dioValid[13];
  unsigned short analog[4];
  boolean analogValid[4];
};

// XBee transmitters IDs, used to identify the source of a message
unsigned char devices[2][8] = { 
  {0x00, 0x13, 0xa2, 0x00, 0x40, 0x0a, 0x18, 0x96},
  {0x00, 0x13, 0xa2, 0x00, 0x40, 0x0a, 0x11, 0xf5}
};

// timeouts for time-base updates
const unsigned long temperature_refresh = 300000; // 5 minute temp update
const unsigned long switch_refresh = 900000; // 15 minute switch refresh

// sensor item data
int            garage_temp = 0;
boolean        garage_door_left = false;
boolean        garage_door_right = false;

int            basement_temp = 0;
boolean        basement_water = false;

// Lookup the device index based on the received ID
int GetDeviceNum(const unsigned char id[])
{
  for (int i = 0; i < 2; i++)
  {
    if (memcmp(&id[0], &devices[i][0], 8) == 0)
    {
      return i;
    }
  }
  return -1;
}

// debugging echo
void echoXBee()
{
  if (Serial2.available() > 0)
  {
    int xByte = Serial2.read();
    Serial.print(xByte, HEX);
  }
}

// XBee packet receiving function. Buffers data until a valid packet is found
// resulting packet output will start with the packet start byte 0x7e and end with the checksum
boolean ReadXBee(unsigned char* buf, int &buflen)
{
  static boolean havestart = false;
  static int tbufidx = 0;
  static unsigned char tbuf[128];
  
  // read characters one at a time until none left in buffer
  if (Serial2.available() > 0)
  {
    // read character
    int c = Serial2.read();
    // if haven't found start byte, checck this one
    if (!havestart)
    {
      if (c == 0x7e)
      {
        havestart = true;
        tbuf[tbufidx++] = c;
      } else {
        // not the start byte, ignore
      }
    } else {
      // add to tbuf
      tbuf[tbufidx++] = c;
      if (tbufidx >= sizeof(tbuf))
      {
        Serial.println("Buffer Overflow, resetting");
        tbufidx = 0;
        havestart = false;
      }

      // check for packet completed
      if (tbufidx > 2)
      {
        int packetlen = tbuf[1] * 0x100 + tbuf[2];
        // sanity check the start byte was true
        if (packetlen > 40)
        {
          // check for a different start byte in the size feilds
          if (tbuf[1] = 0x7e) 
          {
            for (int i=0; i<tbufidx - 1; i++) tbuf[i] = tbuf[i+1];
            tbufidx--;
            return false;
          }
          if (tbuf[2] = 0x7e) 
          {
            for (int i=0; i<tbufidx - 2; i++) tbuf[i] = tbuf[i+2];
            tbufidx -= 2;
            return false;
          }
        }
        if (tbufidx >= packetlen + 4) // account for start, size, checksum byte
        {
          // should have full packet, verify checksum
          int sum = 0;
          for (int i = 0; i < packetlen; i++)
          {
            sum += tbuf[i + 3];
          }
          sum = 0xff - sum & 0xff;
          if (sum == tbuf[packetlen + 3])
          {
            // valid packet, copy to return
            if (packetlen > buflen)
            {
              Serial.println("Buffer too small");
              havestart = false;
              tbufidx = 0;
            } else {
              buflen = packetlen + 3;
              memcpy(buf, tbuf, buflen);
              DebugPrintPacketRaw(buf, buflen);
              havestart = false;
              tbufidx = 0;
              return true;
            }
          } else {
            Serial.println("Checksum failure");
            char s[32];
            sprintf(s, "%2x != %2x", sum, tbuf[packetlen + 3]);
            Serial.println(s);
            DebugPrintPacketRaw(tbuf, tbufidx);
            havestart = false;
            tbufidx = 0;
          }
        }
      }
    }      
  } else {
    // nothing received, return no packet found
    return false;
  }
  return false;
}

// Take a valid XBee data packet and decode the enclosed data
bool DecodeXBee(unsigned char *buf, int buflen, struct XBeePacket &xbeedata)
{
  if (buflen < 20) return false;
  xbeedata.api = buf[3];
  
  // only interpretting Frame Type 0x92 IO Sample Data
  if (xbeedata.api != 0x92)
  {
    String s = "Unsupported Frame Type ";
    s += xbeedata.api;
    DebugPrintXBee(s);
    return false;
  }
  
  // copy the IDs
  memcpy(&xbeedata.xid[0], &buf[4], 8);
  xbeedata.mid = buf[12] * 0x100 + buf[13];
  xbeedata.ptype = buf[14];
  
  // update validity and values of digital IO points
  boolean anydio = false;
  for (int i = 0; i < 8; i++)
  {
    xbeedata.dioValid[i] = ((buf[17] & (0x01 << i)) != 0);
    xbeedata.dio[i] = ((buf[20] & (0x01 << i)) != 0);
    anydio |= xbeedata.dioValid[i];
  }
  for (int i = 0; i < 5; i++)
  {
    xbeedata.dioValid[i+8] = ((buf[16] & (0x01 << i)) != 0);
    xbeedata.dio[i+8] = ((buf[19] & (0x01 << i)) != 0);
    anydio |= xbeedata.dioValid[i+8];
  }
  
  // update validity and values of analog data
  int offset = anydio ? 21 : 19;
  for (int i = 0; i < 4; i++)
  {
    xbeedata.analogValid[i] = ((buf[18] & (0x01 << i)) != 0);
    if (xbeedata.analogValid[i])
    {
      xbeedata.analog[i] = buf[offset] * 0x100 + buf[offset+1];
      offset += 2;
    }
  }
  DebugPrintPacketDecode(xbeedata);
  return true;
}

// Update state boolieans checking for changes
boolean UpdateItemStateBool(const char *name, boolean &curState, boolean newState)
{
  boolean changed = (newState != curState);
  curState = newState;
  DebugPrintItemStateBool(name, curState, changed);
  return changed;
}

// update state integers checking for changes
boolean UpdateItemStateInt(const char *name, int &curState, int newState)
{
  boolean changed = (newState != curState);
  curState = newState;
  DebugPrintItemStateInt(name, curState, changed);
  return changed;
}

// Convert the AD converter input to fahrenheit based on the sensor data sheet
float TemperatureAtoF(int val)
{
  return (val * 100.0 / 0x3ff) * 9.0 / 5.0 + 32.0;
}

// Check if a timer condition has been met, update for next event if so
bool DoTimeRefresh(unsigned long &last, unsigned long period)
{
  unsigned long now = millis();
  if (now > last + period)
  {
    last = now;
    return true;
  }
  return false;
}

// Process a fully decoded XBee IO packet, sending status updates
void ProcessXBee(struct XBeePacket &xbeedata)
{
  // Get the sending device based on the ID in the packet
  int device = GetDeviceNum(xbeedata.xid);
  
  switch (device)
  {
    case 0: // basement
    {
      // notify on switch change (or timeout)
      static boolean        firsttime = true;
      static unsigned long  last_temp_notify = 0;
      static unsigned long  last_switch_notify = 0;
      
      // check timeout conditions
      boolean stimerefresh = DoTimeRefresh(last_switch_notify, switch_refresh);
      boolean ttimerefresh = DoTimeRefresh(last_temp_notify, temperature_refresh);
      
      // update the sensor data and send updates to openHAB if conditions met
      boolean changed = UpdateItemStateBool("Basement Water", basement_water, xbeedata.dio[4]);
      if (changed || stimerefresh || firsttime)
        RestPutState("baseWater", !basement_water);
      
      UpdateItemStateInt("Basement Temp", basement_temp, xbeedata.analog[1]);
      if (ttimerefresh || firsttime) 
      {
        float temp = TemperatureAtoF(basement_temp);
        RestPostUpdate("tempBase", temp);
      }
      
      firsttime = false;
    } break;
    case 1: // garage
    {
      // notify on switch change (or timeout)
      static boolean        firsttime = true;
      static unsigned long  last_temp_notify = 0;
      static unsigned long  last_switch_notify = 0;

      // check timeout conditions
      boolean stimerefresh = DoTimeRefresh(last_switch_notify, switch_refresh);
      boolean ttimerefresh = DoTimeRefresh(last_temp_notify, temperature_refresh);
      
      // update the sensor data and send updates to openHAB if conditions met
      boolean changed = UpdateItemStateBool("Garage Door Left", garage_door_left, xbeedata.dio[2]);
      if (changed || stimerefresh || firsttime)
        RestPutState("garageDoorLeft", garage_door_left);

      changed = UpdateItemStateBool("Garage Door Right", garage_door_right, xbeedata.dio[3]);
      if (changed || stimerefresh || firsttime)
        RestPutState("garageDoorRight", garage_door_right);

      UpdateItemStateInt("Garage Temp", garage_temp, xbeedata.analog[1]);
      if (ttimerefresh || firsttime) 
      {
        float temp = TemperatureAtoF(garage_temp);
        RestPostUpdate("tempGarage", temp);
      }

      firsttime = false;
    } break;
    default: {}
  }
}

// the setup routine runs once when you press reset:
void setup() {
  setupMonitor();
  setupWifi();
  Serial2.begin(9600);    // XBee
}

// main loop, constatly looking for XBee packets
void loop() {
  unsigned char buf[64];
  int buflen = sizeof(buf);
  
  // get a packet from the XBee receiver
  if (ReadXBee(buf, buflen))
  {
    // convert the packet into data fields
    XBeePacket xbeedata;
    memset(&xbeedata, 0, sizeof(xbeedata));
    if (DecodeXBee(buf, buflen, xbeedata))
    {
      // process the data for each sensor
      ProcessXBee(xbeedata);
    } else {
      DebugPrintPacketRaw(buf, buflen, true);
    }
  }
}
