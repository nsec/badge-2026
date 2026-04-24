#pragma once

#include <Stream.h>
#include <cstdint>
#include <string>

namespace challenges {
namespace serialmystery {

void init();
void handleSerialMystery(Stream &stream, const std::string &args);

}  // namespace serialmystery
}  // namespace challenges
