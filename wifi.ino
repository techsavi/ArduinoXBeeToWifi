/* This module handles the interface with the ESP8266 WiFi module */

const char *AP = "SSID";         // Wifi SSID 
const char *PW = "PASSWORD";     // Wifi Password 
const char *IP = "192.168.1.10"; // openHAB IP
const char *PT = "8080";         // openHAB port

// Send a command to the WiFi module and wait for an OK before returning
boolean SendWaitOK(const String &cmd)
{
  Serial3.println(cmd);
  DebugWifiCmd(cmd);
  delay(50);
  if (Serial3.find("OK"))
  {
    Serial.println("OK");
    return true;
  }
  else
  {
    Serial.println("FAIL");
    return false;
  }
}

// Keeps sending a command until it is acknoledged with an OK
boolean RepeatUntilOK(const String &cmd, int maxtries)
{
  for (int i=0; i<maxtries; i++)
  {
    boolean ok = SendWaitOK(cmd);
    if (ok) return ok;
    delay(2000);
  }
}

// Reset WiFi module at startup
boolean resetWifi()
{
  // try up to 5 times to reset and wait until ready
  for (int i=0; i<5; i++)
  {
    Serial3.println("AT+RST");
    if (Serial3.find("ready"))
    {
      Serial.println("Module is ready");
      return true;
    }
  }
  Serial.println("Module had no response.");
  while (1);
}

// Connect the WiFi module to the specified Access Point
boolean connectWiFi() {
  boolean connected = false;
  String cmd = "AT+CWJAP=\"";
  cmd += AP;
  cmd += "\",\"";
  cmd += PW;
  cmd += "\""; 
  
  // If we don't connect, nothing will work, repeatdly try then give up
  // a reset would be required to rejoin
  while (1)
  {
    if (RepeatUntilOK(cmd, 10))
    {
      connected = true;
      return true;
    }
    delay(1000 * 60 * 15); // wait 15 min then try again
  }
  return false;
}

// print the IP address of the WiFi module
void printIP()
{
  //print the ip addr
  Serial3.println("AT+CIFSR");
  Serial.println("ip address:");
  while (Serial3.available())
    Serial.write(Serial3.read());
}

// Unused debugging function, can be used to manually send commands 
// to the WiFi module via the Arduino IDE Serial Monitor
void wifiEcho()
{
  static int col = 0;
  if (Serial.available() > 0) {
    int termByte = Serial.read();
    Serial3.write(termByte);
  }
  String reply;
  while (Serial3.available() > 0) {
    int wifiByte = Serial3.read();
    reply += String(char(wifiByte));
  }
  if (reply.length() > 0) 
  {
    if (reply.length() > 80)
    {
      int offset = 0;
      while (offset < reply.length())
      {
        Serial.println(reply.substring(offset, offset+80));
        offset += 80;
      }
    }
    else Serial.print(reply);
  }
}

// Sends a Rest request
String CIPSend(String cmd)
{
  // Start connection, repeating if it initally fails
  String startstr = "AT+CIPSTART=\"TCP\",\"";
  startstr += IP;
  startstr += "\",";
  startstr += PT;
  if (!RepeatUntilOK(startstr, 5))
  {
    DebugWifiError("CIPSTART Error");
    return "";
  }
  // Send request start with request length
  Serial3.print("AT+CIPSEND=");
  Serial3.println(cmd.length());
  // wait for the prompt acknowledging the CIPSEND
  if (!Serial3.find(">"))
  {
    Serial3.println("AT+CIPCLOSE");
    DebugWifiError("CIPSEND Connect Timeout");
    return "";
  }

  // send request body
  Serial3.print(cmd);

  // wait for replly prefix and 
  Serial3.find("+IPD,");
  
  // read reply length following prefix
  char s[500];
  int b = Serial3.readBytesUntil(':',s,5);
  s[b] = 0;
  int count = atoi(s);
  if (count >= 500) count = 499;
  
  // read reply 
  b = Serial3.readBytes(s, count);
  s[b] = 0;
  DebugWifiRest(cmd);
  DebugWifiReply(s);
  return String(s);
}

// Post a request to OpenHAB
void RestPostUpdate(const char *name, const char *val)
{
  char s[64];
  // construct the POST request
  sprintf(s, "POST /rest/items/%s HTTP/1.0\r\n", name);
  String cmd = s; 
  cmd += "Content-type: text/plain\r\n";
  sprintf(s, "Content-Length: %d\r\n", strlen(val));
  cmd += s;
  cmd += "\r\n";
  cmd += val;
  String reply = CIPSend(cmd);
}

// Put a state value to openHAB
void RestPutState(const char *name, const char *val)
{
  // construct PUT request
  char s[64];
  sprintf(s, "PUT /rest/items/%s/state HTTP/1.0\r\n", name);
  String cmd = s; 
  cmd += "Content-type: text/plain\r\n";
  sprintf(s, "Content-Length: %d\r\n", strlen(val));
  cmd += s;
  cmd += "\r\n";
  cmd += val;

  String reply = CIPSend(cmd);
}

// put a boolean state as a switch value
void RestPutState(const char *name, boolean val)
{
  RestPutState(name, val ? "OPEN" : "CLOSED");
}

// posts a floating point number
void RestPostUpdate(const char *name, float val)
{
  char sval[16];
  dtostrf(val, 4, 1, sval);
  RestPostUpdate(name, sval);
}

// one-time setup of WiFi module
boolean setupWifi() {
  Serial3.begin(57600);   
  Serial3.setTimeout(8000);
  
  SendWaitOK("AT");

  //connect to the wifi
  resetWifi();
  connectWiFi();
  SendWaitOK("AT+CWMODE=1");

  //set the single connection mode
  SendWaitOK("AT+CIPMUX=0");
  SendWaitOK("AT+CIPMODE=0");
  return true;
}


