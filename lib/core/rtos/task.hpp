#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace core {

inline constexpr configSTACK_DEPTH_TYPE DEFAULT_STACK_DEPTH = 4096;

class Task {
public:
  Task(const char *name, UBaseType_t priority) : _name(name), _priority(priority) {}

  virtual ~Task() {
    if (_handle) {
      vTaskDelete(_handle);
    }
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;
  Task(Task &&) = delete;
  Task &operator=(Task &&) = delete;

  void start() {
    configASSERT(!_handle);
    BaseType_t ok = xTaskCreate(entry, _name, DEFAULT_STACK_DEPTH, this, _priority, &_handle);
    configASSERT(ok == pdPASS);
  }

  TaskHandle_t handle() const {
    return _handle;
  }

  const char *name() const {
    return _name;
  }

protected:
  virtual void run() = 0;

private:
  static void entry(void *param) {
    static_cast<Task *>(param)->run();
  }

  const char *_name;
  UBaseType_t _priority;
  TaskHandle_t _handle = nullptr;
};

}  // namespace core
