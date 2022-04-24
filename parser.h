/* ***************************************************************************
* File:    parser
*
* This content may be redistributed and/or modified as outlined
* under the MIT License
*
* ***************************************************************************** */


#ifndef __PARSER_H__
#define __PARSER_H__

// -------- CMDS dispatch functions --------------------------------------------------------------------
bool parserInit();
void parserLoop();

//bool parserProcessCommands(Stream *client, bool aborted);


// -------- Program Interface function vectors --------------------------------------------------------------------
typedef struct
{
  uint8_t dev_type;
  bool (*pgmrInit)();
  void (*pgmrLoop)();
  bool (*pgmrIsConnected)(Stream *client, bool silent);
  bool (*pgmrGetInfo)(Stream *client);
} avrDevice;

static avrDevice _parser_device_funcs;


#include "allincludes.h"

/*
  commands may any of the following:
    discrete commands
    commands with one or more parameter values
    commands with stream content (eg receiveing a file)

  a single line may have multiple commands
  there may be multiple lines
  a stream must be the last line as there is no way of knowing when the file ends
*/

#define PARSER_CMD_NONE    -1
#define PARSER_CMD_HELP    0 // this needs to be the first command ID
// setup / config commands
#define PARSER_CMD_INFO    1

// file commands
#define PARSER_CMD_DIR     11
#define PARSER_CMD_DEL     12
#define PARSER_CMD_CAT     13
#define PARSER_CMD_UPLOAD  14



typedef struct
{
  const uint8_t id;
  const char *name;
  const char *parms;
  const char *desc;
  uint8_t args;
  bool has_stream;  // need to know this in case we attempt to skip the command
  bool abortable;   // command is skipped when abort is active
} parserCmd_t;

// FYI: If you are wondering why the above #defines are added to the data structure it's because I kept reordering the help text and forgetting to renumber the defines



static parserCmd_t _parser_commands[] =
{
  // id, command name, parameters, help text, parm count

  /* ---
    - `HELP`: returns the help text
  --- */
  {PARSER_CMD_HELP, "HELP", "", "return help text", 0, false, false},
  /* ---
    - `INFO`:  for testing.
  --- */
  {PARSER_CMD_INFO, "INFO", "", "return chip information", 2, false, true},
  /* ---
  - `DEL` <filename>: Delete the named file from the SPIFFS.
--- */
  {PARSER_CMD_DEL, "DEL", "<name>", "delete file from SPIFFS", 1, false, true},
/* ---
  - `CAT` <filename>: stream contents of file from the SPIFFS back to the local computer standard output.
--- */
  {PARSER_CMD_CAT, "CAT", "<name>", "stream contents of file", 1, false, true},
/* ---
  - `UPLOAD` <filename> (stream): create a new file in the SPIFFS and store the contents of the stream to the file.
  The `stream` is whatever content is send over TCP to the PortaProg. A common method is to pipe the standard output of a command to TCP.
  For example, to send the contents of a file to standard output, use the Linux `cat` command or the windows `type` command.
    This is a general purpose file operation and no analysis is performed to determine the contents.
  The primary use of the `UPLOAD` command is to store `CMD` files on the SPIFFS but it is available to store any file.
  _NOTE: The command will consume all data available from the stream. It is not possible to include a follow-up command on the same TCP command-line. Subsequent commands may be send to the PortaProg using a new command-line._
--- */
  {PARSER_CMD_UPLOAD, "UPLOAD", "<name> (stream)", "save any type of data stream to SPIFFS", 1, true, true},
  /* ---
    - `DIR`: List all files currently stored on the SPIFFS.
  --- */
  {PARSER_CMD_DIR, "DIR", "", "list files on SPIFFS", 0, false, true},

};

const char *_parserHelp_text_usage = "Linux usage: (echo cmd; echo cmd; ...) | nc"; // the IP and port will be appended
const char *_parserHelp_text_legend[] =
{
  "",
  "The system has five data centers:",
  "    the Monitor",
  "    the TCP stream",
  "    SPIFFS file system",
  "    a memory buffer",
  "    the chip flash",
  "",
  "Commands provide moving content between these locations.",
  "",
  NULL
};


const char *_parserHelp_text_examples[] =
{
  "",
  "Examples:",
  "Command 1:\n(echo 'receive'; cat file.hex; echo 'flash') | nc IP PORT",
  "",
  "Command 2:",
  "(echo 'dump'; echo 'send') | nc IP PORT > file.hex",
  "",
  "Update config (linux):",
  "(echo 'upload .config'; cat config_file) | nc IP PORT",
  "",
  NULL
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

//--------------------------------------------------------------------
static bool _parser_trim(char *line)
{
  for (int i = strlen(line) - 1; i > 0; i--)
  {
    if ((line[i] == '\n') || (line[i] == '\r') || (line[i] == '\t') || (line[i] == ' '))
      continue;
    line[i + 1] = 0;
    return true;
  }
  return false;
  
} //  _parser_trim()


//--------------------------------------------------------------------
static bool _is_printable(char *line)
{
  // sanity check: make sure there are printable characters available
  if (!strlen(line))
    return true;
  bool printable = false;
  for (int i = 0; i < strlen(line); i++)
  {
    if (isprint(line[i]))
    {
      printable = true;
      break;
    }
  }
  return printable;
  
} //  _is_printable()

#define MAX_UART_BUF 127
static char g_last_received_string[MAX_UART_BUF+1]; // we keep the most recent received data so it can be used for smoketest TEST command

//--------------------------------------------------------------------
static bool _uartLoop(bool to_screen, char *match)
{
  bool found = false;
  if (match == NULL)
    found = true;

  char buffer[MAX_UART_BUF+1];
  int16_t len = 0;

  // NOTE: this is a convenience operation; it was RX only but we try to handle TX ... try!

  if (DEBUGSERIAL.available())
  {
    len = streamReadLine(&(DEBUGSERIAL), buffer, MAX_UART_BUF, false);
  }
  return found;
  
} //  _uartLoop()


//--------------------------------------------------------------------
static bool _parserHelp(Stream *client)
{
  if (client)
  {

    uint8_t entries = (sizeof(_parser_commands) / sizeof(parserCmd_t));

    for (int i = 0; i < entries; i++)
    {
      // to save some scrolling, we do not pad the help text on the display
      //MESSAGE("%s %s %s\n", _parser_commands[i].name, _parser_commands[i].parms, _parser_commands[i].desc);
      ioStreamPrintf(client, "%-8s %-16s - %s\n", _parser_commands[i].name, _parser_commands[i].parms, _parser_commands[i].desc);
    }
    for (int i = 0; _parserHelp_text_legend[i] != NULL; i++)
    {
      ioStreamPrintf(client, "%s\n", _parserHelp_text_legend[i]);
    }
    for (int i = 0; _parserHelp_text_examples[i] != NULL; i++)
    {
      ioStreamPrintf(client, "%s\n", _parserHelp_text_examples[i]);
    }

    // full formatted content back to client
    ioStreamPrintf(client, "%s %s %d\n", _parserHelp_text_usage, WiFi.localIP().toString(), configWifiPort);
  }
  return true;
  
} //  _parserHelp()

//--------------------------------------------------------------------
// create a file with the contents of the stream *without* any interpretation other than excess whitespace
static bool _parserSaveStream2File(Stream* client, char* filename) 
{
  MESSAGE("Save stream to file %s\n", filename);
  static char buffer[256+1];  // need space for null terminator

  bool success = false;
  int32_t len;

  File f = filesysOpen(filename, "w");
  if (!f) {
    ioStreamPrintf(client, "Error: unable to write to file %s\n", filename);
  }
  else
    success = true;

  do {
    int c;
    len = 0;
    while (client->available()) {
      c = client->read();
      DEBUG("%c", c);

      if (c == '\r') continue;
      if ((c == ' ') || (c == '\n')) 
      {
        //-- we throw away leading white space
        if (!len) continue;
      }
      if (c == 0) break;
      buffer[len++] = c;

      //-- the result will not be pretty but we need to break before 
      //-- overflow of the buffer
      if (len == 256) break;  
    }

    if (len > 0) 
    {
      //buffer[len++] = '\n'; // newline (I don't think so!)
      buffer[len++] = 0;    // null terminate the string

      // write the buffer to the file
      if (f) 
      {
        //-- the line buffer contains STREAM data to append to the active file
        f.print(buffer);
        VERBOSE("write: %s", buffer);
      }
    }
    else 
    {
      VERBOSE("OOPS: we should not have an empty line\n");
    }
  }
  while (len > 0);

  filesysClose(f);
  return success;
  
} //  _parserSaveStream2File()


//--------------------------------------------------------------------
static bool _parserReadFile2Stream(Stream* client, char* filename) {
  DEBUG("read file to stream %s\n", filename);
  ioStreamPrintf(client, "\r\nread file to stream [%s]\r\n", filename);
  static char buffer[256+1];  // need space for null terminator
  bool success = false;

  File input = filesysOpen(filename, "r");
  if (!input) {
    ioStreamPrintf(client, "Error: unable to open %s\n", filename);
    return false;
  }

  uint32_t len;
  while ((len = streamReadLine(&input, buffer, 256, false)) > 0) 
  {
    FILESYS_STRIP_NL(buffer, len);
    //-- output to destination (but don't duplicate output)
    client->printf("%s\n", buffer); 
  }
  //ioStreamPrint(client, "\r\nDone!\r\n"); // output to display

  filesysClose(input);

  return success;

} //  _parserStreamFile2Spiffs()

/* --
#### parserInit()

Performs all necessary initialization of programming systems.
Must be called once before using any of the other AVR operations.

- return: **bool** `true` on success and `false` on failure

-- */
bool parserInit()
{

  g_last_received_string[0] = 0;


  MESSAGE("Usage:\necho 'help' | nc %s %d\n", WiFi.localIP().toString(), TCP_PORT);

  /*
    if (configWifiSSID[0] == 0) {
      MESSAGE("\n");
      MESSAGE("If this is the first time using the PortaProg,\nupload a .config file to setup WiFi.\nSee documentation for settings.\n");
    }
  */
  return true;
}

/* --
#### parserLoop()

Give the CMDS programming processes - SPI or UPDI - an opportunity to respond to basic events.

**NOTE:*** the TCP interface operates from the WiFi loop process and calls the published AVR API functions.
-- */

void parserLoop()
{
  if (_parser_device_funcs.dev_type)
    (*_parser_device_funcs.pgmrLoop)(); // you can double check but this is most likely a nop

  _uartLoop(true, NULL); // this lets the PortaProg receive and display UART messages; it is RX-only
}

/* --
#### parserProcessCommands()

Process a Stream of commands. The most common Stream source is the TCP data. Any Stream is
supported provided it implements read(), write(), and all print...() methods.

- input: client **Stream ptr** an active Stream with the commands and data to be processes
- input: aborted **bool** indicates a prior command aborted
-- */

bool parserProcessCommands(Stream *client, bool aborted)
{

  if (!client)
  {
    // oops - we cant work without a connected client
    MESSAGE("PARSER processing client not available");
    return false;
  }
  DEBUGSERIAL.println("parserProcessCommands()...");

  // Get data from the client and process it
  uint32_t total = 0; // for diagnostics // TODO remove once things are working
  uint16_t len = 0;
  char c = 0;
  char *linebuffer = g_network_buf;
  int8_t command_id = PARSER_CMD_NONE;
  parserCmd_t *active_cmd = NULL;
  uint8_t command_index = 0;
  bool expecting_args, active_file;
  bool abort_processing = aborted;
  int8_t args = 0;
  uint32_t data_size_processed = 0;

  do
  {
    if (command_id == PARSER_CMD_NONE)
    {
      DEBUGSERIAL.println("zoek command_id...");
      expecting_args = false;
      active_file = false;
      args = 0;
      len = 0;
      command_index = 0;
      active_cmd = NULL;
    }

    //-- with the exception of a stream, commands do not split across lines. 
    //-- commands with streams will start the stream on a new line.
    while (client->available())
    {
      //-- we process commands and arguments the same - both assume 
      //-- whitespace as a delimeter
      //VERBOSE("?");
      c = client->read();
      total++;
      //VERBOSE("%c", c);

      if (c == 0)
        break;
      if (c == '\r')
        continue;
      if ((c == ' ') || (c == '\n'))
      {
        if (!len) // we can throw away leading white space
          continue;
        break;
      }

      linebuffer[len++] = c;
      // we chunk large streams of data
      if (len >= MAX_NETWORK_TEXT)
        break;
    }

    DEBUGSERIAL.printf("parsed [%s]\r\n", linebuffer);

    if (len)
    {
      linebuffer[len++] = 0;
      //len = 0; //-- this will force the outer loop to start reading 
                 //-- a new command or parameter
      //VERBOSE("parsed string: %s\n", linebuffer);

      // if we do not have an active command, then we look for one
      if (command_id < 0)
      {
        // no active command so we look for a match
        for (command_index = 0; command_index < (sizeof(_parser_commands) / sizeof(parserCmd_t)); command_index++)
        {
          // if we match a command from our list, make a note of it
          if (strcasecmp(_parser_commands[command_index].name, linebuffer) == 0)
          {
            active_cmd = &(_parser_commands[command_index]);
            command_id = _parser_commands[command_index].id;
            for(int i=0; i<strlen(active_cmd->name); i++) linebuffer[i]=' ';
            //DEBUGSERIAL.printf("ActiveCmd#[%d], linebuffer[%s]\r\n", command_id, linebuffer);
            break;
          }
        }
      }
      // if we still don't have a command, then its an error
      if (!active_cmd)
      {
        ioStreamPrintf(client, "Error, unrecognized command: [%s]\n\n", linebuffer);
        client->flush();
        command_id = PARSER_CMD_NONE; // output the help text
      }
      else
      {
        //-- if we received a command which needs args we can just continue 
        //-- the processing loop for the first arg
        //-- NOTE: the command is responsible for subsequent args
        if ((active_cmd->id > PARSER_CMD_HELP) && (active_cmd->args > args))
        {
          //-- TODO clean up this code so it isn't checking the arg count three times !
          if (!expecting_args)
          {
            expecting_args = true;
            len = 0;
          }
          else
          {
            args++; //-- we have an arg
            if (active_cmd->args > args)
            {
              //-- we still meed more args so ...
              len--;           //-- we remove the null terminator
              linebuffer[len++] = ' '; //-- and replace it with a space
            }
          }
          //DEBUGSERIAL.printf("%s need %d arg(s) has %d = %s\r\n", active_cmd->name, active_cmd->args, args, linebuffer);
          if (active_cmd->args > args)
            continue;
        }

        DEBUGSERIAL.printf("checking command: #%d %s\n", command_id, linebuffer);

        //-- process the designated command
        //-- any required args - with the expection of a stream - are contiguous 
        //-- in the linebuffer buffer and can be parsed by the command
        if (abort_processing)
        {
          if (active_cmd->abortable)
          {
            DEBUGSERIAL.printf("Aborting CMD: %s %s\n", active_cmd->name, linebuffer);
            //-- skip everything else if this command has a stream as its last parameter
            if (active_cmd->has_stream)
            {
              while (client->available())
                c = client->read();
            }
            command_id = PARSER_CMD_NONE;
          }
          else
          {
            DEBUGSERIAL.printf("Non-Abortable CMD: %s %s\n", active_cmd->name, linebuffer);
          }
        }
      }

      switch (command_id)
      {
        case PARSER_CMD_NONE:
        {
        } break;
        // informational operations
        case PARSER_CMD_HELP:
        {
          _parserHelp(client);
          command_id = PARSER_CMD_NONE;
        }
        break;
        case PARSER_CMD_INFO:
        {
          //DEBUGSERIAL.printf("get_info(%s);\r\n", linebuffer);
          char arg1[11];
          char arg2[11];
          int noArgs = sscanf(linebuffer, "%10s %10s", arg1, arg2);
          if (noArgs == 2) 
          {
            DEBUGSERIAL.printf("INFO: p1=[%s], p2=[%s]\r\n", arg1, arg2);
            ioStreamPrintf(client, "INFO: p1=[%s], p2=[%s]\r\n", arg1, arg2);
          } 
          else 
          {
            ioStreamPrintf(client, "Error: unrecognized INFO parameters: [%s]\r\n", linebuffer);
            ioStreamPrintf(client, "       INFO needs 2 parameters, found [%d]\r\n", noArgs);
            ioStreamPrintf(client, "       INFO parameter uses a strict format: <num> <num>\n");
          }
          command_id = PARSER_CMD_NONE;
        }
        break;
        // generic file operations
        case PARSER_CMD_DIR: 
        {
          ioStreamPrintf(client, "Contents:\n");
          char *info = filesysGetFileInfo(true, true);
          while (info) {
            ioStreamPrintf(client, "\t%s\n", info);
            info = filesysGetFileInfo(false, true);
          }
          client->print("\n");
          command_id = PARSER_CMD_NONE;
        } break;
        case PARSER_CMD_DEL: {
          // linebuffer now has the first arg = the filename
          if (!filesysExists(linebuffer))
            ioStreamPrintf(client, "Error: file %s does not exist\n", linebuffer);
          else {
            filesysDelete(linebuffer);
            ioStreamPrintf(client, "File %s deleted\n", linebuffer);
            //-aaw- ioClear(true);
          }
          command_id = PARSER_CMD_NONE;
        } break;
        case PARSER_CMD_CAT: {
          _parserReadFile2Stream(client, linebuffer);
          command_id = PARSER_CMD_NONE;
        } break;
        case PARSER_CMD_UPLOAD: {
          _parserSaveStream2File(client, linebuffer);
          command_id = PARSER_CMD_NONE;
        } break;

      } // end command switch

#if 0
      // if one of our commands signals we need to abort, then run out any remaining command stream data
      if (abort_processing)
      {
        while (client->available())
          c = client->read();
      }
#endif
    }
    else
    {
      VERBOSE("empty command\n");
    }

  } while (client->available());

  // this should never occur but it's here just in case
  if (active_file)
  {
    //filesysSaveFinish();
    //-aaw- ioStreamPrintf(client, "Received %d bytes\n", data_size_processed);
    DEBUGSERIAL.printf("Received %d bytes\n", data_size_processed);
  }

  VERBOSE("done with commands\n");
  // handle any responces to send back to connected source
  //client->flush();
  return (!abort_processing);
}

#endif
