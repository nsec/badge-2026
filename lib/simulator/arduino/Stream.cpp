#include "Stream.h"
#include "Arduino.h"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/select.h>
#endif

// Global Serial instance
HardwareSerial Serial;

// Stream implementation
int Stream::timedRead() {
  unsigned long start = millis();
  do {
    int c = read();
    if (c >= 0)
      return c;
  } while (millis() - start < _timeout);
  return -1;
}

int Stream::timedPeek() {
  unsigned long start = millis();
  do {
    int c = peek();
    if (c >= 0)
      return c;
  } while (millis() - start < _timeout);
  return -1;
}

size_t Stream::readBytes(char *buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    int c = timedRead();
    if (c < 0)
      break;
    *buffer++ = (char)c;
    count++;
  }
  return count;
}

std::string Stream::readString() {
  std::string ret;
  int c = timedRead();
  while (c >= 0) {
    ret += (char)c;
    c = timedRead();
  }
  return ret;
}

std::string Stream::readStringUntil(char terminator) {
  std::string ret;
  int c = timedRead();
  while (c >= 0 && c != terminator) {
    ret += (char)c;
    c = timedRead();
  }
  return ret;
}

bool Stream::find(const char *target) {
  return findUntil(target, nullptr);
}

bool Stream::findUntil(const char *target, const char *terminator) {
  if (target == nullptr)
    return true;
  size_t targetLen = strlen(target);
  size_t termLen = terminator ? strlen(terminator) : 0;
  size_t index = 0;
  size_t termIndex = 0;

  while (true) {
    int c = timedRead();
    if (c < 0)
      return false;

    if ((char)c == target[index]) {
      if (++index >= targetLen)
        return true;
    } else {
      index = 0;
    }

    if (terminator && (char)c == terminator[termIndex]) {
      if (++termIndex >= termLen)
        return false;
    } else {
      termIndex = 0;
    }
  }
}

// HardwareSerial implementation
#ifndef _WIN32
static struct termios g_original_termios;
static bool g_termios_saved = false;
#endif

void HardwareSerial::setRawMode(bool enable) {
#ifdef _WIN32
  // Windows: nothing special needed, _kbhit/_getch work
  (void)enable;
#else
  if (enable) {
    if (!g_termios_saved) {
      tcgetattr(STDIN_FILENO, &g_original_termios);
      g_termios_saved = true;
    }
    struct termios raw = g_original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  } else {
    if (g_termios_saved) {
      tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
      // Restore blocking mode
      int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
      fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
  }
#endif
}

void HardwareSerial::begin(unsigned long baud) {
  (void)baud;  // Baud rate doesn't matter for stdio
  setRawMode(true);
  _initialized = true;
}

void HardwareSerial::end() {
  setRawMode(false);
  _initialized = false;
}

int HardwareSerial::available() {
  if (!_initialized)
    return 0;

  if (_peeked >= 0)
    return 1;

#ifdef _WIN32
  return _kbhit() ? 1 : 0;
#else
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval tv = {0, 0};
  int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
  return (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) ? 1 : 0;
#endif
}

int HardwareSerial::read() {
  if (!_initialized)
    return -1;

  if (_peeked >= 0) {
    int c = _peeked;
    _peeked = -1;
    return c;
  }

#ifdef _WIN32
  if (_kbhit()) {
    return _getch();
  }
  return -1;
#else
  unsigned char c;
  ssize_t n = ::read(STDIN_FILENO, &c, 1);
  return (n == 1) ? c : -1;
#endif
}

int HardwareSerial::peek() {
  if (!_initialized)
    return -1;

  if (_peeked >= 0)
    return _peeked;

  _peeked = read();
  return _peeked;
}

size_t HardwareSerial::write(uint8_t c) {
  if (!_initialized)
    return 0;
  std::cout << (char)c;
  return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
  if (!_initialized)
    return 0;
  std::cout.write((const char *)buffer, size);
  return size;
}

void HardwareSerial::flush() {
  std::cout.flush();
}
