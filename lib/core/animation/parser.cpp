#include "animation/parser.h"

#include <nlohmann/json.hpp>

#include "animation/behaviors.h"
#include "animation/palette.h"
#include "animation/storage.h"

namespace core::animation {

using json = nlohmann::json;

// --- helpers ----------------------------------------------------------------

static bool hasString(const json &j, const char *key) {
  return j.contains(key) && j[key].is_string();
}

static bool hasNumber(const json &j, const char *key) {
  return j.contains(key) && j[key].is_number();
}

static bool hasArray(const json &j, const char *key) {
  return j.contains(key) && j[key].is_array();
}

static float getFloat(const json &j, const char *key, float defaultValue = 0.0f) {
  if (hasNumber(j, key)) {
    return j[key].get<float>();
  }

  return defaultValue;
}

static Vec2 getVec2(const json &j, const char *key, Vec2 defaultValue = {0, 0}) {
  if (!hasArray(j, key)) {
    return defaultValue;
  }

  const auto &array = j[key];

  if (array.size() < 2 || !array[0].is_number() || !array[1].is_number()) {
    return defaultValue;
  }

  return {array[0].get<float>(), array[1].get<float>()};
}

static RGBF getRgbf(const json &j, const char *key, RGBF defaultValue = {1, 1, 1}) {
  if (!hasArray(j, key)) {
    return defaultValue;
  }

  const auto &array = j[key];

  if (array.size() < 3 || !array[0].is_number() || !array[1].is_number() || !array[2].is_number()) {
    return defaultValue;
  }

  return {array[0].get<float>(), array[1].get<float>(), array[2].get<float>()};
}

// --- motion -----------------------------------------------------------------

static bool parseMotion(const json &j, Motion &out, std::string &err) {
  if (!hasString(j, "type")) {
    err = "motion: missing 'type'";
    return false;
  }

  std::string type = j["type"].get<std::string>();

  if (type == "static") {
    out = StaticMotion{getVec2(j, "pos", {0.5f, 0.5f})};
  } else if (type == "orbit") {
    OrbitMotion motion;
    motion.center = getVec2(j, "center", {0.5f, 0.5f});
    motion.rx = getFloat(j, "rx", 0.3f);
    motion.ry = getFloat(j, "ry", 0.3f);
    motion.period = getFloat(j, "period", 4.0f);
    motion.phase = getFloat(j, "phase");
    motion.reverse = j.contains("reverse") && j["reverse"].is_boolean() && j["reverse"].get<bool>();
    out = motion;
  } else if (type == "pingpong") {
    PingPongMotion motion;
    motion.a = getVec2(j, "a", {0.0f, 0.5f});
    motion.b = getVec2(j, "b", {1.0f, 0.5f});
    motion.period = getFloat(j, "period", 4.0f);
    out = motion;
  } else if (type == "noise_drift") {
    NoiseDriftMotion motion;
    motion.center = getVec2(j, "center", {0.5f, 0.5f});
    motion.amplitude = getFloat(j, "amplitude", 0.2f);
    motion.speed = getFloat(j, "speed", 1.0f);
    out = motion;
  } else if (type == "lissajous") {
    LissajousMotion motion;
    motion.center = getVec2(j, "center", {0.5f, 0.5f});
    motion.rx = getFloat(j, "rx", 0.3f);
    motion.ry = getFloat(j, "ry", 0.3f);
    motion.freqA = getFloat(j, "freq_a", 1.0f);
    motion.freqB = getFloat(j, "freq_b", 2.0f);
    motion.phase = getFloat(j, "phase");
    motion.speed = getFloat(j, "speed", 1.0f);
    out = motion;
  } else if (type == "spline") {
    SplineMotion motion;

    if (!hasArray(j, "points")) {
      err = "spline motion: missing 'points' array";
      return false;
    }

    const auto &pts = j["points"];
    motion.count = 0;

    for (size_t i = 0; i < pts.size() && motion.count < SplineMotion::kMaxPoints; i++) {
      if (!pts[i].is_array() || pts[i].size() < 2) {
        continue;
      }

      motion.points[motion.count] = {pts[i][0].get<float>(), pts[i][1].get<float>()};
      motion.count++;
    }

    if (motion.count < 2) {
      err = "spline motion: need at least 2 control points";
      return false;
    }

    motion.period = getFloat(j, "period", 4.0f);
    motion.reverse = j.contains("reverse") && j["reverse"].is_boolean() && j["reverse"].get<bool>();
    out = motion;
  } else {
    err = "motion: unknown type '" + type + "'";
    return false;
  }

  return true;
}

// --- color ------------------------------------------------------------------

static bool parsePaletteSlotName(const std::string &name, PaletteSlot &out) {
  if (name == "user_primary") {
    out = PaletteSlot::UserPrimary;
  } else if (name == "user_light") {
    out = PaletteSlot::UserLight;
  } else if (name == "user_dark") {
    out = PaletteSlot::UserDark;
  } else if (name == "user_complement") {
    out = PaletteSlot::UserComplement;
  } else if (name == "user_analogous1") {
    out = PaletteSlot::UserAnalogous1;
  } else if (name == "user_analogous2") {
    out = PaletteSlot::UserAnalogous2;
  } else if (name == "theme_green") {
    out = PaletteSlot::ThemeGreen;
  } else if (name == "theme_gold") {
    out = PaletteSlot::ThemeGold;
  } else if (name == "theme_white") {
    out = PaletteSlot::ThemeWhite;
  } else {
    return false;
  }

  return true;
}

static bool parseColor(const json &j, Color &out, std::string &err) {
  if (!hasString(j, "type")) {
    err = "color: missing 'type'";
    return false;
  }

  std::string type = j["type"].get<std::string>();

  if (type == "static") {
    out = StaticColor{getRgbf(j, "color")};
  } else if (type == "hue_cycle") {
    HueCycleColor color;
    color.fromHue = getFloat(j, "from_hue");
    color.toHue = getFloat(j, "to_hue", 1.0f);
    color.period = getFloat(j, "period", 4.0f);
    color.pingPong = j.contains("ping_pong") && j["ping_pong"].is_boolean() && j["ping_pong"].get<bool>();
    out = color;
  } else if (type == "palette") {
    PaletteColor paletteColor;

    if (!hasArray(j, "colors")) {
      err = "palette color: missing 'colors' array";
      return false;
    }

    const auto &colors = j["colors"];
    paletteColor.count = 0;

    for (size_t i = 0; i < colors.size() && paletteColor.count < PaletteColor::kMaxColors; i++) {
      if (!colors[i].is_array() || colors[i].size() < 3) {
        continue;
      }

      paletteColor.colors[paletteColor.count] = {colors[i][0].get<float>(), colors[i][1].get<float>(),
                                                 colors[i][2].get<float>()};
      paletteColor.count++;
    }

    if (paletteColor.count < 2) {
      err = "palette color: need at least 2 colors";
      return false;
    }

    paletteColor.period = getFloat(j, "period", 4.0f);
    out = paletteColor;
  } else if (type == "palette_slot") {
    if (!hasString(j, "slot")) {
      err = "palette_slot color: missing 'slot'";
      return false;
    }

    PaletteSlot slot;

    if (!parsePaletteSlotName(j["slot"].get<std::string>(), slot)) {
      err = "palette_slot color: unknown slot '" + j["slot"].get<std::string>() + "'";
      return false;
    }

    out = PaletteSlotColor{slot};
  } else {
    err = "color: unknown type '" + type + "'";
    return false;
  }

  return true;
}

// --- scalar -----------------------------------------------------------------

static bool parseScalar(const json &j, Scalar &out, std::string &err, const char *label) {
  // Allow a bare number as shorthand for a static scalar.
  if (j.is_number()) {
    out = Scalar(j.get<float>());
    return true;
  }

  if (!j.is_object()) {
    err = std::string(label) + ": expected number or object";
    return false;
  }

  if (!hasString(j, "type")) {
    err = std::string(label) + ": missing 'type'";
    return false;
  }

  std::string type = j["type"].get<std::string>();

  if (type == "static") {
    out = Scalar(getFloat(j, "value", 1.0f));
  } else if (type == "breathe") {
    BreatheScalar scalar;
    scalar.base = getFloat(j, "base", 1.0f);
    scalar.amplitude = getFloat(j, "amplitude", 0.2f);
    scalar.period = getFloat(j, "period", 3.0f);
    scalar.phase = getFloat(j, "phase");
    out = scalar;
  } else if (type == "noise") {
    NoiseScalar scalar;
    scalar.base = getFloat(j, "base", 1.0f);
    scalar.amplitude = getFloat(j, "amplitude", 0.2f);
    scalar.speed = getFloat(j, "speed", 1.0f);
    out = scalar;
  } else {
    err = std::string(label) + ": unknown type '" + type + "'";
    return false;
  }

  return true;
}

// --- track ------------------------------------------------------------------

static bool parseTrack(const json &j, LightTrack &out, std::string &err) {
  if (j.contains("motion") && j["motion"].is_object()) {
    if (!parseMotion(j["motion"], out.motion, err)) {
      return false;
    }
  }

  if (j.contains("color") && j["color"].is_object()) {
    if (!parseColor(j["color"], out.color, err)) {
      return false;
    }
  }

  if (j.contains("radius")) {
    if (!parseScalar(j["radius"], out.radius, err, "radius")) {
      return false;
    }
  }

  if (j.contains("intensity")) {
    if (!parseScalar(j["intensity"], out.intensity, err, "intensity")) {
      return false;
    }
  }

  out.startTime = getFloat(j, "startTime");
  out.fadeIn = getFloat(j, "fadeIn");
  return true;
}

// --- top-level --------------------------------------------------------------

ParseResult parseAnimationJson(const std::string &jsonStr) {
  ParseResult result;

  auto j = json::parse(jsonStr, nullptr, false);

  if (j.is_discarded()) {
    result.error = "invalid JSON";
    return result;
  }

  if (!j.is_object()) {
    result.error = "top-level value must be an object";
    return result;
  }

  result.def = std::make_unique<AnimationDef>();

  // Name
  if (hasString(j, "name")) {
    std::string name = j["name"].get<std::string>();
    size_t len = name.size();

    if (len >= sizeof(result.def->name)) {
      len = sizeof(result.def->name) - 1;
    }

    memcpy(result.def->name, name.c_str(), len);
    result.def->name[len] = '\0';
  } else {
    memcpy(result.def->name, "custom", 7);
  }

  // Duration
  result.def->duration = getFloat(j, "duration");

  // Tracks
  if (!hasArray(j, "tracks")) {
    result.error = "missing 'tracks' array";
    result.def.reset();
    return result;
  }

  const auto &tracks = j["tracks"];

  if (tracks.empty()) {
    result.error = "'tracks' array is empty";
    result.def.reset();
    return result;
  }

  result.def->trackCount = 0;

  for (size_t i = 0; i < tracks.size() && result.def->trackCount < kMaxLights; i++) {
    if (!tracks[i].is_object()) {
      result.error = "track " + std::to_string(i) + ": expected object";
      result.def.reset();
      return result;
    }

    LightTrack track;
    std::string trackErr;

    if (!parseTrack(tracks[i], track, trackErr)) {
      result.error = "track " + std::to_string(i) + ": " + trackErr;
      result.def.reset();
      return result;
    }

    result.def->tracks[result.def->trackCount++] = track;
  }

  result.ok = true;
  return result;
}

ParseResult loadAndParseAnimation(const char *name) {
  std::string json = storageRead(name);

  if (json.empty()) {
    ParseResult result;
    result.error = std::string("'") + name + "' not found on filesystem";
    return result;
  }

  return parseAnimationJson(json);
}

}  // namespace core::animation
