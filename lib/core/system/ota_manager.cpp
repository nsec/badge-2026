#include "ota_manager.h"

#include <Arduino.h>

// ESP-IDF APIs (available in Arduino-ESP32)
#include "esp_ota_ops.h"
#include "esp_partition.h"

namespace {
const char *labelOrUnknown(const esp_partition_t *p) {
  return (p && p->label) ? p->label : "<unknown>";
}

const esp_partition_t *findAppByLabel(const char *label) {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
}
}  // namespace

namespace core {
namespace ota {

void printBootInfo(Stream &io) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();

  io.println("--- Boot info ---");
  io.print("Running partition: ");
  io.println(labelOrUnknown(running));
  io.print("Boot partition:    ");
  io.println(labelOrUnknown(boot));

  if (running) {
    io.print("Running addr/size: 0x");
    io.print((uint32_t)running->address, HEX);
    io.print(" / 0x");
    io.println((uint32_t)running->size, HEX);
  }
  if (boot) {
    io.print("Boot addr/size:    0x");
    io.print((uint32_t)boot->address, HEX);
    io.print(" / 0x");
    io.println((uint32_t)boot->size, HEX);
  }
}

String getRunningPartitionLabel() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running && running->label)
    return String(running->label);
  return String("unknown");
}

bool setNextBoot(BootTarget target, Stream &io) {
  const esp_partition_t *p = nullptr;

  switch (target) {
    case BootTarget::Conference:
      p = findAppByLabel("conference");
      break;
    case BootTarget::Ctf:
      p = findAppByLabel("ctf");
      break;
  }

  if (!p) {
    io.println("Partition label not found. Check partitions/badge_factory_ota.csv labels.");
    return false;
  }

  esp_err_t err = esp_ota_set_boot_partition(p);
  if (err != ESP_OK) {
    io.print("esp_ota_set_boot_partition failed: ");
    io.println((int)err);
    return false;
  }

  io.print("Next boot set to: ");
  io.println(p->label);
  return true;
}

}  // namespace ota
}  // namespace core
