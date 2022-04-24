#ifndef __DISPLAY_H
#define __DISPLAY_H

#define MAX_LINE_TEXT 512 // number of characters we will allow for a wrapping line of text


/*
bool  ioInit();
void  ioLoop();
int   ioPrint(const char *text);
int   ioStreamPrint(Stream *out, const char *line);
int   ioStreamPrintf(Stream *out, const char *fmt, ...);
bool  ioRunCommandLine(const char *commands, bool aborted, bool wait);
//bool  ioRunCommandFile(const char *filename, bool wait);
bool  ioRunCommandPrintf(const char *fmt, ...);
*/

// now the local includes
#include "allincludes.h"
#include <driver/rtc_io.h>
#include "esp_adc_cal.h"

//#include "screenstream.h"
//#include "bufferstream.h"

#define MAX_INPUT_BUFFER (8 * 1024) // a command string or the contents of a command file cannot exceed this size

class ioStream : public Stream
{
  bufferStream *in;
  //screenStream *out;
  char output_line_buffer[MAX_LINE_TEXT + 1];

public:
  //ioStream(bufferStream *ins, screenStream *outs)
  ioStream(bufferStream *ins)
  {
    in = ins;
    //out = outs;
    memset(output_line_buffer, 0, MAX_LINE_TEXT + 1);
  }

  /** Clear the buffers */
  void clear()
  {
    in->clear();
  }

  virtual size_t write(uint8_t b)
  {
    //-aaw- return out->write(b);
    return 1;
  }

  virtual size_t print(const char *text)
  {
    //VERBOSE("ioStream::print (%s)\n", text);
    //-aaw- return out->print(text);
    return in->print(text);
  }

  virtual size_t printf(const char *fmt, ...)
  {
    //VERBOSE("ioStream::printf ()\n");
    va_list args;
    va_start(args, fmt);

    this->output_line_buffer[0] = 0;
    vsnprintf(this->output_line_buffer, MAX_LINE_TEXT, fmt, args);
    this->output_line_buffer[MAX_LINE_TEXT - 1] = 0;
    va_end(args);

    return in->print(this->output_line_buffer);
  }

  virtual void flush()
  {
    in->clear();
  }

  virtual int available()
  {
    return in->available();
  }
  virtual int read()
  {
    return in->read();
  }
  virtual int peek()
  {
    return in->peek();
  }
};

static bufferStream *g_buffer_stream;
static ioStream     *g_io_stream;
static bool         g_io_waiting_for_button;
static bool         g_io_quiet;

/* ---
#### ioInit()

Performs all necessary initialization of both the display and the button(s)
It must be called once before using any of the other display functions.

return: **bool** `true` on success and `false` on failure
--- */
bool ioInit(void)
{
  g_io_quiet      = false;
  g_buffer_stream = NULL;
  g_buffer_stream = new bufferStream(MAX_INPUT_BUFFER);
  if (!g_buffer_stream)
  {
    DEBUGSERIAL.println("ERROR: failed to create Buffer Stream ... so we are pretty much SOL\n");
  }
  else if (g_buffer_stream->availableForWrite() != MAX_INPUT_BUFFER)
  {
    DEBUGSERIAL.printf("ERROR: Input buffer is %d bytes of %d bytes\r\n", g_buffer_stream->availableForWrite(), MAX_INPUT_BUFFER);
  }

  g_io_stream = NULL;
  g_io_stream = new ioStream(g_buffer_stream);
  if (!g_io_stream)
  {
    MESSAGE("ERROR: failed to create I/O Stream ... so we are pretty much SOL\n");
  }

  return true;
}

/* ---
#### ioLoop()

Give the display and button(s) an opportunity to respond to interaction events and update the display
--- */
void ioLoop(void)
{
  //g_screen_stream->refresh();
  //delay(1);
}

// TODO markdown doc for the rest of the io functions

void ioClear(bool home)
{
  //g_screen_stream->clear(home);
}

/* --
#### ioIsMute()

Indicate if the portaprog display is operating in quiet mode - aka supressing status messages.

- output: **bool** true if status messages should be supressed
-- */
/**
int ioPrint(const char *text)
{
  //DEBUGSERIAL.printf("ioPrint(%s)\r\n", text);
  //DEBUGSERIAL.print(text); // most likely any formal output should also go to the debug screen.

  // if we are quiet to the display, we can return now
  return g_screen_stream->print(text);
}
**/
int ioStreamPrint(Stream *out, const char *line)
{
  //DEBUGSERIAL.printf("ioStreamPrint(%s)\r\n", line);
  if ((out != NULL) && (out != g_io_stream))
    out->print(line);

  return strlen(line); // ioPrint(line);
}

int ioStreamPrintf(Stream *out, const char *fmt...)
{
  static char line[MAX_LINE_TEXT + 1];
  va_list args;
  va_start(args, fmt);

  line[0] = 0;
  vsnprintf(line, MAX_LINE_TEXT, fmt, args);
  line[MAX_LINE_TEXT - 1] = 0;
  va_end(args);

  if ((out != NULL) && (out != g_io_stream) )
    out->print(line);

  return strlen(line); // ioPrint(line);
}


bool ioRunCommandLine(const char *line, bool aborted, bool wait)
{
  bool success = true;
  if (line && strlen(line) > 0)
  {
    MESSAGE("CMD: %s\n", line);

    //g_screen_stream->clear(true);
    // store the commands to the input buffer
    if (g_buffer_stream != NULL)
      g_buffer_stream->print(line);

    // BUG ? we need to process the commands "one line at a time"
    success = parserProcessCommands(g_io_stream, aborted);
  }
  return success;
}

bool ioRunCommandPrintf(const char *fmt, ...)
{
  static char line[MAX_LINE_TEXT + 1];
  va_list args;
  va_start(args, fmt);

  line[0] = 0;
  vsnprintf(line, MAX_LINE_TEXT, fmt, args);
  line[MAX_LINE_TEXT - 1] = 0;
  va_end(args);
  return ioRunCommandLine(line, false, false);
}

/*
bool ioRunCommandFile(const char *name, bool wait) {
  // open file. write contents into g_buffer_stream. close file. call parserProcessCommands(g_io_stream);
  bool success = true;
  File f = filesysOpen(name, "r");

  if (f) {
    MESSAGE("Run Command File: %s\n", name);

    while (f.available()) {
      // we need to process the commands "one line at a time"
      char line[MAX_LINE_TEXT + 1];
      uint16_t len = streamReadLine((Stream *)(&f), line, MAX_LINE_TEXT, false);
      if (len) {
        FILESYS_STRIP_NL(line, len);
        if (len) {
          success = ioRunCommandLine(line, !success, false);
        }
      }
    }
    filesysClose(f);
  } else {
    MESSAGE("Unable to run command file %s\n", name);
  }
  if (wait)
    ioWaitForButton(true);
  return true;
}

const char *ioGetBatteryInfo() {

#if 0 // used to debug actual VREF
  int vref;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }
#endif

#define BATTERY_VREF 1150 // default is 1100  .. .this value is just a WAG (sorry)
  static char message[64];
  message[0] = 0;
  uint16_t v = analogRead(NA_BATTERY_ADC);
  //DEBUGSERIAL.printf("analogRead() = %d\n", v);
  float battery_voltage = 0;

  // battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (BATTERY_VREF / 1000.0);
  battery_voltage = ((float)v / 4095.0) * 2.0 * 3.0 * (BATTERY_VREF / 1000.0);

  // KLUDGE when it's on USB, the reported voltage is too high
  if (battery_voltage > 4.3) {
    battery_voltage = battery_voltage * 0.875;
  }

#if 0
  // round to 0.1
  battery_voltage = round((battery_voltage * 10)) / 10;
  snprintf(message, 64, "%3.1fV", battery_voltage);
#else
  // round to 0.05
  battery_voltage = round((battery_voltage * 20)) / 20;
  snprintf(message, 64, "%4.2fV", battery_voltage);
#endif
  return message;
}
*/
/* ---
#### ioWaitForButton()

Wait for a button press/release before continuing

The button state is ignored until it is IDLE then waits for the button release event. The function only returns once the button RELEASE has been detected.

**Note**: This is a _polling function, not interrupt driven_ and this is a _blocking operation_.
--- */

#endif

/*eof*/
