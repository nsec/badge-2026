"""QAOA Grid: numpy-vectorized solver for speed."""
import numpy as np
import time
from scipy.optimize import minimize, differential_evolution

def xor32(seed):
    seed = int(np.uint32(seed) ^ (np.uint32(seed) << np.uint32(13)))
    seed = int(np.uint32(seed) ^ (np.uint32(seed) >> np.uint32(17)))
    seed = int(np.uint32(seed) ^ (np.uint32(seed) << np.uint32(5)))
    return seed & 0xFFFFFFFF

def rng_float(seed, lo, hi):
    seed = xor32(seed)
    return seed, lo + (seed / 4294967296.0) * (hi - lo)

NQ = 10; DIM = 1024
seed_v = 0x47524944
h_f = []
for _ in range(NQ):
    seed_v, v = rng_float(seed_v, -1.0, 1.0); h_f.append(v)
edges = []; J = {}
for i in range(NQ):
    j = (i+1)%NQ; seed_v, w = rng_float(seed_v, -1.0, 1.0)
    J[(i,j)] = w; edges.append((i,j,w))
for _ in range(5):
    seed_v = xor32(seed_v); ie = seed_v % NQ
    seed_v = xor32(seed_v); je = seed_v % NQ
    if ie == je: je = (je+1)%NQ
    if ie > je: ie, je = je, ie
    if (ie,je) not in J:
        seed_v, w = rng_float(seed_v, -0.5, 0.5)
        J[(ie,je)] = w; edges.append((ie,je,w))

diag_np = np.zeros(DIM)
for z in range(DIM):
    c = 0.0
    for qi in range(NQ):
        c += h_f[qi] * (-1.0 if (z & (1<<qi)) else 1.0)
    for (ei,ej,ew) in edges:
        c += ew * (-1.0 if (z&(1<<ei)) else 1.0) * (-1.0 if (z&(1<<ej)) else 1.0)
    diag_np[z] = c

opt = float(np.min(diag_np))
thresh = opt + abs(opt)*0.05
cvar_thresh = opt + abs(opt)*0.20
print(f"Optimal: {opt:.4f}  Thresh: {thresh:.4f}  CVaR: {cvar_thresh:.4f}")

# Precompute sample randoms
s = 0xDEADBEEF
sus = []
for _ in range(256):
    s = xor32(s); sus.append(s/4294967296.0)
sus = np.array(sus)

# Precompute RX index pairs for each qubit
rx_pairs = []
for q in range(NQ):
    step = 1 << q
    i0 = np.array([i for i in range(DIM) if not (i & step)], dtype=np.int32)
    i1 = i0 | step
    rx_pairs.append((i0, i1))

def qaoa_eval_fast(params):
    g1, g2, b1, b2 = params
    sv = np.full(DIM, 1.0/np.sqrt(DIM), dtype=np.complex128)

    # Cost unitary (fully vectorized)
    sv *= np.exp(-1j * g1 * diag_np)
    # Mixer RX(2*b1)
    c, s = np.cos(b1), np.sin(b1)  # note: angle is 2*b1, cos(angle/2)=cos(b1)
    for i0, i1 in rx_pairs:
        a = sv[i0].copy(); b = sv[i1].copy()
        sv[i0] = c*a - 1j*s*b
        sv[i1] = -1j*s*a + c*b

    sv *= np.exp(-1j * g2 * diag_np)
    c, s = np.cos(b2), np.sin(b2)
    for i0, i1 in rx_pairs:
        a = sv[i0].copy(); b = sv[i1].copy()
        sv[i0] = c*a - 1j*s*b
        sv[i1] = -1j*s*a + c*b

    # Sample
    probs = np.abs(sv)**2
    cumprob = np.cumsum(probs)
    z_samp = np.searchsorted(cumprob, sus)
    z_samp = np.clip(z_samp, 0, DIM-1)
    energies = np.sort(diag_np[z_samp])

    best = float(energies[0])
    tail = 256 // 5
    cvar = float(np.mean(energies[:tail]))
    low_t = best + abs(best)*0.1 + 0.5
    hits = int(np.sum(energies < low_t))
    return best, cvar, hits

# Time one eval
t0 = time.time()
b, cv, h = qaoa_eval_fast([1.0, 1.0, 0.5, 0.5])
print(f"One eval: {time.time()-t0:.4f}s  best={b:.4f} cvar={cv:.4f} hits={h}")

def objective(x):
    best, cvar, hits = qaoa_eval_fast(x)
    penalty = 0
    if best >= thresh: penalty += 10*(best - thresh)
    if hits < 20: penalty += 0.1*(20 - hits)
    return cvar + penalty

# Differential evolution
print("\n=== Differential Evolution ===")
t0 = time.time()
bounds = [(0.1, 6.0), (0.1, 6.0), (0.05, 2.0), (0.05, 2.0)]
result = differential_evolution(objective, bounds, seed=42, maxiter=50,
                                 popsize=15, tol=0.001)
elapsed = time.time() - t0
g1, g2, b1, b2 = result.x
best, cvar, hits = qaoa_eval_fast(result.x)
solved = best < thresh and cvar < cvar_thresh and hits >= 20
print(f"Done in {elapsed:.0f}s ({result.nfev} evals)")
print(f"  γ=({g1:.4f}, {g2:.4f}) β=({b1:.4f}, {b2:.4f})")
print(f"  best={best:.4f} cvar={cvar:.4f} hits={hits}")
print(f"  SOLVED: {solved}")

# Multistart Nelder-Mead for refinement
print("\n=== Multistart Nelder-Mead ===")
rng = np.random.default_rng(42)
best_sol = None; best_obj = 999
for trial in range(50):
    x0 = rng.uniform([0.3, 0.3, 0.1, 0.1], [5.0, 5.0, 1.5, 1.5])
    res = minimize(objective, x0, method='Nelder-Mead',
                   options={'maxiter': 200, 'xatol': 0.005, 'fatol': 0.005})
    if res.fun < best_obj:
        best_obj = res.fun
        best_sol = res.x
        b, cv, h = qaoa_eval_fast(res.x)
        sol = b < thresh and cv < cvar_thresh and h >= 20
        if trial % 5 == 0 or sol:
            print(f"  T{trial}: γ=({res.x[0]:.2f},{res.x[1]:.2f}) β=({res.x[2]:.2f},{res.x[3]:.2f}) "
                  f"best={b:.4f} cvar={cv:.4f} hits={h} {'SOLVED!' if sol else ''}")
        if sol:
            break

b, cv, h = qaoa_eval_fast(best_sol)
sol = b < thresh and cv < cvar_thresh and h >= 20
print(f"\nBest: γ=({best_sol[0]:.4f},{best_sol[1]:.4f}) β=({best_sol[2]:.4f},{best_sol[3]:.4f})")
print(f"  best={b:.4f} cvar={cv:.4f} hits={h}")
print(f"  SOLVED: {sol}")
print(f"\nBadge command:")
print(f"quantum grid run {best_sol[0]:.4f} {best_sol[1]:.4f} {best_sol[2]:.4f} {best_sol[3]:.4f}")
print(f"quantum grid store {best_sol[0]:.4f} {best_sol[1]:.4f} {best_sol[2]:.4f} {best_sol[3]:.4f}")

if not sol:
    print("\n*** CHALLENGE MAY BE UNSOLVABLE with current thresholds! ***")
    print("Relaxing criteria test:")
    for pct in [0.10, 0.15, 0.20, 0.25, 0.30]:
        t2 = opt + abs(opt)*pct
        cv2 = opt + abs(opt)*(pct*3)
        b2, cv2_val, h2 = qaoa_eval_fast(best_sol)
        print(f"  {pct*100:.0f}%: thresh={t2:.4f} best={b2:.4f}{'✓' if b2<t2 else '✗'}  "
              f"cvar_t={opt+abs(opt)*(pct*3):.4f} cvar={cv2_val:.4f}{'✓' if cv2_val<opt+abs(opt)*(pct*3) else '✗'}  "
              f"hits={h2}{'✓' if h2>=20 else '✗'}")
