#pragma once

#include "Print.h"
#include <string>

class Stream : public Print {
public:
  virtual ~Stream() = default;

  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  // Timeout for reading operations (ms)
  void setTimeout(unsigned long timeout) {
    _timeout = timeout;
  }

  unsigned long getTimeout() const {
    return _timeout;
  }

  // Read methods
  size_t readBytes(char *buffer, size_t length);

  size_t readBytes(uint8_t *buffer, size_t length) {
    return readBytes((char *)buffer, length);
  }

  std::string readString();
  std::string readStringUntil(char terminator);

  // Find methods
  bool find(const char *target);
  bool findUntil(const char *target, const char *terminator);

protected:
  unsigned long _timeout = 1000;

  int timedRead();
  int timedPeek();
};

// HardwareSerial wraps stdin/stdout for simulator
class HardwareSerial : public Stream {
public:
  void begin(unsigned long baud);
  void end();

  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  void flush() override;

  operator bool() const {
    return _initialized;
  }

private:
  bool _initialized = false;
  int _peeked = -1;
  void setRawMode(bool enable);
};
