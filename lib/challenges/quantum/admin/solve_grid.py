#!/usr/bin/env python3
"""
QAOA Grid Challenge — Admin Solver

Given a badge HWID (MAC hex), computes the per-device parameter transform
and finds optimal parameters for that specific badge.

Usage:
  python3 solve_grid.py <HWID>
  python3 solve_grid.py 80B54EE0783C

Output:
  - Device-specific transform parameters
  - Optimal input parameters (what to type on the badge)
  - Badge CLI commands to paste

Requirements:
  pip install scipy numpy
"""

import sys
import math
import numpy as np
from scipy.optimize import differential_evolution

# ============================================================
# PRNG (matches badge xorshift32)
# ============================================================
def xor32(seed):
    seed = (seed ^ (seed << 13)) & 0xFFFFFFFF
    seed = (seed ^ (seed >> 17)) & 0xFFFFFFFF
    seed = (seed ^ (seed << 5)) & 0xFFFFFFFF
    return seed

def rng_float(seed, lo, hi):
    seed = xor32(seed)
    return seed, lo + (seed / 4294967296.0) * (hi - lo)

# ============================================================
# Build Hamiltonian (matches grid.cpp, seed 0x47524944)
# ============================================================
NQ = 10
DIM = 1 << NQ

def build_hamiltonian():
    seed = 0x47524944
    h_f = []
    for _ in range(NQ):
        seed, v = rng_float(seed, -1.0, 1.0)
        h_f.append(v)
    edges = []
    J = {}
    for i in range(NQ):
        j = (i + 1) % NQ
        seed, w = rng_float(seed, -1.0, 1.0)
        J[(i, j)] = w
        edges.append((i, j, w))
    for _ in range(5):
        seed = xor32(seed); ie = seed % NQ
        seed = xor32(seed); je = seed % NQ
        if ie == je: je = (je + 1) % NQ
        if ie > je: ie, je = je, ie
        if (ie, je) not in J:
            seed, w = rng_float(seed, -0.5, 0.5)
            J[(ie, je)] = w
            edges.append((ie, je, w))
    diag = np.zeros(DIM)
    for z in range(DIM):
        c = 0.0
        for qi in range(NQ):
            si = -1.0 if (z & (1 << qi)) else 1.0
            c += h_f[qi] * si
        for (ei, ej, ew) in edges:
            si = -1.0 if (z & (1 << ei)) else 1.0
            sj = -1.0 if (z & (1 << ej)) else 1.0
            c += ew * si * sj
        diag[z] = c
    return h_f, edges, diag

# ============================================================
# Per-device parameter transform (matches grid.cpp)
# ============================================================
def compute_transform(hwid_hex):
    """Compute per-device affine transform from HWID."""
    mac_bytes = bytes.fromhex(hwid_hex)
    seed = 0x51414F41  # "QAOA"
    for b in mac_bytes:
        seed = (seed * 31 + b) & 0xFFFFFFFF
    scales = []
    offsets = []
    for _ in range(4):
        seed = xor32(seed)
        scales.append(0.8 + (seed & 0xFFFF) / 65535.0 * 0.4)
        seed = xor32(seed)
        offsets.append(-0.3 + (seed & 0xFFFF) / 65535.0 * 0.6)
    return scales, offsets

def apply_transform(g1, g2, b1, b2, scales, offsets):
    """Clamp to [0, pi] then apply affine transform."""
    PI = math.pi
    g1 = max(0.0, min(PI, g1))
    g2 = max(0.0, min(PI, g2))
    b1 = max(0.0, min(PI, b1))
    b2 = max(0.0, min(PI, b2))
    return (
        g1 * scales[0] + offsets[0],
        g2 * scales[1] + offsets[1],
        b1 * scales[2] + offsets[2],
        b2 * scales[3] + offsets[3],
    )

# ============================================================
# QAOA Evaluation (matches badge exactly)
# ============================================================
SAMPLE_SEED = 0xDEADBEEF

# Precompute sample randoms
def build_sample_randoms():
    s = SAMPLE_SEED
    us = []
    for _ in range(256):
        s = xor32(s)
        us.append(s / 4294967296.0)
    return np.array(us)

# Precompute RX index pairs
def build_rx_pairs():
    pairs = []
    for q in range(NQ):
        step = 1 << q
        i0 = np.array([i for i in range(DIM) if not (i & step)], dtype=np.int32)
        i1 = i0 | step
        pairs.append((i0, i1))
    return pairs

def qaoa_eval(g1, g2, b1, b2, diag_np, sample_us, rx_pairs):
    """Fully vectorized QAOA evaluation."""
    sv = np.full(DIM, 1.0 / np.sqrt(DIM), dtype=np.complex128)

    def apply_cost(gamma):
        nonlocal sv
        sv *= np.exp(-1j * gamma * diag_np)

    def apply_rx(angle):
        nonlocal sv
        c, s = np.cos(angle / 2), np.sin(angle / 2)
        for i0, i1 in rx_pairs:
            a, b = sv[i0].copy(), sv[i1].copy()
            sv[i0] = c * a - 1j * s * b
            sv[i1] = -1j * s * a + c * b

    apply_cost(g1); apply_rx(2.0 * b1)
    apply_cost(g2); apply_rx(2.0 * b2)

    probs = np.abs(sv) ** 2
    cumprob = np.cumsum(probs)
    z_samp = np.searchsorted(cumprob, sample_us)
    z_samp = np.clip(z_samp, 0, DIM - 1)
    energies = np.sort(diag_np[z_samp])

    best = float(energies[0])
    tail = max(1, 256 // 5)
    cvar = float(np.mean(energies[:tail]))
    low_t = best + abs(best) * 0.1 + 0.5
    hits = int(np.sum(energies < low_t))
    return best, cvar, hits

# ============================================================
# Solver
# ============================================================
def solve(hwid_hex):
    print(f"=" * 60)
    print(f"QAOA Grid Solver — HWID: {hwid_hex}")
    print(f"=" * 60)

    # Build Hamiltonian
    h_f, edges, diag = build_hamiltonian()
    diag_np = np.array(diag)
    opt = float(np.min(diag_np))
    thresh = opt + abs(opt) * 0.05
    cvar_thresh = opt + abs(opt) * 0.20

    print(f"\nHamiltonian: {NQ} qubits, {len(edges)} edges")
    print(f"Optimal energy: {opt:.4f}")
    print(f"Solve thresholds: best < {thresh:.4f}, CVaR < {cvar_thresh:.4f}, hits >= 20")

    # Compute device transform
    scales, offsets = compute_transform(hwid_hex)
    print(f"\nDevice transform (α, offset):")
    for i, name in enumerate(["γ₁", "γ₂", "β₁", "β₂"]):
        print(f"  {name}: scale={scales[i]:.4f}, offset={offsets[i]:.4f}")

    # Precompute
    sample_us = build_sample_randoms()
    rx_pairs = build_rx_pairs()

    def is_solved(best, cvar, hits):
        return best < thresh and cvar < cvar_thresh and hits >= 20

    # Objective: minimize CVaR with penalty for bad best energy
    def objective(x):
        # x is in INPUT space — apply transform
        g1, g2, b1, b2 = apply_transform(x[0], x[1], x[2], x[3], scales, offsets)
        best, cvar, hits = qaoa_eval(g1, g2, b1, b2, diag_np, sample_us, rx_pairs)
        penalty = 0
        if best >= thresh:
            penalty += 10 * (best - thresh)
        if hits < 20:
            penalty += 0.1 * (20 - hits)
        return cvar + penalty

    # Search in INPUT space [0, π]
    PI = math.pi
    bounds = [(0.01, PI), (0.01, PI), (0.01, PI), (0.01, PI)]

    print(f"\nSearching (differential evolution)...")
    result = differential_evolution(objective, bounds, seed=42, maxiter=50, popsize=15, tol=0.001)

    g1_in, g2_in, b1_in, b2_in = result.x
    # Verify with transform
    g1_t, g2_t, b1_t, b2_t = apply_transform(g1_in, g2_in, b1_in, b2_in, scales, offsets)
    best, cvar, hits = qaoa_eval(g1_t, g2_t, b1_t, b2_t, diag_np, sample_us, rx_pairs)
    solved = is_solved(best, cvar, hits)

    print(f"\nResult ({result.nfev} evaluations):")
    print(f"  Input params:     γ₁={g1_in:.4f}  γ₂={g2_in:.4f}  β₁={b1_in:.4f}  β₂={b2_in:.4f}")
    print(f"  Transformed:      γ₁={g1_t:.4f}  γ₂={g2_t:.4f}  β₁={b1_t:.4f}  β₂={b2_t:.4f}")
    print(f"  Best energy: {best:.4f} (threshold: {thresh:.4f})")
    print(f"  CVaR:        {cvar:.4f} (threshold: {cvar_thresh:.4f})")
    print(f"  Low hits:    {hits} (min: 20)")
    print(f"  SOLVED: {solved}")

    if not solved:
        # Try multistart Nelder-Mead
        from scipy.optimize import minimize
        print(f"\nTrying multistart Nelder-Mead...")
        rng = np.random.default_rng(42)
        best_sol = result.x
        best_obj = result.fun
        for trial in range(30):
            x0 = rng.uniform(0.01, PI, size=4)
            res = minimize(objective, x0, method='Nelder-Mead',
                           options={'maxiter': 200, 'xatol': 0.005, 'fatol': 0.005})
            if res.fun < best_obj:
                best_obj = res.fun
                best_sol = res.x
        g1_in, g2_in, b1_in, b2_in = best_sol
        g1_t, g2_t, b1_t, b2_t = apply_transform(g1_in, g2_in, b1_in, b2_in, scales, offsets)
        best, cvar, hits = qaoa_eval(g1_t, g2_t, b1_t, b2_t, diag_np, sample_us, rx_pairs)
        solved = is_solved(best, cvar, hits)
        print(f"  Best found: γ₁={g1_in:.4f}  γ₂={g2_in:.4f}  β₁={b1_in:.4f}  β₂={b2_in:.4f}")
        print(f"  Energy: {best:.4f}  CVaR: {cvar:.4f}  Hits: {hits}  SOLVED: {solved}")

    print(f"\n{'=' * 60}")
    print(f"BADGE CLI COMMANDS (paste these):")
    print(f"{'=' * 60}")
    print(f"quantum grid run {g1_in:.4f} {g2_in:.4f} {b1_in:.4f} {b2_in:.4f}")
    print(f"quantum grid store {g1_in:.4f} {g2_in:.4f} {b1_in:.4f} {b2_in:.4f}")
    print()

    return solved

# ============================================================
# Main
# ============================================================
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 solve_grid.py <HWID>")
        print("Example: python3 solve_grid.py 80B54EE0783C")
        sys.exit(1)

    hwid = sys.argv[1].strip().upper()
    if len(hwid) != 12:
        print(f"Error: HWID must be 12 hex chars, got '{hwid}' ({len(hwid)} chars)")
        sys.exit(1)

    if not solve(hwid):
        print("\n*** WARNING: Solution not found! Check transform implementation. ***")
        sys.exit(1)
