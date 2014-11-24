/* This module handles the debug messages sent to the main Serial port 
 * so events can be monitored or debugged via the connected PC
 * adjut the boolean flags below to enable or disable outputs
 */
boolean debug_packet_raw = false;
boolean debug_packet_decode = false;
boolean debug_item_state = true;
boolean debug_wifi_cmd = true;
boolean debug_wifi_rest = true;
boolean debug_wifi_reply = false;
boolean debug_xbee = true;

// Print the raw XBee packet data in hex codes
void DebugPrintPacketRaw(unsigned char* buf, int buflen, boolean force)
{
  if (!debug_packet_raw && !force) return;
  
  Serial.println("Received Packet Data");
  for (int i = 0; i < buflen; i++)
  {
    char s[16];
    sprintf(s, "%02x ", buf[i]);
    Serial.print(s);
  }
  Serial.println();
}

void DebugPrintPacketRaw(unsigned char* buf, int buflen)
{
  DebugPrintPacketRaw(buf, buflen, false);
}

// print the decoded XBee IO packet information, allow flag override on errors
void DebugPrintPacketDecode(struct XBeePacket &xbeedata, boolean force)
{
  if (!debug_packet_decode && !force) return;
  
  // Debug echo parsed data
  Serial.println("XBee Packet Contents");
  Serial.print("API       "); Serial.println(xbeedata.api, HEX);
  Serial.print("DeviceID  ");
  for (int i = 0; i < 8; i++) {char s[8]; sprintf(s, "%02x ", xbeedata.xid[i]); Serial.print(s);}
  Serial.println();
  Serial.print("My ID     "); Serial.print(xbeedata.mid >> 8, HEX); Serial.println(xbeedata.mid & 0xff, HEX);
  Serial.print("Pack Type "); Serial.println(xbeedata.ptype, HEX);
  for (int i = 0; i < 13; i++)
  {
    if (xbeedata.dioValid[i])
    {
      char s[20];
      sprintf(s, "DIO    %2d  %d", i, xbeedata.dio[i]);
      Serial.println(s);
    }
  }
  for (int i = 0; i < 4; i++)
  {
    if (xbeedata.analogValid[i])
    {
      char s[20];
      sprintf(s, "Analog %2d %d", i, xbeedata.analog[i]);
      Serial.println(s);
    }
  }
  Serial.println();
}

// monitor an XBee packet decode only if enabled by flags
void DebugPrintPacketDecode(struct XBeePacket &xbeedata)
{
  DebugPrintPacketDecode(xbeedata, false);
}

// monitor the decoded sensor event boolean
void DebugPrintItemStateBool(const char *name, boolean state, boolean changed)
{
  if (!debug_item_state) return;
  
  char s[64];
  sprintf(s, "%-20s %3d %s", name, state ? 1 : 0, changed ? "CHANGED" : "");
  Serial.println(s);
}

// monitor the decoded sensor event integer
void DebugPrintItemStateInt(const char *name, int state, boolean changed)
{
  if (!debug_item_state) return;

  char s[64];
  sprintf(s, "%-20s %3d %s", name, state, changed ? "CHANGED" : 0);
  Serial.println(s);
}

// monitor the wifi command strings
void DebugWifiCmd(const String &s)
{
  if (!debug_wifi_cmd) return;
  
  Serial.print("WIFI CMD:   ");
  Serial.println(s);
}

// monitor the outgoing Rest requests
void DebugWifiRest(const String &s)
{
  if (!debug_wifi_rest) return;
  
  Serial.print("WIFI REST:  ");
  Serial.println(s);
  Serial.println();
}

// monitor a WiFi reply
void DebugWifiReply(const String &s)
{
  if (!debug_wifi_reply) return;
  
  Serial.print("WIFI REPLY: ");
  Serial.println(s);
}

// log a Wifi error
void DebugWifiError(const String &s)
{
  Serial.print("WIFI ERROR: ");
  Serial.println(s);
}

// print a string to the monitor
void DebugPrintXBee(String &s)
{
  if (!debug_xbee) return;
  Serial.println(s);
}

// configure the serial port at startup
boolean setupMonitor()
{
    Serial.begin(57600);     // PC
    return true;
}
