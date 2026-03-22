/**
 * grid.cpp — "Grid Optimization" QAOA challenge implementation.
 *
 * Cost Hamiltonian:  Hc = Σ hᵢZᵢ + Σ Jᵢⱼ ZᵢZⱼ
 * QAOA circuit at depth p=2:
 *   |+⟩^n → [e^{-iγ₁Hc} · e^{-iβ₁Hm}] → [e^{-iγ₂Hc} · e^{-iβ₂Hm}]
 * where Hm = ΣXᵢ (mixer, implemented as RX(2β) per qubit).
 *
 * Evaluation is sampling-based with a deterministic PRNG seed.
 * Metrics: best energy, low-energy hit count, CVaR (bottom 20% tail average).
 *
 * Coefficients from fixed-seed PRNG — identical on badge and dock.
 */

#include "grid.h"
#include "qsim.h"
#include <../core/storage/nvs_quantum.h>
#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace challenges {
namespace grid {

// ---------------------------------------------------------------------------
// Cost Hamiltonian coefficients
// ---------------------------------------------------------------------------

static float g_h[NUM_QUBITS];              // single-qubit Z coeffs
static float g_J[NUM_QUBITS][NUM_QUBITS];  // ZZ coupling (upper triangle only)
static uint8_t g_numEdges = 0;

struct Edge {
  uint8_t i, j;
  float w;
};

static Edge g_edges[30];                   // max edges for 10-qubit sparse graph
static float g_diagCost[1 << NUM_QUBITS];  // precomputed diagonal cost for each basis state

static bool g_hamiltonian_ready = false;

/// Same PRNG as crystal — deterministic coefficient generation
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

  uint32_t seed = 0x47524944;  // "GRID" in hex — distinct from crystal seed

  // Single-qubit fields
  for (uint8_t i = 0; i < NUM_QUBITS; i++)
    g_h[i] = rng_float(seed, -1.0f, 1.0f);

  // ZZ couplings — random sparse graph (ring + random extra edges)
  memset(g_J, 0, sizeof(g_J));
  g_numEdges = 0;

  // Ring connectivity
  for (uint8_t i = 0; i < NUM_QUBITS; i++) {
    uint8_t j = (i + 1) % NUM_QUBITS;
    float w = rng_float(seed, -1.0f, 1.0f);
    g_J[i][j] = w;
    g_edges[g_numEdges++] = {i, j, w};
  }

  // Add 5 random extra edges for complexity
  for (int k = 0; k < 5; k++) {
    uint8_t i = coeff_rng(seed) % NUM_QUBITS;
    uint8_t j = coeff_rng(seed) % NUM_QUBITS;
    if (i == j)
      j = (j + 1) % NUM_QUBITS;
    if (i > j) {
      uint8_t t = i;
      i = j;
      j = t;
    }
    if (g_J[i][j] == 0.f) {
      float w = rng_float(seed, -0.5f, 0.5f);
      g_J[i][j] = w;
      g_edges[g_numEdges++] = {i, j, w};
    }
  }

  // Precompute diagonal cost for each basis state
  // cost(z) = Σ hᵢ·sᵢ + Σ Jᵢⱼ·sᵢ·sⱼ  where sᵢ = (-1)^{bit i of z}
  uint32_t dim = 1u << NUM_QUBITS;
  for (uint32_t z = 0; z < dim; z++) {
    float c = 0.f;
    for (uint8_t i = 0; i < NUM_QUBITS; i++) {
      float si = (z & (1u << i)) ? -1.f : 1.f;
      c += g_h[i] * si;
    }
    for (uint8_t e = 0; e < g_numEdges; e++) {
      float si = (z & (1u << g_edges[e].i)) ? -1.f : 1.f;
      float sj = (z & (1u << g_edges[e].j)) ? -1.f : 1.f;
      c += g_edges[e].w * si * sj;
    }
    g_diagCost[z] = c;
  }

  g_hamiltonian_ready = true;
}

// ---------------------------------------------------------------------------
// QAOA circuit
// ---------------------------------------------------------------------------

static void applyQAOA(qsim::StateVec &sv, float gamma1, float gamma2, float beta1, float beta2) {
  // Layer 1: cost + mixer
  qsim::apply_cost_unitary(sv, g_diagCost, gamma1);
  for (uint8_t q = 0; q < NUM_QUBITS; q++)
    qsim::gate_rx(sv, q, 2.f * beta1);

  // Layer 2: cost + mixer
  qsim::apply_cost_unitary(sv, g_diagCost, gamma2);
  for (uint8_t q = 0; q < NUM_QUBITS; q++)
    qsim::gate_rx(sv, q, 2.f * beta2);
}

// ---------------------------------------------------------------------------
// Evaluation — sampling-based
// ---------------------------------------------------------------------------

void evaluate(float gamma1, float gamma2, float beta1, float beta2, GridMetrics &out) {
  buildHamiltonian();

  // Prepare state |+⟩^n
  qsim::StateVec sv;
  qsim::init_plus(sv, NUM_QUBITS);

  // Apply QAOA
  applyQAOA(sv, gamma1, gamma2, beta1, beta2);

  // Sample NUM_SAMPLES bitstrings deterministically
  float energies[NUM_SAMPLES];
  uint32_t seed = SAMPLE_SEED;

  for (uint16_t s = 0; s < NUM_SAMPLES; s++) {
    uint32_t z = qsim::sample_once(sv, seed);
    energies[s] = g_diagCost[z];
  }

  // Sort for statistics
  std::sort(energies, energies + NUM_SAMPLES);

  // Best energy
  out.best_energy = energies[0];

  // Mean
  float sum = 0.f;
  for (uint16_t s = 0; s < NUM_SAMPLES; s++)
    sum += energies[s];
  out.mean_energy = sum / NUM_SAMPLES;

  // CVaR — bottom 20% tail average
  uint16_t tailCount = NUM_SAMPLES / 5;  // 51 samples
  if (tailCount < 1)
    tailCount = 1;
  float tailSum = 0.f;
  for (uint16_t s = 0; s < tailCount; s++)
    tailSum += energies[s];
  out.cvar = tailSum / tailCount;

  // Low-energy hit count (below low_threshold)
  out.low_threshold = out.best_energy + fabsf(out.best_energy) * 0.1f + 0.5f;
  out.low_hits = 0;
  for (uint16_t s = 0; s < NUM_SAMPLES; s++) {
    if (energies[s] < out.low_threshold)
      out.low_hits++;
  }
}

// ---------------------------------------------------------------------------
// Solve criteria
// ---------------------------------------------------------------------------

/// Compute the optimal (minimum) energy across all basis states.
static float optimalEnergy() {
  buildHamiltonian();
  float best = g_diagCost[0];
  uint32_t dim = 1u << NUM_QUBITS;
  for (uint32_t z = 1; z < dim; z++) {
    if (g_diagCost[z] < best)
      best = g_diagCost[z];
  }
  return best;
}

float solveThreshold() {
  // Must find best energy within 5% of optimal (tight)
  float opt = optimalEnergy();
  return opt + fabsf(opt) * 0.05f;
}

float solveCvarThreshold() {
  // CVaR (bottom 20%) must be within 20% of optimal
  float opt = optimalEnergy();
  return opt + fabsf(opt) * 0.20f;
}

uint16_t solveMinHits() {
  // At least 20 low-energy samples (out of 256)
  return 20;
}

bool isSolved(const GridMetrics &m) {
  return m.best_energy < solveThreshold() && m.low_hits >= solveMinHits() && m.cvar < solveCvarThreshold();
}

// ---------------------------------------------------------------------------
// CRC-32 (identical to crystal.cpp)
// ---------------------------------------------------------------------------

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
  }
  return ~crc;
}

// ---------------------------------------------------------------------------
// NVS persistence
// ---------------------------------------------------------------------------

bool store(float gamma1, float gamma2, float beta1, float beta2, const GridMetrics &m) {
  GridState st;
  memset(&st, 0, sizeof(st));
  st.version = NVS_VERSION;
  st.solved = isSolved(m) ? 1 : 0;
  st.gamma1 = qsim::to_millirad(gamma1);
  st.gamma2 = qsim::to_millirad(gamma2);
  st.beta1 = qsim::to_millirad(beta1);
  st.beta2 = qsim::to_millirad(beta2);
  st.best_energy_milli = static_cast<int32_t>(roundf(m.best_energy * 1000.f));
  st.low_energy_hits = m.low_hits;
  st.cvar_milli = static_cast<int32_t>(roundf(m.cvar * 1000.f));

  size_t csLen = offsetof(GridState, checksum);
  st.checksum = crc32(reinterpret_cast<const uint8_t *>(&st), csLen);

  return core::storage::quantumWriteBlob("grid", &st, sizeof(st));
}

bool load(GridState &out) {
  size_t rd = core::storage::quantumReadBlob("grid", &out, sizeof(out));
  if (rd != sizeof(out))
    return false;
  if (out.version != NVS_VERSION)
    return false;
  size_t csLen = offsetof(GridState, checksum);
  uint32_t expected = crc32(reinterpret_cast<const uint8_t *>(&out), csLen);
  return (out.checksum == expected);
}

void reset() {
  core::storage::quantumErase("grid");
}

// ---------------------------------------------------------------------------
// CLI:  quantum grid <info|run|hist|store|status|reset>
// ---------------------------------------------------------------------------

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

/// Parse all 4 QAOA params.  Returns false if insufficient.
static bool parse4Params(const std::string &args, size_t &idx, float &g1, float &g2, float &b1, float &b2) {
  return parseFloat(args, idx, g1) && parseFloat(args, idx, g2) && parseFloat(args, idx, b1) &&
         parseFloat(args, idx, b2);
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
    stream.printf("=== Grid Optimization (QAOA p=2) ===\r\n");
    stream.printf("Qubits: %d, Depth: %d, Params: 4 (γ₁ γ₂ β₁ β₂)\r\n", NUM_QUBITS, QAOA_DEPTH);
    stream.printf("Samples: %d (seed=0x%08X)\r\n", NUM_SAMPLES, SAMPLE_SEED);
    stream.flush();
    stream.printf("Single-qubit fields h:\r\n");
    for (uint8_t i = 0; i < NUM_QUBITS; i++)
      stream.printf("  h[%d] = %.4f\r\n", i, g_h[i]);
    stream.flush();
    stream.printf("ZZ couplings J (%d edges):\r\n", g_numEdges);
    for (uint8_t e = 0; e < g_numEdges; e++)
      stream.printf("  J[%d,%d] = %.4f\r\n", g_edges[e].i, g_edges[e].j, g_edges[e].w);
    stream.flush();
    stream.printf("Optimal energy: %.4f\r\n", optimalEnergy());
    stream.printf("Solve criteria:\r\n");
    stream.printf("  best energy  < %.4f\r\n", solveThreshold());
    stream.printf("  CVaR (20%%)   < %.4f\r\n", solveCvarThreshold());
    stream.printf("  low hits     >= %d\r\n", solveMinHits());
    stream.flush();
    return;
  }

  // --- run <γ₁> <γ₂> <β₁> <β₂> ---
  if (sub == "run") {
    float g1, g2, b1, b2;
    if (!parse4Params(args, idx, g1, g2, b1, b2)) {
      stream.println("Usage: quantum grid run <γ₁> <γ₂> <β₁> <β₂>");
      return;
    }
    GridMetrics m;
    evaluate(g1, g2, b1, b2, m);
    stream.printf("Best energy:  %.4f\r\n", m.best_energy);
    stream.printf("Mean energy:  %.4f\r\n", m.mean_energy);
    stream.printf("CVaR (20%%):   %.4f\r\n", m.cvar);
    stream.printf("Low hits:     %d / %d  (threshold: %.4f)\r\n", m.low_hits, NUM_SAMPLES, m.low_threshold);
    stream.printf("Solve: %s\r\n", isSolved(m) ? "SOLVED!" : "not solved");
    stream.flush();
    return;
  }

  // --- hist <γ₁> <γ₂> <β₁> <β₂> --- print energy histogram
  if (sub == "hist") {
    float g1, g2, b1, b2;
    if (!parse4Params(args, idx, g1, g2, b1, b2)) {
      stream.println("Usage: quantum grid hist <γ₁> <γ₂> <β₁> <β₂>");
      return;
    }

    // Run QAOA and collect per-sample energies
    qsim::StateVec sv;
    qsim::init_plus(sv, NUM_QUBITS);
    applyQAOA(sv, g1, g2, b1, b2);

    // Bin energies into 10 buckets
    float eMin = g_diagCost[0], eMax = g_diagCost[0];
    uint32_t dim = 1u << NUM_QUBITS;
    for (uint32_t z = 1; z < dim; z++) {
      if (g_diagCost[z] < eMin)
        eMin = g_diagCost[z];
      if (g_diagCost[z] > eMax)
        eMax = g_diagCost[z];
    }

    constexpr int BINS = 10;
    uint16_t bins[BINS] = {};
    float binWidth = (eMax - eMin) / BINS;
    if (binWidth < 0.001f)
      binWidth = 0.001f;

    uint32_t seed = SAMPLE_SEED;
    for (uint16_t s = 0; s < NUM_SAMPLES; s++) {
      uint32_t z = qsim::sample_once(sv, seed);
      float e = g_diagCost[z];
      int b = static_cast<int>((e - eMin) / binWidth);
      if (b >= BINS)
        b = BINS - 1;
      if (b < 0)
        b = 0;
      bins[b]++;
    }

    stream.printf("Energy histogram (%d samples):\r\n", NUM_SAMPLES);
    for (int b = 0; b < BINS; b++) {
      float lo = eMin + b * binWidth;
      float hi = lo + binWidth;
      stream.printf("  [%7.3f, %7.3f): %d", lo, hi, bins[b]);
      // ASCII bar
      int barLen = bins[b] * 40 / NUM_SAMPLES;
      stream.print("  ");
      for (int k = 0; k < barLen; k++)
        stream.print("#");
      stream.println();
    }
    stream.flush();
    return;
  }

  // --- store <γ₁> <γ₂> <β₁> <β₂> ---
  if (sub == "store") {
    float g1, g2, b1, b2;
    if (!parse4Params(args, idx, g1, g2, b1, b2)) {
      stream.println("Usage: quantum grid store <γ₁> <γ₂> <β₁> <β₂>");
      return;
    }
    GridMetrics m;
    evaluate(g1, g2, b1, b2, m);
    bool ok = store(g1, g2, b1, b2, m);
    stream.printf("Best: %.4f  CVaR: %.4f  Hits: %d  %s\r\n", m.best_energy, m.cvar, m.low_hits,
                  isSolved(m) ? "SOLVED" : "not solved");
    stream.printf("NVS store: %s\r\n", ok ? "success" : "FAILED");
    stream.flush();
    return;
  }

  // --- status ---
  if (sub == "status") {
    GridState st;
    if (!load(st)) {
      stream.println("Grid: no valid stored state (or version mismatch)");
      return;
    }
    stream.printf("Grid state (v%d):\r\n", st.version);
    stream.printf("  Solved: %s\r\n", st.solved ? "YES" : "no");
    stream.printf("  γ₁=%d  γ₂=%d  β₁=%d  β₂=%d  (millirad)\r\n", st.gamma1, st.gamma2, st.beta1, st.beta2);
    stream.printf("  Best energy: %.3f\r\n", st.best_energy_milli / 1000.f);
    stream.printf("  Low hits: %d\r\n", st.low_energy_hits);
    stream.printf("  CVaR: %.3f\r\n", st.cvar_milli / 1000.f);
    stream.printf("  Checksum: 0x%08X\r\n", st.checksum);
    stream.flush();
    return;
  }

  // --- reset ---
  if (sub == "reset") {
    reset();
    stream.println("Grid state erased.");
    return;
  }

  stream.println("Unknown grid sub-command. Try: info, run, hist, store, status, reset");
}

}  // namespace grid
}  // namespace challenges
