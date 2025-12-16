#pragma once

namespace conference {

/**
 * Initialize conference-specific modules
 * Called from main.cpp setup()
 */
void init();

/**
 * Poll/update conference modules
 * Called from main.cpp loop()
 */
void tick();

} // namespace conference
