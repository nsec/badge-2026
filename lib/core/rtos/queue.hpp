#pragma once

#include <chrono>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace core {

using Milliseconds = std::chrono::milliseconds;
inline constexpr Milliseconds WAIT_FOREVER = Milliseconds::max();

namespace detail {

inline TickType_t toTicks(Milliseconds ms) {
  if (ms == WAIT_FOREVER) {
    return portMAX_DELAY;
  }
  return pdMS_TO_TICKS(ms.count());
}

}  // namespace detail

template <typename T> class Queue {
public:
  explicit Queue(UBaseType_t depth) : _handle(xQueueCreate(depth, sizeof(T))) {
    configASSERT(_handle);
  }

  ~Queue() {
    vQueueDelete(_handle);
  }

  Queue(const Queue &) = delete;
  Queue &operator=(const Queue &) = delete;
  Queue(Queue &&) = delete;
  Queue &operator=(Queue &&) = delete;

  bool send(const T &item, Milliseconds timeout = WAIT_FOREVER) {
    return xQueueSend(_handle, &item, detail::toTicks(timeout)) == pdTRUE;
  }

  bool sendFromISR(const T &item, BaseType_t *woken = nullptr) {
    return xQueueSendFromISR(_handle, &item, woken) == pdTRUE;
  }

  bool receive(T &item, Milliseconds timeout = WAIT_FOREVER) {
    return xQueueReceive(_handle, &item, detail::toTicks(timeout)) == pdTRUE;
  }

  QueueHandle_t handle() const {
    return _handle;
  }

private:
  QueueHandle_t _handle;
};

}  // namespace core
