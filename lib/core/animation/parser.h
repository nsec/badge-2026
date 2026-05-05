#pragma once

#include <memory>
#include <string>

#include "animation/engine.h"

namespace core::animation {

/// Result of parsing a JSON animation definition.
struct ParseResult {
  bool ok = false;
  std::string error;
  std::unique_ptr<AnimationDef> def;
};

/// Parse a JSON string into a heap-allocated AnimationDef.
/// On failure, result.ok is false and result.error describes the problem.
ParseResult parseAnimationJson(const std::string &jsonStr);

/// Load an animation by name from the filesystem, parse its JSON, and return
/// a heap-allocated AnimationDef. Convenience wrapper around storageRead + parseAnimationJson.
ParseResult loadAndParseAnimation(const char *name);

}  // namespace core::animation
