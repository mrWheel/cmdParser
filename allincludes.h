
#ifndef _ALLINCLUDES_H
#define _ALLINCLUDES_H

#include <Arduino.h>
#include <functional>

#include <FS.h>
#include <SPIFFS.h>

// ----------------------------------------------------------------------------
// -- useful macros for message display at various levels of importance -------
// ----------------------------------------------------------------------------

#define DEBUGSERIAL  Serial  // used for messages while debugging the PortaProg code
#define CHIPSERIAL   Serial1 // used to communicate with an attached device

#define DEBUG(fmt, ...)     DEBUGSERIAL.printf(fmt, ##__VA_ARGS__)
//#define VERBOSE(fmt, ...)   DEBUGSERIAL.printf(fmt, ##__VA_ARGS__)

//#define VERBOSEON(fmt, ...) DEBUGSERIAL.printf(fmt, ##__VA_ARGS__)
#define MESSAGE(fmt, ...)   ioStreamPrintf(NULL, fmt, ##__VA_ARGS__)
#define CLIENTMSG(fmt, ...) ioStreamPrintf(client, fmt, ##__VA_ARGS__)

#ifndef DEBUG
  #define DEBUG(fmt, ...) ((void)0)
#endif
#ifndef VERBOSE
  #define VERBOSE(fmt, ...) ((void)0)
#endif
#ifndef MESSAGE
  #define MESSAGE(fmt, ...) ((void)0)
#endif
#ifndef CLIENTMSG
  #define CLIENTMSG(fmt, ...) ((void)0)
#endif

// there are mixed reports of a serial flush problem. it was encountered on on eof the test boards used during development
#define SERIAL_FLUSH(s) s.flush()
//#define SERIAL_FLUSH(s) while(s.available()) s.read()


#define MAX_NETWORK_TEXT 256
extern char g_network_buf[];  // it is shared between the telnet and the tcp processing
//extern WiFiClient g_tcp_client; // I hate the notion but things are not working with attempting to pass by reference
static char mdnsName[32] = {"parser"};       // make it a composite of 'root the last hex value from the MAC address
static bool g_wifi_connected = false;
static bool g_wifi_is_hotspot = false;
char g_network_buf[MAX_NETWORK_TEXT + 1 + 1]; // buffer + '\n' + 0
int configWifiPort = 0;


#include "filesys.h"
#include "parser.h"
#include "bufferstream.h"
#include "io.h"

#endif
