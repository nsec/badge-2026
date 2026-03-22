#pragma once
/**
 * grid.h — "Grid Optimization" QAOA challenge.
 *
 * 10-qubit QAOA at depth p=2.
 * Player tunes 4 parameters (γ₁, γ₂, β₁, β₂) to minimize a cost
 * Hamiltonian via sampling-based evaluation.
 * Successful solve artifacts are canonicalized and stored in NVS
 * for dock-side validation.
 */

#include <Stream.h>
#include <cstdint>
#include <string>

namespace challenges {
namespace grid {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t NUM_QUBITS = 10;
static constexpr uint8_t QAOA_DEPTH = 2;             // p = 2
static constexpr uint8_t NUM_PARAMS = 4;             // γ₁, γ₂, β₁, β₂
static constexpr uint16_t NUM_SAMPLES = 256;         // deterministic shot count
static constexpr uint32_t SAMPLE_SEED = 0xDEADBEEF;  // fixed seed for reproducibility

/// NVS schema version.
static constexpr uint8_t NVS_VERSION = 1;

// ---------------------------------------------------------------------------
// NVS persistence structure (stored as blob, key "grid")
// ---------------------------------------------------------------------------

struct __attribute__((packed)) GridState {
  uint8_t version;            // must equal NVS_VERSION
  uint8_t solved;             // 0 = unsolved, 1 = solved
  int16_t gamma1, gamma2;     // milliradians
  int16_t beta1, beta2;       // milliradians
  int32_t best_energy_milli;  // best sampled energy × 1000
  uint16_t low_energy_hits;   // count of samples below threshold
  int32_t cvar_milli;         // CVaR (bottom 20%) × 1000
  uint32_t checksum;          // CRC-32 of preceding fields
};

// Total: 1+1+2+2+2+2+4+2+4+4 = 24 bytes

// ---------------------------------------------------------------------------
// Evaluation results (returned by evaluate, not persisted as-is)
// ---------------------------------------------------------------------------

struct GridMetrics {
  float best_energy;
  float mean_energy;
  float cvar;           // bottom 20% tail average
  uint16_t low_hits;    // samples with energy < low_threshold
  float low_threshold;  // energy cutoff for "low hits"
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Evaluate QAOA with given params.  Fills metrics struct.
void evaluate(float gamma1, float gamma2, float beta1, float beta2, GridMetrics &out);

/// Get the energy threshold for "solved" (best_energy must be below this).
float solveThreshold();

/// Get the CVaR threshold for "solved" (CVaR must be below this).
float solveCvarThreshold();

/// Get the minimum low-hit count required for "solved".
uint16_t solveMinHits();

/// Is the challenge solved given these metrics?
bool isSolved(const GridMetrics &m);

/// Store a solved result to NVS.  Canonicalizes params, computes checksum.
bool store(float gamma1, float gamma2, float beta1, float beta2, const GridMetrics &m);

/// Load the stored GridState from NVS.  Returns true if valid.
bool load(GridState &out);

/// Erase stored Grid state.
void reset();

/// Handle `quantum grid <subcommand>` from CLI.
void handleCommand(Stream &stream, const std::string &args);

}  // namespace grid
}  // namespace challenges
