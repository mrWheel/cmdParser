#ifndef __BUFFERSTREAM_H
#define __BUFFERSTREAM_H

// by tracking 'stored_size' and 'unread_size' we are able to rewind the buffer repeat using it
class bufferStream : public Stream
{
private:
  // the input (buffer) properties
  uint8_t *buffer;
  int32_t buffer_size, stored_size;
  int32_t buffer_pos, unread_size;
  uint16_t errors;

public:
  static const uint32_t DEFAULT_SIZE = 256;

  bufferStream(uint32_t buffer_size = bufferStream::DEFAULT_SIZE);
  ~bufferStream();

  // operations to change internal buffer

  // operations to change internal buffer
  void clear();

  // read from internal buffer
  virtual int32_t available();
  virtual int peek();
  virtual int read();


  virtual void flush();
  virtual int availableForWrite(void);
  virtual size_t write(uint8_t);
  int32_t writeStream(Stream *src);
};
/* ---
#### bufferStream::bufferStream()

Implements a compatible stream class where print() and write() operates on the device display.

--- */

bufferStream::bufferStream(uint32_t buffer_size)
{
  VERBOSE("bufferStream::bufferStream\n");

  this->buffer = NULL;
  VERBOSE("Total heap:  %7d bytes\n", ESP.getHeapSize());
  VERBOSE("Free heap:   %7d bytes\n", ESP.getFreeHeap());
  VERBOSE("Total PSRAM: %7d bytes\n", ESP.getPsramSize());
  VERBOSE("Free PSRAM:  %7d bytes\n", ESP.getFreePsram());
  VERBOSE("--------------------------\n");

  // TODO consider making the buffer a list of buffers so they can be allocated within the ESP32 RAM
  if (ESP.getFreePsram() > buffer_size)
    this->buffer = (uint8_t *)ps_malloc(buffer_size);
  if (this->buffer == NULL)
    this->buffer = (uint8_t *)malloc(buffer_size);
  if (this->buffer == NULL)
    this->buffer_size = 0;
  else
    this->buffer_size = buffer_size;

  MESSAGE("Display buffer %d KB\n", (this->buffer_size / 1024));

  VERBOSE("--------------------------\n");

  VERBOSE("Total heap:  %7d bytes\n", ESP.getHeapSize());
  VERBOSE("Free heap:   %7d bytes\n", ESP.getFreeHeap());
  VERBOSE("Total PSRAM: %7d bytes\n", ESP.getPsramSize());
  VERBOSE("Free PSRAM:  %7d bytes\n", ESP.getFreePsram());

  this->clear();

  /*
    this uses a circular buffer:
      the buffer_size is a constant.
      the buffer_pos is the index of the next byte to read
      the unread_size is number of unread bytes
    read from the beginning and write to the end
    wrap at end of the buffer
  */
}

bufferStream::~bufferStream()
{
  free(buffer);
}

void bufferStream::clear()
{
  buffer_pos = 0;
  unread_size = 0;
  stored_size = 0;
  errors = 0;
  memset(buffer, 0, buffer_size);
}

// ------------------------------------------------------
// the following reads from a Stream* and writes to the internal buffer
// ------------------------------------------------------



int32_t bufferStream::available()
{
  if (buffer == NULL)
    return 0;

  return unread_size;
}

int bufferStream::peek()
{
  return ((unread_size == 0) ? -1 : buffer[buffer_pos]);
}

int bufferStream::read()
{
  if (buffer == NULL)
    return -1;
  if (unread_size == 0)
  {
    errors++;
    return -1;
  }
  else
  {
    int ret = buffer[buffer_pos];
    buffer_pos++;
    unread_size--;
    if (buffer_pos == buffer_size)
    {
      buffer_pos = 0;
    }
    errors = 0;
    return ret;
  }
}

size_t bufferStream::write(uint8_t b)
{
  if (buffer == NULL)
    return 0;
  if (unread_size == buffer_size)
  {
    errors++;
    return 0;
  }
  else
  {
    int32_t i = buffer_pos + unread_size;
    if (i >= buffer_size)
    {
      i -= buffer_size;
    }
    buffer[i] = b;
    unread_size++;
    stored_size++;
    return 1;
  }
}

int32_t bufferStream::availableForWrite()
{
  if (buffer == NULL)
    return 0;
  return buffer_size - unread_size;
}

int32_t bufferStream::writeStream(Stream *src)
{
  VERBOSE("loadStream()\n");
  if (buffer == NULL)
    return 0;
  int c;
  int32_t count = 0;

  while ((c = src->read()) >= 0)
  {
    if (this->write(c) == 0)
      errors++;
    else
      count++;
  }
  return count;
}

void bufferStream::flush()
{
}

#endif

/*eof*/
