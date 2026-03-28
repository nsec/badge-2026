#include "tasks/display_task.h"

#include <cstring>

#include "hardware/eink.h"
#include "hardware/nsec_logo.h"
#include "system/ota_manager.h"
#include "storage/nvs_social.h"

#include <Fonts/FreeMonoBold9pt7b.h>

namespace {

// Timeout durations in milliseconds for each command type.
constexpr TickType_t TIMEOUT_NFC_SCAN_MS = 15000;
constexpr TickType_t TIMEOUT_PAIR_RESULT_MS = 5000;
constexpr TickType_t TIMEOUT_SOCIAL_PROGRESS_MS = 10000;

}  // namespace

namespace core {

Queue<DisplayCommand> *g_displayQueue = nullptr;

DisplayTask::DisplayTask(Queue<DisplayCommand> &queue)
    : Task("display", badge::config::tasks::priority_display, 8192), _queue(queue), _timer(nullptr) {}

void DisplayTask::run() {
  // Create a one-shot software timer for auto-revert to logo.
  // The timer is not started until a command with a timeout is processed.
  _timer = xTimerCreate("disp_to", pdMS_TO_TICKS(1000), pdFALSE, this, onTimeout);
  configASSERT(_timer);

  DisplayCommand cmd;
  for (;;) {
    if (!_queue.receive(cmd))
      continue;

    if (!hw::einkAvailable())
      continue;

    // Stop any pending revert-to-logo timer.
    xTimerStop(_timer, 0);

    TickType_t timeout = 0;

    switch (cmd.type) {
      case DisplayCommand::Type::ShowLogo:
        showLogo();
        // No timer for logo — it stays indefinitely.
        break;
      case DisplayCommand::Type::ModeChange:
        showModeChange(cmd);
        // No timer — mode screen stays until replaced by a scan result, pair result, or logo.
        break;
      case DisplayCommand::Type::NfcScan:
        showNfcScan(cmd);
        timeout = TIMEOUT_NFC_SCAN_MS;
        break;
      case DisplayCommand::Type::PairResult:
        showPairResult(cmd);
        timeout = TIMEOUT_PAIR_RESULT_MS;
        break;
      case DisplayCommand::Type::SocialProgress:
        showSocialProgress(cmd);
        timeout = TIMEOUT_SOCIAL_PROGRESS_MS;
        break;
    }

    if (timeout > 0) {
      xTimerChangePeriod(_timer, pdMS_TO_TICKS(timeout), 0);
      // xTimerChangePeriod also starts the timer.
    }
  }
}

void DisplayTask::onTimeout(TimerHandle_t timer) {
  // Timer daemon context — must not do SPI/display work here.
  // Send a ShowLogo command to the display queue instead.
  auto *self = static_cast<DisplayTask *>(pvTimerGetTimerID(timer));
  (void)self;

  if (g_displayQueue) {
    DisplayCommand cmd;
    cmd.type = DisplayCommand::Type::ShowLogo;
    g_displayQueue->send(cmd, Milliseconds(0));
  }
}

void DisplayTask::showLogo() {
  auto &display = hw::einkDisplay();
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawInvertedBitmap(0, 0, badge::LOGO_BITMAP, badge::LOGO_WIDTH, badge::LOGO_HEIGHT, GxEPD_BLACK);
  } while (display.nextPage());
}

void DisplayTask::showModeChange(const DisplayCommand &cmd) {
  const char *modeName = "NFC Off";
  const char *modeDetail = "";

  switch (cmd.nfcMode) {
    case 0:
      modeName = "NFC Off";
      modeDetail = "";
      break;
    case 1:
      modeName = "NFC Reader";
      modeDetail = "Scanning...";
      break;
    case 2:
      modeName = "NFC Emulator";
      modeDetail = "Emulating...";
      break;
    case 3:
      modeName = "NFC Pair";
      modeDetail = "Pairing...";
      break;
    default:
      modeName = "NFC Unknown";
      break;
  }

  String label = ota::getRunningPartitionLabel();
  const char *firmware = "Conference";
  if (label == "ctf")
    firmware = "CTF";

  renderText(firmware, modeName, modeDetail[0] ? modeDetail : nullptr);
}

void DisplayTask::showNfcScan(const DisplayCommand &cmd) {
  renderText("Tag Found", cmd.uid, cmd.ndef[0] ? cmd.ndef : nullptr);
}

void DisplayTask::showPairResult(const DisplayCommand &cmd) {
  switch (cmd.pairOutcome) {
    case 0:
      renderText("Pair Result", "New Partner!");
      break;
    case 1:
      renderText("Pair Result", "Already Paired");
      break;
    case 2:
    default:
      renderText("Pair Result", "Pair Failed");
      break;
  }
}

void DisplayTask::showSocialProgress(const DisplayCommand &cmd) {
  auto key = static_cast<storage::SocialKey>(cmd.socialKey);
  const char *name = storage::socialKeyName(key);

  char valueLine[48];
  snprintf(valueLine, sizeof(valueLine), "%s: %u", name, cmd.socialValue);

  renderText("Social", valueLine);
}

void DisplayTask::renderText(const char *line1, const char *line2, const char *line3) {
  auto &display = hw::einkDisplay();
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  // Count how many lines we have.
  int lineCount = 1;
  if (line2)
    lineCount++;
  if (line3)
    lineCount++;

  const char *lines[3] = {line1, line2, line3};

  // Compute vertical positions. Distribute lines evenly across the 200px height.
  // Each line occupies a zone of height = 200 / lineCount.
  // Place text baseline at the vertical center of each zone.
  int16_t tbx, tby;
  uint16_t tbw, tbh;

  // Pre-compute positions for each line.
  uint16_t xPos[3] = {};
  uint16_t yPos[3] = {};
  uint16_t zoneHeight = display.height() / lineCount;

  for (int i = 0; i < lineCount; i++) {
    display.getTextBounds(lines[i], 0, 0, &tbx, &tby, &tbw, &tbh);
    xPos[i] = ((display.width() - tbw) / 2) - tbx;
    // Center baseline vertically within its zone.
    yPos[i] = (zoneHeight * i) + (zoneHeight / 2) + (tbh / 2) - tby - tbh;
  }

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int i = 0; i < lineCount; i++) {
      display.setCursor(xPos[i], yPos[i]);
      display.print(lines[i]);
    }
  } while (display.nextPage());
}

}  // namespace core
