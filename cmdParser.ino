/*
*   Program   : cmdParser
*  
*   This code is based on the great "portaprog" code by "Bradan Lane Studio"
*  
*   Copyright 2022 Willem Aandewiel
*  
*   This code may be redistributed and/or modified as outlined
*   under the MIT License
*
*   TERMS OF USE: MIT License. See bottom of file.
*   
***************************************************************************
*  Use Boards Manager to install Arduino ESP32 core 2.0.2
*
* Arduino-IDE settings for cmdParser:
*
*   - Board: "ESP32 Wrover Module"
*   - Upload Speed: "115200"  -  "921600"
*   - Flash Frequency: "80 MHz"
*   - Flash mode: "QIO" | "DOUT" | "DIO"    // if you change from one to the other OTA may fail!
*   - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB FS)"
*   - Core Debug Level: "None"
*   - Port: <select correct port>
*   
* Using library FS at version 2.0.0         (part of Arduino ESP32 Core @2.0.2)
* Using library SPIFFS at version 2.0.0     (part of Arduino ESP32 Core @2.0.2)
* Using library WiFi at version 2.0.0       (part of Arduino ESP32 Core @2.0.2)
* Using library WebServer at version 2.0.0  (part of Arduino ESP32 Core @2.0.2)
* Using library ESPmDNS at version 2.0.0    (part of Arduino ESP32 Core @2.0.2)
*
*
* Formatting ( http://astyle.sourceforge.net/astyle.html#_Quick_Start )
*   - Allman style (-A1)
*   - tab 2 spaces (-s2)
*   - Indent 'switch' blocks (-S)
*   - Indent preprocessor blocks (-xW)
*   - Indent multi-line preprocessor definitions ending with a backslash (-w)
*   - Indent C++ comments beginning in column one (-Y)
*   - Insert space padding after commas (-xg)
*   - Attach a pointer or reference operator (-k3)
*
*   use:  astyle -A1 -s2 -S -xW -w -Y -xg -k3 <*.ino>
*
******************************************************************************
*/


#include "mySecrets.h"

const char *ssid      = WIFI_SSID;
const char *password  = WIFI_PASSWORD;

#define TCP_PORT          8888  // could be anything for starters adn the user can override it with the .config file
#define TELNET_PORT         23  // its default
#define ALLOW_TELNET
#define TCP_TIMEOUT       1500  // milliseconds
#define MAX_FILENAME_LEN    32
//----


// local includes

#include <driver/dac.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <functional>

//--- prototypes ------------------------------------------------
bool      parserProcessCommands(Stream *client, bool aborted);

bool      ioInit();
void      ioLoop();
int       ioPrint(const char *text);
int       ioStreamPrint(Stream *out, const char *line);
int       ioStreamPrintf(Stream *out, const char *fmt, ...);
bool      ioRunCommandLine(const char *commands, bool aborted, bool wait);
bool      ioRunCommandPrintf(const char *fmt, ...);

bool      filesysInit();
void      filesysLoop();
bool      filesysExists(const char *name);
void      filesysDelete(const char *name);
File      filesysOpen(const char *name, const char *mode);
void      filesysClose(File handle);
char      *filesysGetFileInfo(bool first, bool hidden);
uint8_t   filesysGetType(const char *name);
uint8_t   filesysReadHex(Stream *handle);
uint16_t  streamReadLine(Stream *handle, char *buf, uint16_t size, bool escaped_characters);
bool      filesysSaveStart(const char *name);
bool      filesysSaveWrite(uint8_t *buf, size_t size);
bool      filesysSaveFinish();

#include "allincludes.h"

static WiFiMulti wifiMulti; // Create an instance of the WiFiMulti class, called 'wifiMulti'

static WiFiServer g_tcp_server(TCP_PORT); // TODO need a more secure option
#ifdef ALLOW_TELNET
  static WiFiServer g_telnet_server(TELNET_PORT); // TODO need a more secure option
#endif
// we only support one client (of each type) at a time
#ifdef ALLOW_TELNET
  static WiFiClient g_telnet_client;
#endif

static WiFiClient g_tcp_client;

//--------------------------------------------------------------------------
static void wifi_handle_tcp_requests()
{
  // handle any connection requests
  // we only support one telnet connect at a time; a second one is ejected
  if (g_tcp_server.hasClient())
  {
    DEBUGSERIAL.println("tcp client requested");
    if (g_tcp_client) g_tcp_client.stop();
    g_tcp_client = g_tcp_server.available();
  }
  else
  {
    // we lost the connection
    if (g_tcp_client && !g_tcp_client.connected())
    {
      g_tcp_client.stop();
      DEBUGSERIAL.println("tcp client stopped");
      return;
    }
  }

  if (g_tcp_client && g_tcp_client.connected())
  {
    uint32_t timeout = millis() + TCP_TIMEOUT;
    while (millis() < timeout)
    {
      if (g_tcp_client.available())
        break;
    }
    if (millis() >= timeout)
    {
      // we timed out
      DEBUGSERIAL.println("TCP Client Timeout");
      g_tcp_client.stop();
      return;
    }
    // TCP processing is handed off to the parser subsystem
    parserProcessCommands((Stream *) (&g_tcp_client), false);
  }
  // since we operate in single command/response mode, we can close the client session
  g_tcp_client.stop();
  
} //  wifi_handle_tcp_requests()


//--------------------------------------------------------------------------
static bool wifi_start_tcp_server() 
{
  // Start Telnet server
  g_tcp_server.begin(TCP_PORT);
  g_tcp_server.setNoDelay(true);
  return true;

} //  wifi_start_tcp_server()


//--------------------------------------------------------------------------
#ifdef ALLOW_TELNET
static bool wifi_start_telnet_server()
{
  // Start Telnet server
  g_telnet_server.begin();
  g_telnet_server.setNoDelay(true);
  return true;
  
} //  wifi_start_telnet_server()


static void wifi_handle_telnet_requests()
{
  memset(g_network_buf, 0, (MAX_NETWORK_TEXT + 1 + 1));
  // the telnet connection interfaces to the CHIPSERIAL UART

  // handle any connection requests
  // we only support one telnet connect at a time; a second one is ejected
  if (g_telnet_server.hasClient())
  {
    DEBUGSERIAL.println("telnet client requester");
    if (g_telnet_client)
      g_telnet_client.stop();
    g_telnet_client = g_telnet_server.available();
  }
  else
  {
    // we lost the connection
    if (g_telnet_client && !g_telnet_client.connected())
      g_telnet_client.stop();
  }

  //--  a little trick: if the first character is a backslash, then we 
  //-- redirect the telnet to the parser command process
  if (g_telnet_client.available() && (g_telnet_client.peek() == '\\'))
  {
    g_telnet_client.read(); // throw away the back slash
    delay(1000);
    /*
      OK, this is a huge kludge.
      we delay so the user has time to compose and send the command(s).
      they (I) had better type quick .. or use cut/paste ;-)
    */
    parserProcessCommands((Stream *)(&g_telnet_client), false);
    return;
  }

  // Get data from the telnet client and push it to the UART
  while (g_telnet_client.available())
  {
    CHIPSERIAL.write(g_telnet_client.read());
  }

  // get any data from UART and push it to telnet client
  if (CHIPSERIAL.available())
  {
    size_t len = CHIPSERIAL.available();

    if (len > MAX_NETWORK_TEXT)
      len = MAX_NETWORK_TEXT;

    CHIPSERIAL.readBytes(g_network_buf, len);

    // Push UART data to telnet client
    CHIPSERIAL.write(g_network_buf, len);

    // if the buffer doesn't have a newline, add one but be sure there is space
    if (g_network_buf[len-2] != '\n')
    {
      if (len == MAX_NETWORK_TEXT)
        len -= 2;
      g_network_buf[len-1] = '\n';
      g_network_buf[len] = 0;
    }
    else
    {
      // if the buffer is not null terminated, do so but be sure there is space
      if (g_network_buf[len-1] != 0)
      {
        if (len == MAX_NETWORK_TEXT)
          len--;
        g_network_buf[len] = 0;
      }
    }

    VERBOSE("\nbuf[%d]:\n", len);
    ioStreamPrint(NULL, g_network_buf);

  }
  
} //  wifi_handle_telnet_requests()
#endif
/* ---
#### wifiLoop()
Give the WiFi services an opportunity to respond to any necessary actions
--- */
void wifiLoop()
{
#ifdef ALLOW_TELNET
  wifi_handle_telnet_requests();
#endif

  wifi_handle_tcp_requests();
  
  if (Serial.available())
  {
    parserProcessCommands(&Serial, false);
  }
  
} //  wifiLoop()


//--------------------------------------------------------------------------
static void wifi_start_mdns()   // Start the mDNS responder
{
  if (!MDNS.begin("parser")) // start the multicast domain name server
    MESSAGE("mDNS error");
  else
  {
    MDNS.addService("tcpupdi",  "tcp", TCP_PORT);
    MDNS.addService("telnet", "tcp", TELNET_PORT);
    //MESSAGE("TCP://%s\n", wifiAddress());
  }
  
} //  wifi_start_mdns()

// -----------------------------------------------------------
// the standard Arduino entrypoints setup() and loop()
// -----------------------------------------------------------

//--------------------------------------------------------------------
void setup()
{
  // Serial = default UART on ESP32
  DEBUGSERIAL.begin(115200);
  while (!Serial)
    ; // wait for serial attach
  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_STA);

  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  g_wifi_connected = true;
  wifi_start_mdns();

  wifi_start_tcp_server();
  
#ifdef ALLOW_TELNET
  wifi_start_telnet_server();
#endif

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  DEBUGSERIAL.println();
  DEBUGSERIAL.println();
  DEBUGSERIAL.println("Serial0 Initialized");

  // free up GPIO25 and GPIO26 pins
  dac_output_disable(DAC_CHANNEL_1);
  dac_output_disable(DAC_CHANNEL_2);

  ioInit(); // do this first so we have screen output capability

  filesysInit();

  //configRead(); // if a config file exists, load all of the settings

  //wifiInit();
  parserInit();

  DEBUGSERIAL.println("---- System Initialized ----");

} //  setup()


//--------------------------------------------------------------------
void loop()
{
  wifiLoop();
  filesysLoop();
  //parserLoop();
  ioLoop();
  delay(1);

} //  loop()

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
/*eof*/
