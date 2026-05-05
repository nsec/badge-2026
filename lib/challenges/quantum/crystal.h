#pragma once
/**
 * crystal.h — "Crystal Tuning" VQE challenge.
 *
 * 8-qubit variational quantum eigensolver.
 * Player tunes 16 parameters (2-layer RY + CZ-chain ansatz) to minimize
 * a Transverse-Field Ising Model Hamiltonian.  Successful solve artifacts
 * are canonicalized and stored in NVS for dock-side validation.
 */

#include <Stream.h>
#include <cstdint>
#include <string>

namespace challenges {
namespace crystal {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t NUM_QUBITS = 8;
static constexpr uint8_t NUM_LAYERS = 2;
static constexpr uint8_t NUM_PARAMS = NUM_QUBITS * NUM_LAYERS;  // 16

/// NVS schema version — increment to invalidate old saves.
static constexpr uint8_t NVS_VERSION = 3;

// ---------------------------------------------------------------------------
// NVS persistence structure (stored as blob, key "crystal")
// ---------------------------------------------------------------------------

struct __attribute__((packed)) CrystalState {
  uint8_t version;             // must equal NVS_VERSION
  uint8_t solved;              // 0 = unsolved, 1 = solved
  int16_t params[NUM_PARAMS];  // milliradians (16 × 2 bytes = 32 bytes)
  int32_t energy_milli;        // best energy × 1000
  uint32_t checksum;           // CRC-32 of preceding fields
};

// Total: 1 + 1 + 32 + 4 + 4 = 42 bytes

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Evaluate the Crystal Hamiltonian energy for given parameters (radians).
/// Returns ⟨ψ(θ)|H|ψ(θ)⟩.
float evaluate(const float params[NUM_PARAMS]);

/// Get the energy threshold below which the challenge is considered solved.
float solveThreshold();

/// Get the exact ground-state energy (for dock validation / display).
float groundStateEnergy();

/// Store a solved result to NVS.  Canonicalizes params, computes checksum.
/// Returns true on success.
bool store(const float params[NUM_PARAMS], float energy);

/// Load the stored CrystalState from NVS.  Returns true if valid data exists.
bool load(CrystalState &out);

/// Erase the stored Crystal state from NVS.
void reset();

/// Handle `quantum crystal <subcommand>` from CLI.
void handleCommand(Stream &stream, const std::string &args);

}  // namespace crystal
}  // namespace challenges
