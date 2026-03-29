#pragma once
/**
 * qsim.h — Lightweight quantum statevector simulator for ESP32-S3.
 *
 * Provides in-place gate kernels (RX, RZ, CZ, H), expectation values
 * (Z, ZZ, X), deterministic sampling, and angle canonicalization.
 *
 * Uses float complex to keep 12-qubit state at 32 KB (vs 64 KB for double).
 * Precision is ~7 digits — sufficient for millidegree angles and
 * energy comparisons to 3–4 significant figures.
 */

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>

namespace qsim {

/// Complex amplitude type.  float saves 50% memory vs double.
struct Cf {
  float re, im;

  Cf() : re(0.f), im(0.f) {}

  Cf(float r, float i) : re(r), im(i) {}

  Cf operator+(Cf o) const {
    return {re + o.re, im + o.im};
  }

  Cf operator-(Cf o) const {
    return {re - o.re, im - o.im};
  }

  Cf operator*(Cf o) const {
    return {re * o.re - im * o.im, re * o.im + im * o.re};
  }

  Cf operator*(float s) const {
    return {re * s, im * s};
  }

  float norm2() const {
    return re * re + im * im;
  }  // |z|^2
};

/// Statevector: caller-owned buffer of 2^nq amplitudes.
using StateVec = std::vector<Cf>;

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

/// Initialize |0...0⟩  (computational basis state 0).
void init_zero(StateVec &sv, uint8_t nq);

/// Initialize |+...+⟩  (equal superposition).
void init_plus(StateVec &sv, uint8_t nq);

// ---------------------------------------------------------------------------
// In-place gate kernels  (O(2^nq) per call, zero allocation)
// ---------------------------------------------------------------------------

/// RX(θ) on qubit q.   RX(θ) = cos(θ/2)I - i·sin(θ/2)X
void gate_rx(StateVec &sv, uint8_t q, float theta);

/// RY(θ) on qubit q.   RY(θ) = [[cos(θ/2), -sin(θ/2)], [sin(θ/2), cos(θ/2)]]
void gate_ry(StateVec &sv, uint8_t q, float theta);

/// RZ(θ) on qubit q.   RZ(θ) = diag(e^{-iθ/2}, e^{iθ/2})  — diagonal, real fast
void gate_rz(StateVec &sv, uint8_t q, float theta);

/// Hadamard on qubit q.
void gate_h(StateVec &sv, uint8_t q);

/// CZ on qubits q0, q1.  Diagonal: phase-flip |11⟩ component.
void gate_cz(StateVec &sv, uint8_t q0, uint8_t q1);

// ---------------------------------------------------------------------------
// Expectation values  (O(2^nq), no allocation)
// ---------------------------------------------------------------------------

/// ⟨ψ|Z_q|ψ⟩
float expect_z(const StateVec &sv, uint8_t q);

/// ⟨ψ|Z_q0 Z_q1|ψ⟩
float expect_zz(const StateVec &sv, uint8_t q0, uint8_t q1);

/// ⟨ψ|X_q|ψ⟩
float expect_x(const StateVec &sv, uint8_t q);

// ---------------------------------------------------------------------------
// Diagonal cost unitary  (for QAOA)
// ---------------------------------------------------------------------------

/// Apply e^{-i γ Hc} where Hc is diagonal in the Z basis.
/// `diag_costs[z]` holds the cost value for basis state z.
/// This multiplies each amplitude by e^{-i γ cost(z)}.
void apply_cost_unitary(StateVec &sv, const float *diag_costs, float gamma);

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------

/// Deterministic Born-rule sample.  Returns a basis-state index.
/// Uses a simple xorshift32 PRNG seeded by `seed`.  Advances `seed` in place.
uint32_t sample_once(const StateVec &sv, uint32_t &seed);

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

/// Wrap angle to [-π, π].
float wrap_angle(float theta);

/// Convert float radians → int16_t milliradians (clamped to ±31415).
int16_t to_millirad(float theta);

/// Convert int16_t milliradians → float radians.
float from_millirad(int16_t mr);

/// Check for NaN or Inf.
bool is_valid(float v);

}  // namespace qsim
