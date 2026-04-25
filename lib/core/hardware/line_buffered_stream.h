#pragma once

#include <Arduino.h>
#include <Stream.h>

#ifndef NATIVE_BUILD
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/timers.h"
#endif

namespace core {
namespace hw {

/*
 * Batched stream wrapper for ESP32-S3 USB CDC serial.
 *
 * The HWCDC driver has race conditions that silently discard data.
 * In particular, HWCDC::flush() re-checks isCDC_Connected() and, when
 * the SOF tick-hook races, discards the entire TX ring buffer.
 *
 * This wrapper accumulates output in a userspace buffer and drains
 * to HWCDC in batches via write() only — HWCDC::flush() is never called.
 *
 * Drain triggers:
 *   - flush(): unconditional (echo, prompt, end of command)
 *   - Newline written + >20ms since last drain (progressive output)
 *   - Background timer: drains if data has been idle for >50ms
 *     (safety net that ensures nothing sits unsent indefinitely)
 */
class LineBufferedStream : public Stream {
  static constexpr unsigned long DRAIN_INTERVAL_MS = 20;
  static constexpr unsigned long IDLE_DRAIN_MS = 50;

public:
  explicit LineBufferedStream(Stream &inner) : _inner(inner) {
#ifndef NATIVE_BUILD
    _mutex = xSemaphoreCreateMutex();
    _timer = xTimerCreate("serial_drain", pdMS_TO_TICKS(IDLE_DRAIN_MS), pdTRUE, this, timerCallback);
    xTimerStart(_timer, 0);
#endif
  }

  size_t write(uint8_t c) override {
#ifdef NATIVE_BUILD
    // No buffering needed on native — no USB CDC race conditions.
    return _inner.write(c);
#else
    lock();
    if (_pos < sizeof(_buf)) {
      _buf[_pos++] = c;
    }

    if (c == '\n') {
      maybeTimedDrain();
    }

    updateLastWriteTimestamp();
    unlock();
    return 1;
#endif
  }

  size_t write(const uint8_t *buf, size_t len) override {
#ifdef NATIVE_BUILD
    return _inner.write(buf, len);
#else
    lock();
    bool hasNewline = false;

    for (size_t i = 0; i < len; i++) {
      if (_pos < sizeof(_buf)) {
        _buf[_pos++] = buf[i];
      }

      hasNewline |= buf[i] == '\n';
    }

    if (hasNewline) {
      maybeTimedDrain();
    }

    updateLastWriteTimestamp();
    unlock();
    return len;
#endif
  }

  void flush() override {
#ifdef NATIVE_BUILD
    _inner.flush();
#else
    lock();
    drain();
    unlock();
#endif
  }

  int available() override {
    return _inner.available();
  }

  int read() override {
    return _inner.read();
  }

  int peek() override {
    return _inner.peek();
  }

private:
  Stream &_inner;
  uint8_t _buf[4096];
  size_t _pos = 0;
  unsigned long _lastDrainMs = 0;
  unsigned long _lastWriteMs = 0;

#ifndef NATIVE_BUILD
  SemaphoreHandle_t _mutex = nullptr;
  TimerHandle_t _timer = nullptr;

  static void timerCallback(TimerHandle_t timer) {
    auto *self = static_cast<LineBufferedStream *>(pvTimerGetTimerID(timer));
    self->lock();

    if (self->_pos > 0 && (millis() - self->_lastWriteMs) >= IDLE_DRAIN_MS) {
      self->drain();
    }

    self->unlock();
  }
#endif

  void lock() {
#ifndef NATIVE_BUILD
    if (_mutex)
      xSemaphoreTake(_mutex, portMAX_DELAY);
#endif
  }

  void unlock() {
#ifndef NATIVE_BUILD
    if (_mutex)
      xSemaphoreGive(_mutex);
#endif
  }

  void updateLastWriteTimestamp() {
    _lastWriteMs = millis();
  }

  void drain() {
    if (_pos > 0) {
      _inner.write(_buf, _pos);
      // Never call _inner.flush() here.  HWCDC::flush() re-checks
      // isCDC_Connected() and, if the SOF tick-hook races, discards the
      // entire TX ring buffer — destroying the bytes write() just enqueued.
      _pos = 0;
      _lastDrainMs = millis();
    }
  }

  void maybeTimedDrain() {
    if (_pos > 0 && (millis() - _lastDrainMs) >= DRAIN_INTERVAL_MS) {
      drain();
    }
  }
};

}  // namespace hw
}  // namespace core
