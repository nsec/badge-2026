/**
 * qsim.cpp — Quantum statevector simulator for ESP32-S3.
 *
 * All gate kernels operate in-place using the standard bit-index pattern
 * adapted from the reference implementation in quantum/quantum.cpp.
 * No matrix construction, no temporary allocations in hot paths.
 */

#include "qsim.h"
#include <cstring>

namespace qsim {

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void init_zero(StateVec &sv, uint8_t nq) {
  size_t dim = 1u << nq;
  sv.assign(dim, Cf(0.f, 0.f));
  sv[0] = Cf(1.f, 0.f);
}

void init_plus(StateVec &sv, uint8_t nq) {
  size_t dim = 1u << nq;
  float amp = 1.f / sqrtf(static_cast<float>(dim));
  sv.assign(dim, Cf(amp, 0.f));
}

// ---------------------------------------------------------------------------
// Single-qubit gate kernel (generic 2×2 in-place)
//
// For qubit q, we iterate over pairs of indices (i0, i1) that differ
// only in bit q.  i0 has bit q = 0, i1 has bit q = 1.
//
//   new_i0 = a * old_i0 + b * old_i1
//   new_i1 = c * old_i0 + d * old_i1
//
// This is O(2^nq) with zero allocation — same index pattern as the
// reference quantum.cpp apply_single_qubit_gate.
// ---------------------------------------------------------------------------

static void apply_2x2(StateVec &sv, uint8_t q, Cf a, Cf b, Cf c, Cf d) {
  size_t dim = sv.size();
  size_t mask = 1u << q;
  for (size_t i = 0; i < dim; i++) {
    if (i & mask)
      continue;  // process each pair once (when bit q = 0)
    size_t i0 = i;
    size_t i1 = i | mask;
    Cf v0 = sv[i0];
    Cf v1 = sv[i1];
    sv[i0] = a * v0 + b * v1;
    sv[i1] = c * v0 + d * v1;
  }
}

// ---------------------------------------------------------------------------
// Gate implementations
// ---------------------------------------------------------------------------

void gate_rx(StateVec &sv, uint8_t q, float theta) {
  float c = cosf(theta * 0.5f);
  float s = sinf(theta * 0.5f);
  // RX(θ) = [[cos(θ/2), -i·sin(θ/2)],
  //           [-i·sin(θ/2), cos(θ/2)]]
  apply_2x2(sv, q, Cf(c, 0.f), Cf(0.f, -s), Cf(0.f, -s), Cf(c, 0.f));
}

void gate_ry(StateVec &sv, uint8_t q, float theta) {
  float c = cosf(theta * 0.5f);
  float s = sinf(theta * 0.5f);
  // RY(θ) = [[cos(θ/2), -sin(θ/2)],
  //           [sin(θ/2),  cos(θ/2)]]
  apply_2x2(sv, q, Cf(c, 0.f), Cf(-s, 0.f), Cf(s, 0.f), Cf(c, 0.f));
}

void gate_rz(StateVec &sv, uint8_t q, float theta) {
  // RZ(θ) = diag(e^{-iθ/2}, e^{iθ/2})  — no pair mixing needed
  float c = cosf(theta * 0.5f);
  float s = sinf(theta * 0.5f);
  Cf phase0(c, -s);  // e^{-iθ/2}
  Cf phase1(c, s);   // e^{+iθ/2}
  size_t dim = sv.size();
  size_t mask = 1u << q;
  for (size_t i = 0; i < dim; i++) {
    if (i & mask)
      sv[i] = sv[i] * phase1;
    else
      sv[i] = sv[i] * phase0;
  }
}

void gate_h(StateVec &sv, uint8_t q) {
  float inv = 1.f / sqrtf(2.f);
  // H = (1/√2) [[1, 1], [1, -1]]
  apply_2x2(sv, q, Cf(inv, 0.f), Cf(inv, 0.f), Cf(inv, 0.f), Cf(-inv, 0.f));
}

void gate_cz(StateVec &sv, uint8_t q0, uint8_t q1) {
  // CZ: flip sign of amplitudes where both q0 AND q1 are |1⟩
  size_t mask0 = 1u << q0;
  size_t mask1 = 1u << q1;
  size_t dim = sv.size();
  for (size_t i = 0; i < dim; i++) {
    if ((i & mask0) && (i & mask1)) {
      sv[i].re = -sv[i].re;
      sv[i].im = -sv[i].im;
    }
  }
}

// ---------------------------------------------------------------------------
// Expectation values
// ---------------------------------------------------------------------------

float expect_z(const StateVec &sv, uint8_t q) {
  // ⟨Z_q⟩ = Σ_i |a_i|^2 * (-1)^{bit q of i}
  float val = 0.f;
  size_t mask = 1u << q;
  for (size_t i = 0; i < sv.size(); i++) {
    float p = sv[i].norm2();
    val += (i & mask) ? -p : p;
  }
  return val;
}

float expect_zz(const StateVec &sv, uint8_t q0, uint8_t q1) {
  // ⟨Z_q0 Z_q1⟩ = Σ_i |a_i|^2 * (-1)^{bit q0 ⊕ bit q1}
  float val = 0.f;
  size_t m0 = 1u << q0;
  size_t m1 = 1u << q1;
  for (size_t i = 0; i < sv.size(); i++) {
    float p = sv[i].norm2();
    int parity = ((i & m0) ? 1 : 0) ^ ((i & m1) ? 1 : 0);
    val += parity ? -p : p;
  }
  return val;
}

float expect_x(const StateVec &sv, uint8_t q) {
  // ⟨X_q⟩ = Σ_{pairs i0,i1} 2·Re(conj(a_i0) · a_i1)
  // where i0 has bit q = 0 and i1 = i0 | (1 << q)
  float val = 0.f;
  size_t mask = 1u << q;
  for (size_t i = 0; i < sv.size(); i++) {
    if (i & mask)
      continue;
    size_t i0 = i;
    size_t i1 = i | mask;
    // Re(conj(a0) * a1) = a0.re*a1.re + a0.im*a1.im
    val += 2.f * (sv[i0].re * sv[i1].re + sv[i0].im * sv[i1].im);
  }
  return val;
}

// ---------------------------------------------------------------------------
// Diagonal cost unitary (for QAOA)
// ---------------------------------------------------------------------------

void apply_cost_unitary(StateVec &sv, const float *diag_costs, float gamma) {
  for (size_t i = 0; i < sv.size(); i++) {
    float angle = -gamma * diag_costs[i];
    float c = cosf(angle);
    float s = sinf(angle);
    Cf phase(c, s);
    sv[i] = sv[i] * phase;
  }
}

// ---------------------------------------------------------------------------
// Deterministic sampling (xorshift32)
// ---------------------------------------------------------------------------

static uint32_t xorshift32(uint32_t &s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

uint32_t sample_once(const StateVec &sv, uint32_t &seed) {
  // Generate uniform random in [0, 1)
  uint32_t r = xorshift32(seed);
  float u = static_cast<float>(r) / 4294967296.f;

  // Cumulative probability search
  float cumul = 0.f;
  for (size_t i = 0; i < sv.size(); i++) {
    cumul += sv[i].norm2();
    if (u < cumul)
      return static_cast<uint32_t>(i);
  }
  // Fallback (rounding)
  return static_cast<uint32_t>(sv.size() - 1);
}

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

static constexpr float PI_F = 3.14159265358979323846f;

float wrap_angle(float theta) {
  while (theta > PI_F)
    theta -= 2.f * PI_F;
  while (theta < -PI_F)
    theta += 2.f * PI_F;
  return theta;
}

int16_t to_millirad(float theta) {
  theta = wrap_angle(theta);
  int32_t mr = static_cast<int32_t>(roundf(theta * 1000.f));
  if (mr > 31415)
    mr = 31415;
  if (mr < -31415)
    mr = -31415;
  return static_cast<int16_t>(mr);
}

float from_millirad(int16_t mr) {
  return static_cast<float>(mr) * 0.001f;
}

bool is_valid(float v) {
  return !std::isnan(v) && !std::isinf(v);
}

}  // namespace qsim
