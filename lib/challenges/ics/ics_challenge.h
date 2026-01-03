#ifndef ICS_H
#define ICS_H

#include <WiFi.h>
#include <PubSubClient.h>

namespace challenges {
namespace ics {

void init();
void tick();

} // namespace ics
} // namespace challenges
#endif