#ifndef __FILE_SYSTEM_H
#define __FILE_SYSTEM_H

//#define MAX_FILENAME_LEN 32
#define MAX_FORMATBYTES 10

#define FILE_TYPE_UNKN  0
#define FILE_TYPE_CMD 1
#define FILE_TYPE_HEX 2

#define FILESYS_STRIP_NL(b, n)   \
  if (n && b[n - 1] == '\n') { \
    n--;                     \
    b[n] = 0;                \
  }

/*
bool filesysInit();
void filesysLoop();

bool filesysExists(const char* name);
void filesysDelete(const char *name);
File filesysOpen(const char *name, const char* mode);
void filesysClose(File handle);
char *filesysGetFileInfo(bool first, bool hidden);
uint8_t filesysGetType(const char *name);
uint8_t filesysReadHex(Stream *handle);
uint16_t streamReadLine(Stream* handle, char *buf, uint16_t size, bool escaped_characters);

bool filesysSaveStart(const char* name);
bool filesysSaveWrite(uint8_t* buf, size_t size);
bool filesysSaveFinish();
*/

/* ***************************************************************************
* File:    filesys.cpp
* Date:    2020.08.09
* Author:  Bradan Lane Studio
*
* This content may be redistributed and/or modified as outlined
* under the MIT License
*
* ******************************************************************************/

/* ---
--------------------------------------------------------------------------
### FILESYS API

Provide access to the SPIFFS file system.

The SPIFFS flash interface is handled by the combination of the display and the UI button(s).

The display starts with the list of available files.

The `TTGOT4` uses 3 buttons for navigation and selection.

The 'TTGO18650' used a 1-button interface with short presses for navigation and long presses for selection.

The **HEXFILE API** provides a series of functions for handling Intel HEX file format and uses Stream I/O
to handle both FILE operations and Stream data with the TCP connection.
--- */



// local includes
#include "allincludes.h"

static File _filesys_upload_file; // a File variable to temporarily store the received file
static int _filesys_upload_size = 0;
static File _spiffs_dir; // the directory list

// we quietly fix filenames to comply the SPIFFS requirements
char *_filesys_fix_name(const char *name)
{
  static char filename[MAX_FILENAME_LEN + 1];

  if (name[0] != '/')
    snprintf(filename, MAX_FILENAME_LEN, "/%s", name);
  else
    snprintf(filename, MAX_FILENAME_LEN, "%s", name);

  return filename;
}


static char *formatBytes(size_t bytes)   // convert sizes in bytes to KB and MB
{
  static char fmt_buf[MAX_FORMATBYTES + 1];
  if (bytes < 1024)
    snprintf(fmt_buf, MAX_FORMATBYTES, "  %4d Bs", bytes);
  else if (bytes < (1024 * 1024))
    snprintf(fmt_buf, MAX_FORMATBYTES, "%6.1f KB", (bytes / 1024.0));
  else if (bytes < (1024 * 1024 * 1024))
    snprintf(fmt_buf, MAX_FORMATBYTES, "%6.1f MB", (bytes / 1024.0 / 1024.0));
  else
    snprintf(fmt_buf, MAX_FORMATBYTES, "%6.1f GB", (bytes / 1024.0 / 1024.0 / 1024.0));

  fmt_buf[MAX_FORMATBYTES] = 0;
  return fmt_buf;
}

bool filesysInit()   // Start the SPIFFS and list all contents
{
  bool success = false;
  // attempt to start
  success = SPIFFS.begin();
  if (!success)
    success = SPIFFS.begin(true); // attempt to format SPIFFS

  if (success)   // Start the SPI Flash File System (SPIFFS)
  {
    _spiffs_dir = SPIFFS.open("/", "r");

    DEBUGSERIAL.println("SPIFFS started. Contents:");
    char *info = filesysGetFileInfo(true, true);
    while (info)
    {
      DEBUGSERIAL.printf("\t%s\n", info);
      info = filesysGetFileInfo(false, true);
    }
    DEBUGSERIAL.printf("\n");
    return true;
  }
  DEBUGSERIAL.println("SPIFFS failed.");
  return false;
}

void filesysLoop()
{
  // currently nothing to do
}

bool filesysSaveStart(const char *name)
{

  char *filename = _filesys_fix_name(name);
  _filesys_upload_size = 0;
  DEBUGSERIAL.printf("handleFileUpload Name: %s\n", filename);
  _filesys_upload_file = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)

  if (_filesys_upload_file)
    return true;
  return false;
}

bool filesysSaveWrite(uint8_t *buf, size_t size)
{
  if (_filesys_upload_file)
  {
    _filesys_upload_file.write(buf, size); // Write the received bytes to the file
    _filesys_upload_size += size;
  }
  return true; // TODO check return code and/or byte count and return appropriate status
}

bool filesysSaveFinish()
{
  if (_filesys_upload_file)
  {
    _filesys_upload_file.close(); // Close the file again
    DEBUGSERIAL.printf("Upload %d bytes\n", _filesys_upload_size);
    _filesys_upload_size = 0;
    return true;
  }
  return false;
}

bool filesysExists(const char *name)
{
  char *filename = _filesys_fix_name(name);
  return SPIFFS.exists(filename);
}

void filesysDelete(const char *name)
{
  char *filename = _filesys_fix_name(name);
  SPIFFS.remove(filename);
}

File filesysOpen(const char *name, const char *mode)
{
  if (mode == NULL)
    mode = "r";
  char *filename = _filesys_fix_name(name);
  File f = SPIFFS.open(filename, mode); // Open the file
  if (!f)
  {
    MESSAGE("Failed to open %s for mode[%s]\n", name, mode);
  }
  return f;
  //return SPIFFS.open(name, FILE_READ); // Open the file
}


void filesysClose(File f)
{
  f.close();
  VERBOSE("\n");
}

char *filesysGetFileInfo(bool first, bool show_hidden)
{
  static char buf[MAX_FILENAME_LEN + MAX_FORMATBYTES + 1];
  File file;

  bool found;
  do
  {
    buf[0] = 0;
    found = false;
    if (first)
    {
      _spiffs_dir.rewindDirectory();
      first = false;
    }

    file = _spiffs_dir.openNextFile();

    if (file)
    {
      const char *name = file.name();
      if (name[0] == '/')
        name++;
      // skip hidden files unless told to include them
      if ((name[0] == '.') && !show_hidden)
        continue;

      if (file.isDirectory())
      {
        snprintf(buf, (MAX_FILENAME_LEN + MAX_FORMATBYTES), "[%s]", name);
      }
      else
        snprintf(buf, (MAX_FILENAME_LEN + MAX_FORMATBYTES), "%s\t%s", name, formatBytes(file.size()));
      found = true;
    }
    else
      break;
  } while (!found);

  if (buf[0])
    return buf;
  return 0;
}

uint8_t filesysGetType(const char *name)
{
  // KLUDGE - we don't bother attempting all possible cases and we dont bother createing a temporary buffer to uppercase
  if (strstr(name, ".HEX") || strstr(name, ".hex"))
    return FILE_TYPE_HEX;
  if (strstr(name, ".CMD") || strstr(name, ".cmd"))
    return FILE_TYPE_CMD;
  else
    return FILE_TYPE_UNKN;
}

uint16_t streamReadLine(Stream *handle, char *buf, uint16_t size, bool escaped_characters)
{
  /*
    we need the next line but without any newline or linefeed
    rather than attempt to remove trailing newline or linefeed when reading
    we remove it when reading the next line. that way, we do not need to
    roll back when we read a valid character.
  */
  size--; // save space for a null terminator
  uint16_t len = 0;
  bool special_char = false;

  while (handle->available())
  {
    int c;
    c = handle->read();
    VERBOSE("%c", c);

    // we need to **optionally** support 'escaped shecial characters since the user may need to manually indicate a newline
    if (escaped_characters)
    {
      if (special_char)
      {
        if (c == 'n')
          c = '\n';
        special_char = false;
      }
      else
      {
        if (c == '\\')
        {
          special_char = true;
          continue;
        }
      }
    }

    if ((c == '\r'))
      continue;

    buf[len++] = c;
    if (len >= size)
      break;
    if (c == '\n')
      break;
  }
  buf[len] = 0;
  return len;
}

#endif
