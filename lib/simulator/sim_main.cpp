// Native simulator entry point
// Calls Arduino-style setup() and loop()

#include "arduino/Arduino.h"
#include <cstdio>
#include <csignal>

extern "C" void vTaskStartScheduler();

// Forward declarations for Arduino-style functions (defined in main.cpp)
extern void setup();
extern void loop();

// FreeRTOS assert handler
extern "C" void vAssertCalled(const char *file, int line) {
  fprintf(stderr, "FreeRTOS assertion failed: %s:%d\n", file, line);
  std::abort();
}

// Static allocation callbacks for FreeRTOS
extern "C" {

void vApplicationGetIdleTaskMemory(void **ppxIdleTaskTCBBuffer, void **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
  static uint8_t ucIdleTaskTCB[256];
  static uint8_t ucIdleTaskStack[256 * sizeof(void *)];

  *ppxIdleTaskTCBBuffer = ucIdleTaskTCB;
  *ppxIdleTaskStackBuffer = ucIdleTaskStack;
  *pulIdleTaskStackSize = 256;
}

void vApplicationGetTimerTaskMemory(void **ppxTimerTaskTCBBuffer, void **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
  static uint8_t ucTimerTaskTCB[256];
  static uint8_t ucTimerTaskStack[512 * sizeof(void *)];

  *ppxTimerTaskTCBBuffer = ucTimerTaskTCB;
  *ppxTimerTaskStackBuffer = ucTimerTaskStack;
  *pulTimerTaskStackSize = 512;
}

}  // extern "C"

// Signal handler for clean shutdown
static volatile bool g_running = true;

static void signal_handler(int sig) {
  (void)sig;
  g_running = false;
  Serial.end();
  printf("\nShutdown requested.\n");
  std::exit(0);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Call Arduino setup (creates and registers FreeRTOS tasks)
  setup();

  // Start the FreeRTOS scheduler — this does not return.
  // On ESP32, loop() runs inside a FreeRTOS task; since loop() here
  // just calls vTaskDelay(portMAX_DELAY), we skip it and let the
  // scheduler run the tasks created during setup().
  vTaskStartScheduler();

  // Should never get here
  Serial.end();
  return 1;
}
