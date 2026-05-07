#pragma once

#include <cstdbool>

namespace challenges {
namespace ics {
namespace drm {

bool passthrough(bool x);
bool invert(bool x);
bool permit(bool x);
bool deny(bool x);

}  // namespace drm
}  // namespace ics
}  // namespace challenges
