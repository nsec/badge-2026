#include "network/wifi_portal_task.h"

#ifndef NATIVE_BUILD

  #include <Arduino.h>
  #include <WiFi.h>
  #include <DNSServer.h>
  #include <ESPAsyncWebServer.h>
  #include <nlohmann/json.hpp>

  #include "hardware/hwid.h"
  #include "network/web_page.h"
  #include "storage/nvs_config.h"

namespace {

DNSServer dnsServer;
AsyncWebServer webServer(80);

using json = nlohmann::json;

core::storage::UserConfig parseConfigJson(const uint8_t *data, size_t len) {
  core::storage::UserConfig cfg = core::storage::configRead();
  auto reply = json::parse(data, data + len, nullptr, false);
  if (reply.is_discarded()) {
    return cfg;
  }

  if (reply.contains("name") && reply["name"].is_string()) {
    std::string name = reply["name"].get<std::string>();
    if (name.size() > core::storage::CONFIG_NAME_MAX_LEN)
      name.resize(core::storage::CONFIG_NAME_MAX_LEN);
    strlcpy(cfg.name, name.c_str(), sizeof(cfg.name));
  }

  if (reply.contains("r") && reply["r"].is_number_unsigned()) {
    cfg.favoriteColor.r = reply["r"].get<uint8_t>();
  }

  if (reply.contains("g") && reply["g"].is_number_unsigned()) {
    cfg.favoriteColor.g = reply["g"].get<uint8_t>();
  }

  if (reply.contains("b") && reply["b"].is_number_unsigned()) {
    cfg.favoriteColor.b = reply["b"].get<uint8_t>();
  }

  if (reply.contains("brightness") && reply["brightness"].is_number_unsigned()) {
    cfg.brightness = reply["brightness"].get<uint8_t>();
  }

  return cfg;
}

}  // namespace

namespace core {

void PortalTask::startPortal() {
  if (_running)
    return;

  // Build unique SSID from last 2 bytes of MAC
  uint8_t mac[hw::MAC_LEN];
  hw::getHwidMac(mac);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "NSEC-%02X%02X", mac[4], mac[5]);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[portal] AP started: %s @ %s\r\n", ssid, apIP.toString().c_str());

  // Captive portal DNS: resolve everything to our IP
  dnsServer.start(53, "*", apIP);

  // Serve the config page
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", web_page::HTML);
  });

  // Captive portal detection endpoints — redirect to our page
  webServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  webServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  webServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  // GET config
  webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    storage::UserConfig cfg = storage::configRead();
    // clang-format off
    json prefs = {
        {"name", cfg.name},
        {"r", cfg.favoriteColor.r},
        {"g", cfg.favoriteColor.g},
        {"b", cfg.favoriteColor.b},
        {"brightness", cfg.brightness},
    };
    // clang-format on

    request->send(200, "application/json", prefs.dump().c_str());
  });

  // GET social progress
  webServer.on("/api/social", HTTP_GET, [](AsyncWebServerRequest *request) {
    json progress = {
        {"social", storage::socialRead(storage::SocialKey::Social)},
        {"sponsor", storage::socialRead(storage::SocialKey::Sponsor)},
        {"light", storage::socialRead(storage::SocialKey::Light)},
        {"attraction", storage::socialRead(storage::SocialKey::Attraction)},
    };

    request->send(200, "application/json", progress.dump().c_str());
  });

  // POST config — parse body, write NVS, notify controller
  Queue<ControllerEvent> *ctrlQueue = &_controllerQueue;
  webServer.on(
      "/api/config", HTTP_POST,
      [](AsyncWebServerRequest *request) {
      },        // headers only
      nullptr,  // upload handler
      [ctrlQueue](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        storage::UserConfig cfg = parseConfigJson(data, len);
        storage::configWrite(cfg);
        Serial.printf("[portal] Config saved: name=%s color=(%u,%u,%u) bright=%u\r\n", cfg.name, cfg.favoriteColor.r,
                      cfg.favoriteColor.g, cfg.favoriteColor.b, cfg.brightness);

        // Notify the controller
        ConfigChangedEvent evt;
        evt.config = cfg;
        ctrlQueue->send(evt, Milliseconds(100));

        request->send(200, "application/json", "{\"ok\":true}");
      });

  // Catch-all for captive portal
  webServer.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  webServer.begin();
  _running = true;
  Serial.println("[portal] Web server started");
}

void PortalTask::stopPortal() {
  if (!_running) {
    return;
  }

  webServer.end();
  webServer.reset();  // clear all registered route handlers
  dnsServer.stop();
  WiFi.softAPdisconnect(true);  // kick clients and stop broadcasting
  _running = false;
  Serial.println("[portal] Stopped");
}

void PortalTask::run() {
  for (;;) {
    PortalCommand cmd;
    if (_running) {
      /*
       * While running, process DNS and check for commands with a short timeout.
       * The DNS server doesn't use async APIs, so we have to call processNextRequest()
       * regularly to keep it responsive.
       *
       * If a Stop command comes in, we won't call it again until restarted.
       * It feels a bit clunky and inefficient, but running as an AP is already pretty
       * power-hungry and we expect the portal to be used for short periods.
       */
      dnsServer.processNextRequest();
      if (_cmdQueue.receive(cmd, Milliseconds(50))) {
        if (cmd == PortalCommand::Stop)
          stopPortal();
      }
    } else {
      // While stopped, block on the command queue
      if (_cmdQueue.receive(cmd)) {
        if (cmd == PortalCommand::Start)
          startPortal();
      }
    }
  }
}

}  // namespace core

#else  // NATIVE_BUILD

namespace core {

void PortalTask::run() {
  // No WiFi in simulator; just drain the queue
  for (;;) {
    PortalCommand cmd;
    _cmdQueue.receive(cmd);
  }
}

}  // namespace core

#endif
