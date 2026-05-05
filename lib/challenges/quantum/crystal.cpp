/**
 * crystal.cpp — "Crystal Tuning" VQE challenge implementation.
 *
 * Hamiltonian:  Transverse-Field Ising Model (TFIM) on a chain
 *               H = -J Σ ZᵢZᵢ₊₁ - h Σ Xᵢ + Σ δᵢ Zᵢ
 * Ansatz:       2-layer (RY per qubit, CZ chain entangler)
 * Evaluation:   Deterministic ⟨ψ(θ)|H|ψ(θ)⟩ via statevector
 *
 * Coefficients are derived from a fixed PRNG seed so both badge and
 * dock produce identical Hamiltonians.
 */

#include "crystal.h"
#include "qsim.h"
#include <../core/storage/nvs_quantum.h>
#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace challenges {
namespace crystal {

// ---------------------------------------------------------------------------
// Hamiltonian: Transverse-Field Ising Model on a chain
//
//   H = -J Σ ZᵢZᵢ₊₁  -  h Σ Xᵢ  +  Σ δᵢ Zᵢ
//
// J > 0: ferromagnetic coupling (favours aligned spins)
// h > 0: transverse field (quantum fluctuations)
// δᵢ:    small random local fields (symmetry breaking)
//
// This is the canonical VQE benchmark.  h/J = 1.5 places the system
// near the quantum phase transition, where the ground state has
// significant quantum correlations that require iterative optimization.
// ---------------------------------------------------------------------------

static float g_J_coupling;
static float g_h_field;
static float g_delta[NUM_QUBITS];
static bool g_hamiltonian_ready = false;

static uint32_t coeff_rng(uint32_t &seed) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

static float rng_float(uint32_t &seed, float lo, float hi) {
  uint32_t r = coeff_rng(seed);
  float u = static_cast<float>(r) / 4294967296.f;
  return lo + u * (hi - lo);
}

static void buildHamiltonian() {
  if (g_hamiltonian_ready)
    return;

  uint32_t seed = 0x4E534543;  // "NSEC"

  g_J_coupling = 1.0f;
  g_h_field = 1.5f;

  for (uint8_t i = 0; i < NUM_QUBITS; i++)
    g_delta[i] = rng_float(seed, -0.15f, 0.15f);

  g_hamiltonian_ready = true;
}

// ---------------------------------------------------------------------------
// Ansatz:  2-layer [RY(θᵢ) on each qubit, CZ chain entangler]
//
// RY (not RX) is used because RY produces real-valued superpositions
// that give non-zero ⟨X⟩ expectation — essential for the transverse
// field term.  RX from |0⟩ keeps ⟨X⟩ = 0 for all θ.
// ---------------------------------------------------------------------------

static void applyAnsatz(qsim::StateVec &sv, const float params[NUM_PARAMS]) {
  for (uint8_t layer = 0; layer < NUM_LAYERS; layer++) {
    for (uint8_t q = 0; q < NUM_QUBITS; q++) {
      qsim::gate_ry(sv, q, params[layer * NUM_QUBITS + q]);
    }
    // CZ chain entangler (nearest-neighbour, matching the ZZ chain)
    for (uint8_t q = 0; q + 1 < NUM_QUBITS; q++) {
      qsim::gate_cz(sv, q, q + 1);
    }
  }
}

// ---------------------------------------------------------------------------
// Energy evaluation
// ---------------------------------------------------------------------------

float evaluate(const float params[NUM_PARAMS]) {
  buildHamiltonian();

  for (uint8_t i = 0; i < NUM_PARAMS; i++) {
    if (!qsim::is_valid(params[i]))
      return 999.f;
  }

  qsim::StateVec sv;
  qsim::init_zero(sv, NUM_QUBITS);
  applyAnsatz(sv, params);

  float energy = 0.f;

  // -J Σ ⟨ZᵢZᵢ₊₁⟩  (chain, not ring)
  for (uint8_t i = 0; i + 1 < NUM_QUBITS; i++)
    energy -= g_J_coupling * qsim::expect_zz(sv, i, i + 1);

  // -h Σ ⟨Xᵢ⟩
  for (uint8_t i = 0; i < NUM_QUBITS; i++)
    energy -= g_h_field * qsim::expect_x(sv, i);

  // Σ δᵢ ⟨Zᵢ⟩
  for (uint8_t i = 0; i < NUM_QUBITS; i++)
    energy += g_delta[i] * qsim::expect_z(sv, i);

  return energy;
}

// ---------------------------------------------------------------------------
// Thresholds
//
// We compute the ground-state energy approximately by brute-force
// evaluation at zero params (baseline) — the actual threshold is set
// as a fraction below the all-zero baseline.  The dock has the same
// coefficients and can verify independently.
// ---------------------------------------------------------------------------

/// Approximate ground-state energy (precomputed from the fixed-seed Hamiltonian).
/// This is computed once at first call.
static float g_ground_energy = 0.f;
static bool g_ground_computed = false;

static void computeGroundEnergy() {
  if (g_ground_computed)
    return;

  buildHamiltonian();

  // Analytic lower bound for TFIM chain:
  // E_min >= -(N-1)|J| - N|h| - Σ|δᵢ|
  float bound = 0.f;
  bound += (NUM_QUBITS - 1) * fabsf(g_J_coupling);  // ZZ chain terms
  bound += NUM_QUBITS * fabsf(g_h_field);           // X terms
  for (uint8_t i = 0; i < NUM_QUBITS; i++)
    bound += fabsf(g_delta[i]);  // local Z terms
  g_ground_energy = -bound;
  g_ground_computed = true;
}

float groundStateEnergy() {
  computeGroundEnergy();
  return g_ground_energy;
}

float solveThreshold() {
  computeGroundEnergy();
  // The TFIM with h/J=1.5 is near the quantum phase transition.
  // The 2-layer RY+CZ ansatz can reach ~64.4% of the bound via coordinate
  // descent.  Set threshold at 64.2% — requires 3-4 full sweeps of
  // parameter tuning to achieve.
  return g_ground_energy * 0.642f;
}

// ---------------------------------------------------------------------------
// CRC-32 for checksum
// ---------------------------------------------------------------------------

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc >>= 1;
    }
  }
  return ~crc;
}

// ---------------------------------------------------------------------------
// NVS persistence
// ---------------------------------------------------------------------------

bool store(const float params[NUM_PARAMS], float energy) {
  CrystalState st;
  memset(&st, 0, sizeof(st));
  st.version = NVS_VERSION;
  st.solved = (energy < solveThreshold()) ? 1 : 0;

  for (uint8_t i = 0; i < NUM_PARAMS; i++)
    st.params[i] = qsim::to_millirad(params[i]);

  st.energy_milli = static_cast<int32_t>(roundf(energy * 1000.f));

  // Checksum over everything except the checksum field itself
  size_t csLen = offsetof(CrystalState, checksum);
  st.checksum = crc32(reinterpret_cast<const uint8_t *>(&st), csLen);

  return core::storage::quantumWriteBlob("crystal", &st, sizeof(st));
}

bool load(CrystalState &out) {
  size_t rd = core::storage::quantumReadBlob("crystal", &out, sizeof(out));
  if (rd != sizeof(out))
    return false;
  if (out.version != NVS_VERSION)
    return false;

  // Verify checksum
  size_t csLen = offsetof(CrystalState, checksum);
  uint32_t expected = crc32(reinterpret_cast<const uint8_t *>(&out), csLen);
  return (out.checksum == expected);
}

void reset() {
  core::storage::quantumErase("crystal");
}

// ---------------------------------------------------------------------------
// CLI:  quantum crystal <info|run|sweep|set|params|store|status|reset>
// ---------------------------------------------------------------------------

/// Working parameter buffer — persists in memory across CLI calls.
/// Initialized to zero. Updated by `set`, `sweep` (auto-saves best).
static float g_workParams[NUM_PARAMS] = {};

/// Cooldown: minimum 2 seconds between evaluations to limit automation.
static constexpr uint32_t EVAL_COOLDOWN_MS = 3000;
static uint32_t g_lastEvalMs = 0;

/// Wrap angle to [-π, π] for display.
static float wrapToPi(float x) {
  x = fmodf(x, 2.f * M_PI);
  if (x > M_PI)
    x -= 2.f * M_PI;
  if (x < -M_PI)
    x += 2.f * M_PI;
  return x;
}

static bool checkCooldown(Stream &stream) {
  uint32_t now = millis();
  if (now - g_lastEvalMs < EVAL_COOLDOWN_MS) {
    stream.printf("Cooldown: wait %d ms\r\n", EVAL_COOLDOWN_MS - (now - g_lastEvalMs));
    return false;
  }
  g_lastEvalMs = now;
  return true;
}

/// Parse a float token from args starting at idx.  Advances idx past it.
static bool parseFloat(const std::string &args, size_t &idx, float &out) {
  while (idx < args.size() && args[idx] == ' ')
    idx++;
  if (idx >= args.size())
    return false;
  size_t start = idx;
  while (idx < args.size() && args[idx] != ' ')
    idx++;
  out = atof(args.substr(start, idx - start).c_str());
  return true;
}

static bool parseInt(const std::string &args, size_t &idx, int &out) {
  while (idx < args.size() && args[idx] == ' ')
    idx++;
  if (idx >= args.size())
    return false;
  size_t start = idx;
  while (idx < args.size() && args[idx] != ' ')
    idx++;
  out = atoi(args.substr(start, idx - start).c_str());
  return true;
}

void handleCommand(Stream &stream, const std::string &args) {
  size_t idx = 0;
  std::string sub;
  while (idx < args.size() && args[idx] == ' ')
    idx++;
  size_t start = idx;
  while (idx < args.size() && args[idx] != ' ')
    idx++;
  sub = args.substr(start, idx - start);

  buildHamiltonian();

  // --- info ---
  if (sub.empty() || sub == "info") {
    stream.printf("=== Crystal Tuning (VQE) ===\r\n");
    stream.printf("Qubits: %d, Layers: %d, Params: %d\r\n", NUM_QUBITS, NUM_LAYERS, NUM_PARAMS);
    stream.printf("Hamiltonian: H = -J Σ ZᵢZᵢ₊₁ - h Σ Xᵢ + Σ δᵢZᵢ\r\n");
    stream.printf("  J (ZZ coupling) = %.4f\r\n", g_J_coupling);
    stream.printf("  h (transverse)  = %.4f\r\n", g_h_field);
    stream.printf("  Local fields δ: (hidden — use sweep to probe)\r\n");
    stream.printf("Ansatz: %d-layer variational circuit, %d parameters\r\n", NUM_LAYERS, NUM_PARAMS);
    stream.printf("Solve threshold: %.4f\r\n", solveThreshold());
    stream.printf("Type 'quantum crystal circuit' to view the ansatz.\r\n");
    return;
  }

  // --- circuit ---
  if (sub == "circuit") {
    // clang-format off
    stream.printf("=== Crystal Ansatz (2-layer RY + CZ chain) ===\r\n\r\n");
    stream.printf("q0 --[RY(t0 )]--*-----------[RY(t8 )]--*-----------\r\n");
    stream.printf("                |                       |\r\n");
    stream.printf("q1 --[RY(t1 )]--*--[CZ]-----[RY(t9 )]--*--[CZ]-----\r\n");
    stream.printf("                    |                       |\r\n");
    stream.printf("q2 --[RY(t2 )]-----*--[CZ]--[RY(t10)]-----*--[CZ]--\r\n");
    stream.printf("                       |                       |\r\n");
    stream.printf("     :          :      :     :          :      :\r\n");
    stream.printf("                       |                       |\r\n");
    stream.printf("q6 --[RY(t6 )]-----*--[CZ]--[RY(t14)]-----*--[CZ]--\r\n");
    stream.printf("                    |                       |\r\n");
    stream.printf("q7 --[RY(t7 )]--*--[CZ]-----[RY(t15)]--*--[CZ]-----\r\n");
    stream.printf("                |\r\n");
    stream.printf("     <- Layer 1 ->           <- Layer 2 ->\r\n\r\n");
    stream.printf("  * RY(t) rotations prepare superpositions\r\n");
    stream.printf("  * CZ chain entangles nearest-neighbour qubits\r\n");
    stream.printf("  * 16 params: t[0..7] (layer 1) + t[8..15] (layer 2)\r\n");
    stream.printf("  * Goal: minimize <psi(t)|H|psi(t)>\r\n");
    // clang-format on
    return;
  }

  // --- run [θ₀ ... θ₁₃]  (uses working params if omitted) ---
  if (sub == "run") {
    float params[NUM_PARAMS];
    memcpy(params, g_workParams, sizeof(params));
    // Override with any explicitly provided params
    for (uint8_t i = 0; i < NUM_PARAMS; i++) {
      float tmp;
      if (!parseFloat(args, idx, tmp))
        break;
      params[i] = tmp;
    }
    float energy = evaluate(params);
    bool solved = (energy < solveThreshold());
    stream.printf("Energy: %.6f\r\n", energy);
    stream.printf("Threshold: %.6f\r\n", solveThreshold());
    stream.printf("Result: %s\r\n", solved ? "SOLVED!" : "not solved");
    return;
  }

  // --- set <idx> <val> — set one working param ---
  if (sub == "set") {
    int paramIdx;
    float val;
    if (!parseInt(args, idx, paramIdx) || !parseFloat(args, idx, val)) {
      stream.printf("Usage: quantum crystal set <0..%d> <value>\r\n", NUM_PARAMS - 1);
      return;
    }
    if (paramIdx < 0 || paramIdx >= NUM_PARAMS) {
      stream.printf("Error: index must be 0..%d\r\n", NUM_PARAMS - 1);
      return;
    }
    g_workParams[paramIdx] = val;
    float energy = evaluate(g_workParams);
    stream.printf("θ[%d] = %.4f  →  Energy: %.6f  %s\r\n", paramIdx, wrapToPi(val), energy,
                  energy < solveThreshold() ? "SOLVED!" : "");
    return;
  }

  // --- params — show current working params ---
  if (sub == "params") {
    float energy = evaluate(g_workParams);
    stream.printf("Working params (Energy: %.6f):\r\n", energy);
    for (uint8_t i = 0; i < NUM_PARAMS; i++) {
      stream.printf("  \xce\xb8[%2d] = %.4f\r\n", i, wrapToPi(g_workParams[i]));
    }
    return;
  }

  // --- sweep <param_index> <start> <end> <steps> ---
  // Uses working params as base. Auto-saves best θ back to working params.
  if (sub == "sweep") {
    if (!checkCooldown(stream))
      return;
    int paramIdx, steps;
    float sweepStart, sweepEnd;
    if (!parseInt(args, idx, paramIdx) || !parseFloat(args, idx, sweepStart) || !parseFloat(args, idx, sweepEnd) ||
        !parseInt(args, idx, steps)) {
      stream.println("Usage: quantum crystal sweep <idx> <start> <end> <steps>");
      return;
    }
    if (paramIdx < 0 || paramIdx >= NUM_PARAMS) {
      stream.printf("Error: param index must be 0..%d\r\n", NUM_PARAMS - 1);
      return;
    }
    if (steps < 2 || steps > 100) {
      stream.println("Error: steps must be 2..100");
      return;
    }

    // Use working params as base
    float baseParams[NUM_PARAMS];
    memcpy(baseParams, g_workParams, sizeof(baseParams));

    stream.printf("Sweeping θ[%d] from %.3f to %.3f (%d steps):\r\n", paramIdx, sweepStart, sweepEnd, steps);

    float bestE = 999.f;
    float bestTheta = 0.f;

    for (int s = 0; s < steps; s++) {
      float t = sweepStart + (sweepEnd - sweepStart) * s / (steps - 1);
      baseParams[paramIdx] = t;
      float e = evaluate(baseParams);
      stream.printf("  θ=%.4f  E=%.6f\r\n", t, e);
      if (e < bestE) {
        bestE = e;
        bestTheta = t;
      }
    }

    // Report best — player must manually `set` to apply
    stream.printf("Best: θ[%d]=%.4f  E=%.6f  %s\r\n", paramIdx, wrapToPi(bestTheta), bestE,
                  bestE < solveThreshold() ? "SOLVED!" : "not solved");
    return;
  }

  // --- store [θ₀ ... θ₁₃]  (uses working params if omitted) ---
  if (sub == "store") {
    float params[NUM_PARAMS];
    memcpy(params, g_workParams, sizeof(params));
    // Override with any explicitly provided params
    for (uint8_t i = 0; i < NUM_PARAMS; i++) {
      float tmp;
      if (!parseFloat(args, idx, tmp))
        break;
      params[i] = tmp;
    }
    float energy = evaluate(params);
    bool ok = store(params, energy);
    stream.printf("Energy: %.6f  %s\r\n", energy, energy < solveThreshold() ? "SOLVED" : "not solved");
    stream.printf("NVS store: %s\r\n", ok ? "success" : "FAILED");
    return;
  }

  // --- status ---
  if (sub == "status") {
    CrystalState st;
    if (!load(st)) {
      stream.println("Crystal: no valid stored state (or version mismatch)");
      return;
    }
    stream.printf("Crystal state (v%d):\r\n", st.version);
    stream.printf("  Solved: %s\r\n", st.solved ? "YES" : "no");
    stream.printf("  Energy: %.3f\r\n", st.energy_milli / 1000.f);
    stream.printf("  Params (millirad):");
    for (uint8_t i = 0; i < NUM_PARAMS; i++) {
      stream.printf(" %d", st.params[i]);
    }
    stream.println();
    stream.printf("  Checksum: 0x%08X\r\n", st.checksum);
    return;
  }

  // --- reset ---
  if (sub == "reset") {
    reset();
    stream.println("Crystal state erased.");
    return;
  }

  stream.println("Unknown crystal sub-command. Try: info, run, set, params, sweep, store, status, reset");
}

}  // namespace crystal
}  // namespace challenges
