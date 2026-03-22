# The Crystal and the Grid — Quantum CTF Challenge Track

## Overview

This is a two-stage quantum computing CTF challenge for the NorthSec 2026 badge.
Players use the badge's command-line interface to explore variational quantum
algorithms, tune parameters locally, then dock their badge at a quantum validation
station to earn flags.

No prior quantum computing experience is required — just curiosity, patience,
and a willingness to experiment. The challenges are designed to teach you the
core ideas as you solve them.

### CLI Quick Reference

```
> quantum help
=== Quantum Challenge Track ===

Usage:
  quantum crystal <cmd>   - Crystal Tuning (VQE) challenge
  quantum grid <cmd>      - Grid Optimization (QAOA) challenge
  quantum flag            - show captured flags

Crystal sub-commands:
  crystal info            - show Hamiltonian & ansatz
  crystal set <idx> <val> - set one working param
  crystal params          - show working params & energy
  crystal run             - evaluate (uses working params)
  crystal sweep <idx> <lo> <hi> <steps>
                          - sweep one param, auto-saves best
  crystal store           - store working params to NVS
  crystal status / reset

Grid sub-commands:
  grid info               - show cost Hamiltonian
  grid run <g1 g2 b1 b2>  - evaluate QAOA
  grid hist <g1 g2 b1 b2> - energy histogram
  grid store <g1 g2 b1 b2> - store result to NVS
  grid status / reset
```

---

## Background: What You Need to Know

### Quantum Bits (Qubits)

A classical bit is 0 or 1. A **qubit** can be in a **superposition** —
a combination of |0⟩ and |1⟩ with complex amplitudes. When measured,
it collapses to 0 or 1 with probabilities determined by those amplitudes.

With *n* qubits, the full quantum state is a vector of $2^n$ amplitudes.
For 8 qubits, that's 256 amplitudes. For 10 qubits, 1024.

### Quantum Gates

Gates transform qubit states. The ones used here:

| Gate | What it does |
|------|-------------|
| **RY(θ)** | Rotates a single qubit by angle θ around the Y axis. At θ=0, nothing changes. At θ=π/2, puts \|0⟩ into an equal superposition. |
| **CZ** | Controlled-Z: flips the phase of the \|11⟩ component of two qubits. Creates *entanglement* between qubits. |
| **H** | Hadamard: puts \|0⟩ into equal superposition $\frac{1}{\sqrt{2}}(\|0⟩ + \|1⟩)$. |
| **RX(θ)** | Rotates around the X axis (used in the Grid challenge mixer). |

### Hamiltonians and Energy

A **Hamiltonian** is an operator that describes the energy of a quantum system.
Given a quantum state |ψ⟩, the **expectation value** ⟨ψ|H|ψ⟩ tells you the
average energy if you measured that state.

The lowest possible energy is called the **ground state energy**. Finding it
is hard in general — that's what makes it an interesting optimization problem.

### Pauli Operators

Hamiltonians are built from **Pauli operators**:

- **Z** (Pauli-Z): measures spin along z-axis. ⟨Z⟩ = +1 for |0⟩, -1 for |1⟩
- **X** (Pauli-X): measures spin along x-axis. ⟨X⟩ = +1 for |+⟩, -1 for |-⟩
- **ZZ** (tensor product): correlates two qubits. ⟨ZZ⟩ = +1 if same, -1 if different

---

## Challenge 1: Crystal Tuning (VQE)

### The Transverse-Field Ising Model (TFIM)

The Crystal challenge uses the **Transverse-Field Ising Model**, one of the
most studied Hamiltonians in quantum physics. It models a chain of interacting
magnetic spins in an external field:

$$H = -J \sum_{i} Z_i Z_{i+1} \;-\; h \sum_{i} X_i \;+\; \sum_{i} \delta_i Z_i$$

The three terms represent competing physical effects:

| Term | Coefficient | Physical meaning |
|------|------------|------------------|
| $-J \sum Z_i Z_{i+1}$ | J = 1.0 | **Coupling**: neighbouring spins want to align (both \|0⟩ or both \|1⟩). Lowers energy when adjacent qubits agree. |
| $-h \sum X_i$ | h = 1.5 | **Transverse field**: external field that "pushes" spins into superposition. Lowers energy when qubits have non-zero ⟨X⟩. |
| $\sum \delta_i Z_i$ | δᵢ ∈ [-0.15, 0.15] | **Local disorder**: small random biases on each qubit that break symmetry. Some qubits prefer \|0⟩, others prefer \|1⟩. |

The **competition** between coupling (wants alignment) and transverse field
(wants superposition) is what makes the ground state non-trivial. With J=1.0
and h=1.5, the system is **near the quantum phase transition** (h/J ≈ 1.0
is the critical point for infinite chains). In this regime, quantum
fluctuations are strong and the ground state has significant entanglement
— a 2-layer variational circuit can approximate it, but only with
carefully tuned parameters.

The δᵢ values are generated deterministically from a fixed PRNG seed, so
every badge has the same Hamiltonian. Run `quantum crystal info` to see the
exact coefficients.

### What is VQE?

The **Variational Quantum Eigensolver** (VQE) is a hybrid quantum-classical
algorithm for finding the ground state of a Hamiltonian. The idea:

1. Build a **parameterized quantum circuit** (called an *ansatz*)
2. Evaluate the energy ⟨ψ(θ)|H|ψ(θ)⟩ for current parameters θ
3. **Adjust parameters** to minimize the energy
4. Repeat until you can't go lower

Think of it like tuning 16 knobs on a radio to find the lowest frequency.
Each knob affects the signal, and some knobs interact with each other.

### The Ansatz Circuit

The Crystal challenge uses a 2-layer hardware-efficient ansatz:

```
|0⟩ ─ RY(θ₀) ─■───── RY(θ₈)  ─■─────
|0⟩ ─ RY(θ₁) ─■─■─── RY(θ₉)  ─■─■───
|0⟩ ─ RY(θ₂) ───■─■─ RY(θ₁₀) ───■─■─
|0⟩ ─ RY(θ₃) ─────■─■ RY(θ₁₁) ─────■─■
|0⟩ ─ RY(θ₄) ───────■─■ RY(θ₁₂) ───────■─■
|0⟩ ─ RY(θ₅) ─────────■─■ RY(θ₁₃) ─────────■─■
|0⟩ ─ RY(θ₆) ───────────■─■ RY(θ₁₄) ───────────■─■
|0⟩ ─ RY(θ₇) ─────────────■ RY(θ₁₅) ─────────────■
           Layer 1                 Layer 2
```

- **RY(θ)** rotations create superpositions from the initial |00000000⟩ state
- **CZ chain** gates (shown as ■─■) entangle neighbouring qubits
- The circuit runs **twice** (2 layers) with independent parameters

**Why RY and not RX?** This is a key design choice. RY gates produce
*real-valued* superpositions: RY(θ)|0⟩ = cos(θ/2)|0⟩ + sin(θ/2)|1⟩.
This means sweeping θ directly affects ⟨X⟩ (the transverse field term).
RX gates would keep ⟨X⟩ = 0 for all θ when starting from |0⟩, making the
transverse field invisible to the optimizer.

### Technical Details

- **8 qubits**, 2-layer ansatz
- **16 parameters** (θ₀ through θ₁₅) — rotation angles in radians
- Circuit: `|0⟩^8 → [RY(θ) per qubit + CZ chain] × 2 layers`
- Hamiltonian: `H = -J Σ ZᵢZᵢ₊₁ - h Σ Xᵢ + Σ δᵢZᵢ`  (J=1.0, h=1.5)
- Evaluation is **deterministic** — same params always give same energy
- You need to find energy below the solve threshold shown by `crystal info`

### Strategy: Coordinate Descent

The intended approach is **coordinate descent** — optimize one parameter at a
time while holding the others fixed. This is exactly what `sweep` does:

1. **Sweep each parameter** across a wide range to find its rough optimum
2. The `sweep` command **auto-saves** the best value to working params
3. **Re-sweep** all parameters — the optimum of each shifts as others change
4. After 3–4 rounds, the coarse sweeps saturate. Switch to **narrower sweeps**
5. Fine-tune with ranges like [-0.3, 0.3] around the current value
6. Continue until energy drops below the threshold

#### Step-by-step Walkthrough

```sh
# 1. See the Hamiltonian and threshold
quantum crystal info

# 2. Check baseline energy (all zeros)
quantum crystal run
#    → Energy ≈ -7.01 (all spins aligned, no transverse field contribution)

# 3. ROUND 1: Coarse sweep all 16 parameters [−π, π]
quantum crystal sweep 0 -3.14 3.14 20
quantum crystal sweep 1 -3.14 3.14 20
# ... continue through 15 ...
quantum crystal sweep 15 -3.14 3.14 20
#    → Energy after Round 1 ≈ -12.1 (NOT solved)

# 4. ROUND 2: Re-sweep — optima shift as neighbours changed
quantum crystal sweep 0 -3.14 3.14 20
quantum crystal sweep 1 -3.14 3.14 20
# ... all 16 again ...
#    → Energy after Round 2 ≈ -12.4 (NOT solved, but improving)

# 5. ROUND 3: One more coarse pass
#    → Energy ≈ -12.5 (still not solved, close to plateau)

# 6. FINE-TUNING: Narrow sweeps around current values
quantum crystal sweep 0 1.2 1.9 20
quantum crystal sweep 1 1.0 1.6 20
# ... fine-tune each param in its neighbourhood ...

# 7. Check current state
quantum crystal params
#    → Energy ≈ -12.7 when parameters are precisely tuned

# 8. Manually adjust individual params
quantum crystal set 4 0.82

# 9. Store the solution
quantum crystal store

# 10. Dock your badge → receive flag
quantum flag
```

### Understanding the Physics

At **θ = 0** (all parameters zero), the state is |00000000⟩:
- ZZ terms: all neighbours agree → ⟨ZᵢZᵢ₊₁⟩ = +1 → contributes -J per pair = **-7.0**
- X terms: ⟨Xᵢ⟩ = 0 for |0⟩ → contributes **0**
- Z terms: all ⟨Zᵢ⟩ = +1 → small contribution from δᵢ ≈ **-0.01**
- **Total ≈ -7.01**

With h=1.5, the transverse field is strong — there's a lot of energy to be
gained from the X terms if you can rotate qubits into superposition. But
rotating also disrupts ZZ alignment. The optimal angles are **large**
(around 0.8–1.6 radians in layer 1) and highly **interdependent** —
changing one qubit's angle shifts the optimal angle of its neighbours.

This interdependence is why multiple rounds of sweeps are needed:
- Round 1 finds rough optima assuming neighbours are at |0⟩
- Round 2 adjusts because neighbours are now rotated
- Rounds 3+ squeeze out remaining correlations
- Fine-tuning captures the subtle effects of the disorder terms δᵢ

Edge qubits (q0, q7) have only one ZZ neighbour instead of two,
so they can rotate more aggressively toward the X eigenstate.

### Solution Spoiler

<details>
<summary>Click to reveal a working solution</summary>

Starting from all zeros, coordinate descent converges after 3 coarse rounds
plus fine-tuning. Coarse 20-step sweeps [−π, π] reach ~63% of the analytic
bound; fine-tuning with width ±0.15 pushes past the 64% threshold.

Approximate solution parameters:
```
θ₀  ≈  1.56    θ₈  ≈ -0.31
θ₁  ≈  1.28    θ₉  ≈  0.00
θ₂  ≈  1.07    θ₁₀ ≈ -0.12
θ₃  ≈  0.97    θ₁₁ ≈ -0.14
θ₄  ≈  0.78    θ₁₂ ≈ -0.08
θ₅  ≈  0.83    θ₁₃ ≈ -0.12
θ₆  ≈  1.10    θ₁₄ ≈  0.01
θ₇  ≈  1.52    θ₁₅ ≈ -0.31
```

Layer 1 angles (θ₀–θ₇) are large (∼0.8–1.6) — the strong transverse field
requires significant rotation to capture ⟨X⟩ energy. Interior qubits (q2–q5)
have smaller angles because they have two ZZ constraints vs one for edges.
Layer 2 angles (θ₈–θ₁₅) are small corrections (∼±0.3) — the CZ entangler
after layer 1 creates correlations that the second layer fine-tunes.

Badge CLI commands (copy-paste, 64 sweeps total):
```sh
# Round 1: coarse sweep (Energy: -7.01 → -12.06)
quantum crystal sweep 0 -3.14 3.14 20
quantum crystal sweep 1 -3.14 3.14 20
quantum crystal sweep 2 -3.14 3.14 20
quantum crystal sweep 3 -3.14 3.14 20
quantum crystal sweep 4 -3.14 3.14 20
quantum crystal sweep 5 -3.14 3.14 20
quantum crystal sweep 6 -3.14 3.14 20
quantum crystal sweep 7 -3.14 3.14 20
quantum crystal sweep 8 -3.14 3.14 20
quantum crystal sweep 9 -3.14 3.14 20
quantum crystal sweep 10 -3.14 3.14 20
quantum crystal sweep 11 -3.14 3.14 20
quantum crystal sweep 12 -3.14 3.14 20
quantum crystal sweep 13 -3.14 3.14 20
quantum crystal sweep 14 -3.14 3.14 20
quantum crystal sweep 15 -3.14 3.14 20

# Round 2: re-sweep (Energy: -12.06 → -12.43)
quantum crystal sweep 0 -3.14 3.14 20
quantum crystal sweep 1 -3.14 3.14 20
quantum crystal sweep 2 -3.14 3.14 20
quantum crystal sweep 3 -3.14 3.14 20
quantum crystal sweep 4 -3.14 3.14 20
quantum crystal sweep 5 -3.14 3.14 20
quantum crystal sweep 6 -3.14 3.14 20
quantum crystal sweep 7 -3.14 3.14 20
quantum crystal sweep 8 -3.14 3.14 20
quantum crystal sweep 9 -3.14 3.14 20
quantum crystal sweep 10 -3.14 3.14 20
quantum crystal sweep 11 -3.14 3.14 20
quantum crystal sweep 12 -3.14 3.14 20
quantum crystal sweep 13 -3.14 3.14 20
quantum crystal sweep 14 -3.14 3.14 20
quantum crystal sweep 15 -3.14 3.14 20

# Round 3: re-sweep (Energy: -12.43 → -12.54, plateaus)
quantum crystal sweep 0 -3.14 3.14 20
quantum crystal sweep 1 -3.14 3.14 20
quantum crystal sweep 2 -3.14 3.14 20
quantum crystal sweep 3 -3.14 3.14 20
quantum crystal sweep 4 -3.14 3.14 20
quantum crystal sweep 5 -3.14 3.14 20
quantum crystal sweep 6 -3.14 3.14 20
quantum crystal sweep 7 -3.14 3.14 20
quantum crystal sweep 8 -3.14 3.14 20
quantum crystal sweep 9 -3.14 3.14 20
quantum crystal sweep 10 -3.14 3.14 20
quantum crystal sweep 11 -3.14 3.14 20
quantum crystal sweep 12 -3.14 3.14 20
quantum crystal sweep 13 -3.14 3.14 20
quantum crystal sweep 14 -3.14 3.14 20
quantum crystal sweep 15 -3.14 3.14 20

# Fine-tune: narrow sweeps (Energy: -12.54 → -12.71 = SOLVED!)
quantum crystal sweep 0 1.42 1.72 20
quantum crystal sweep 1 1.11 1.41 20
quantum crystal sweep 2 0.79 1.09 20
quantum crystal sweep 3 0.79 1.09 20
quantum crystal sweep 4 0.48 0.78 20
quantum crystal sweep 5 0.48 0.78 20
quantum crystal sweep 6 0.79 1.09 20
quantum crystal sweep 7 1.42 1.72 20
quantum crystal sweep 8 -0.46 -0.16 20
quantum crystal sweep 9 -0.15 0.15 20
quantum crystal sweep 10 -0.15 0.15 20
quantum crystal sweep 11 -0.46 -0.16 20
quantum crystal sweep 12 -0.15 0.15 20
quantum crystal sweep 13 -0.15 0.15 20
quantum crystal sweep 14 -0.15 0.15 20
quantum crystal sweep 15 -0.46 -0.16 20

# Store result
quantum crystal store
```
</details>

---

## Challenge 2: Grid Optimization (QAOA)

### What is QAOA?

The **Quantum Approximate Optimization Algorithm** (QAOA) is a quantum
algorithm for solving combinatorial optimization problems. Given a "cost"
function over binary variables, QAOA prepares a quantum state that tends
to sample low-cost solutions.

Think of it like shaking a landscape model and watching where marbles
settle — you're tuning the way you shake (4 parameters) to make marbles
collect in the deepest valleys (lowest energy solutions).

### How QAOA Differs from VQE

| | Crystal (VQE) | Grid (QAOA) |
|---|---|---|
| **Goal** | Minimize expectation value | Sample low-energy bitstrings |
| **Circuit** | Generic ansatz (RY + CZ) | Structured: alternating cost & mixer unitaries |
| **Evaluation** | Deterministic ⟨H⟩ | 256 samples from output distribution |
| **Metric** | Single energy value | Best energy, CVaR, hit count |
| **Params** | 16 free angles | 4 angles (γ₁, γ₂, β₁, β₂) |

### Technical Details

- **10 qubits**, QAOA at depth p=2
- **4 parameters**: γ₁, γ₂ (cost angles), β₁, β₂ (mixer angles)
- Circuit: `|+⟩^10 → [cost(γ₁) → mixer(β₁)] → [cost(γ₂) → mixer(β₂)]`
- Evaluation: **256 deterministic samples** from the output state
- Metrics reported: best energy, mean energy, CVaR (bottom 20%), low-energy hit count
- **Three criteria must ALL be met:**
  - Best sampled energy below the energy threshold
  - CVaR (bottom 20% average) below the CVaR threshold
  - At least 20 low-energy samples out of 256

### Strategy Hints

- Start with small values (e.g., 0.5 for all 4 params) and observe metrics
- **γ parameters** control how strongly the cost function shapes the state
- **β parameters** control how much the mixer explores new solutions
- Use `grid hist` to visualize — you want a histogram skewed left (low energies)
- Increasing γ too much can oversaturate; there's a sweet spot
- Try sweeping one parameter at a time using `grid run` repeatedly
- The CVaR metric rewards *consistent* low-energy sampling, not just one lucky shot

### Step-by-step Walkthrough

```sh
# 1. See the cost function and thresholds
quantum grid info

# 2. Try a starting point
quantum grid run 0.5 0.5 0.5 0.5

# 3. Look at the energy histogram
quantum grid hist 0.5 0.5 0.5 0.5

# 4. Adjust — try increasing γ values
quantum grid run 1.0 1.0 0.5 0.5

# 5. Compare histograms, find better parameters
quantum grid hist 1.0 1.0 0.3 0.3

# 6. Keep adjusting until all three criteria are met
# 7. Store your best
quantum grid store <γ₁> <γ₂> <β₁> <β₂>

# 8. Verify
quantum grid status

# 9. Dock → receive flag
quantum flag
```

---

## Validation & Flags

### How Docking Works

1. Store your solution: `quantum crystal store` or `quantum grid store <params>`
2. Check status: `quantum crystal status` / `quantum grid status`
3. Insert badge into the **quantum dock station** (dock-quantum2)
4. The dock reads your stored parameters, **independently recomputes** the energy
5. If your solution is valid, the dock sends the flag back to your badge
6. Check flags: `quantum flag`

### What Gets Validated

The dock does NOT trust a "solved" bit. It:
- Reads your stored parameters (milliradians)
- Verifies the CRC-32 checksum
- Recomputes the quantum simulation using the same Hamiltonian
- Compares the recomputed energy against your reported energy
- Checks that the energy meets the solve criteria
- Only then issues the flag

### Checking Your Flags

```
> quantum flag
  crystal flag = FLAG{cryst4l_tun3d_VQE_2026}
  grid flag = FLAG{gr1d_0pt1m1z3d_QAOA_2026}
```

---

## Resetting Progress

```sh
# Reset Crystal challenge state
quantum crystal reset

# Reset Grid challenge state
quantum grid reset

# Check that flags are gone
quantum flag
```

Note: Resetting erases stored parameters/scores. Flags that were already
captured and stored remain (they're in separate NVS keys).

---

## Architecture Notes (for challenge developers)

### NVS Schema

Crystal state is stored as a 42-byte blob (key `"crystal"` in namespace `"quantum"`):
- version (u8), solved (u8), 16 params (i16 milliradians), energy×1000 (i32), CRC-32 (u32)

Grid state is stored as a 24-byte blob (key `"grid"` in namespace `"quantum"`):
- version (u8), solved (u8), γ₁/γ₂/β₁/β₂ (i16 milliradians), best_energy×1000 (i32), low_hits (u16), CVaR×1000 (i32), CRC-32 (u32)

### Dock Protocol

| Sub-opcode | Direction | Description |
|---|---|---|
| 0x03 | Dock → Badge | Request CrystalState (42 bytes) |
| 0x04 | Dock → Badge | Request GridState (24 bytes) |
| 0x05 | Dock → Badge | Crystal validation result + flag |
| 0x06 | Dock → Badge | Grid validation result + flag |

### Reproducibility

Both badge and dock use identical deterministic PRNG seeds:
- Crystal Hamiltonian: seed `0x4E534543` ("NSEC")
- Grid Hamiltonian: seed `0x47524944` ("GRID")
- Grid sampling: seed `0xDEADBEEF`

This ensures the dock can independently verify all badge-side computations.

### Key Design Decisions

- **RY over RX**: RY gates produce real-valued superpositions where ⟨X⟩ = sin(θ),
  directly responding to the transverse field. RX from |0⟩ gives ⟨X⟩ = 0 for all θ,
  making the h-term invisible to the optimizer.
- **CZ chain (not ring)**: Matches the ZZ chain topology of the TFIM Hamiltonian.
  A ring entangler would add a spurious q7↔q0 correlation.
- **h/J = 1.5**: Places the system near the quantum phase transition (h/J ≈ 1.0
  for infinite chains). The strong transverse field means the ground state has
  significant quantum correlations that require iterative optimization to capture.
  With h=0.3 (ordered phase), a single sweep round trivially solves the challenge.
- **64% of analytic bound**: The threshold is set at 64% of the analytic lower
  bound $E_{min} \geq -(N{-}1)J - Nh - \sum|\delta_i|$. Coarse sweeps reach ~63%;
  fine-tuning is needed to push past the threshold. This ensures players spend
  meaningful time iteratively refining their parameters.
