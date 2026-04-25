#include "animation/storage.h"

#include <algorithm>

#ifdef NATIVE_BUILD

  #include <filesystem>
  #include <fstream>
  #include <sstream>

namespace core::animation {

static const std::filesystem::path kAnimDir = "animations";

bool storageInit() {
  return std::filesystem::is_directory(kAnimDir);
}

std::vector<std::string> storageList() {
  std::vector<std::string> names;

  if (!std::filesystem::is_directory(kAnimDir)) {
    return names;
  }

  for (const auto &entry : std::filesystem::directory_iterator(kAnimDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    auto ext = entry.path().extension().string();

    if (ext != ".json") {
      continue;
    }

    names.push_back(entry.path().stem().string());
  }

  std::sort(names.begin(), names.end());
  return names;
}

std::string storageRead(const char *name) {
  auto path = kAnimDir / (std::string(name) + ".json");
  std::ifstream file(path);

  if (!file.is_open()) {
    return {};
  }

  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

}  // namespace core::animation

#else  // ESP32

  #include <Arduino.h>
  #include <SPIFFS.h>

namespace core::animation {

bool storageInit() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[anim] SPIFFS mount failed");
    return false;
  }

  return true;
}

std::vector<std::string> storageList() {
  std::vector<std::string> names;

  File root = SPIFFS.open("/");

  if (!root) {
    return names;
  }

  File entry = root.openNextFile();

  while (entry) {
    std::string fname = entry.name();
    // entry.name() may return "boot.json" or "/boot.json" depending on
    // the ESP32 Arduino core version.  Handle both by finding the last '/'
    // and stripping the .json suffix.
    const std::string suffix = ".json";

    if (fname.size() > suffix.size() && fname.compare(fname.size() - suffix.size(), suffix.size(), suffix) == 0) {
      size_t nameStart = fname.rfind('/');
      nameStart = (nameStart == std::string::npos) ? 0 : nameStart + 1;
      names.push_back(fname.substr(nameStart, fname.size() - nameStart - suffix.size()));
    }

    entry = root.openNextFile();
  }

  std::sort(names.begin(), names.end());
  return names;
}

std::string storageRead(const char *name) {
  std::string path = std::string("/") + name + ".json";

  File file = SPIFFS.open(path.c_str(), "r");

  if (!file) {
    return {};
  }

  std::string contents;
  contents.reserve(file.size());

  while (file.available()) {
    contents += static_cast<char>(file.read());
  }

  file.close();
  return contents;
}

}  // namespace core::animation

#endif
