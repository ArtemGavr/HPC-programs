"""
Hello World exercise: Parallel Scaling part.

Fits Amdahl's law to strong.dat, Gustafson's law to weak.dat, and (challenge)
the Universal Scaling Law to both. Prints the answers and saves plots.

Run on your Mac (not on Hamilton's login node):
    pip install numpy scipy matplotlib
    python3 scaling.py
"""
import numpy as np
from scipy.optimize import curve_fit
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---------------------------------------------------------------- data
strong = np.loadtxt("strong.dat", delimiter=",")
p_s, t_s = strong[:, 0], strong[:, 1]

weak = np.loadtxt("weak.dat", delimiter=",")
p_w, t_w = weak[:, 0], weak[:, 1]
ts_given = weak[:, 2] if weak.shape[1] > 2 else None   # extra column, if present

# ================================================================ STRONG
# Amdahl: t(p) = t1 * (f + (1 - f) / p).  We keep t1 = measured t(1) and fit f.
t1 = t_s[0]
def amdahl_time(p, f):
    return t1 * (f + (1 - f) / p)

def fit_amdahl(mask):
    (f,), _ = curve_fit(amdahl_time, p_s[mask], t_s[mask], p0=[0.01], bounds=(0, 1))
    return f

f_all = fit_amdahl(np.ones_like(p_s, dtype=bool))
f_128 = fit_amdahl(p_s <= 128)       # leave out the p=256 point where time goes UP

speedup_s = t1 / t_s
print("=== STRONG SCALING (Amdahl) ===")
print(" p    t(p)      speedup  efficiency")
for p, t, s in zip(p_s, t_s, speedup_s):
    print(f"{int(p):4d} {t:9.4f} {s:9.2f} {s/p:9.2f}")
print(f"serial fraction f, fit to all points:    {f_all:.4f}  -> max speedup 1/f = {1/f_all:.0f}")
print(f"serial fraction f, fit to p <= 128 only: {f_128:.4f}  -> max speedup 1/f = {1/f_128:.0f}")

pp = np.logspace(0, np.log2(256), 200, base=2)
plt.figure(figsize=(6, 4.5))
plt.loglog(p_s, speedup_s, "o", label="measured  t(1)/t(p)")
plt.loglog(pp, 1 / (f_all + (1 - f_all) / pp), "-", label=f"Amdahl, f = {f_all:.4f} (all points)")
plt.loglog(pp, 1 / (f_128 + (1 - f_128) / pp), "--", label=f"Amdahl, f = {f_128:.4f} (p ≤ 128)")
plt.loglog(pp, pp, ":", color="grey", label="ideal (S = p)")
plt.xscale("log", base=2); plt.yscale("log", base=2)
plt.xlabel("threads p"); plt.ylabel("speedup S(p)")
plt.title("Strong scaling (strong.dat)"); plt.legend(); plt.grid(alpha=.3)
plt.tight_layout(); plt.savefig("strong_scaling.png", dpi=150)

# ================================================================ WEAK
# Gustafson: t(1) = t(p) f + t(p) (1 - f) p  ->  scaled speedup S(p) = f + (1 - f) p
# With only (p, t(p)) we can't fit f: each p is a different-size problem with its own
# unknown single-process time. So we generate a serial-time column, Eq. (5):
#   ts(p) = 0.4 * t(1) + xi * p,  xi ~ U[0, 0.01]
rng = np.random.default_rng(42)       # fixed seed so the numbers are repeatable
xi = rng.uniform(0, 1e-2, size=p_w.size)
ts_gen = 0.4 * t_w[0] + xi * p_w

def gustafson_analysis(ts, label):
    beta = np.clip(ts / t_w, 0, 1)                  # serial fraction at each p
    S_meas = beta + (1 - beta) * p_w                # implied scaled speedup T(p,1)/T(p,p)
    # Gustafson assumes ONE f for all p. We take the median beta as that f
    # (median, not mean, so one noisy point can't drag it around).
    f = np.median(beta)
    print(f"\n--- serial times: {label} ---")
    print(" p    t(p)     ts(p)   beta=ts/t  scaled S")
    for p, t, s, b, S in zip(p_w, t_w, ts, beta, S_meas):
        print(f"{int(p):4d} {t:8.4f} {s:8.4f} {b:8.3f} {S:10.2f}")
    print(f"beta ranges {beta.min():.2f}..{beta.max():.2f}; serial fraction f = median beta = {f:.3f}")
    return beta, S_meas, f

print("\n=== WEAK SCALING (Gustafson) ===")
print("t(1)/t(p) (weak efficiency):", np.round(t_w[0] / t_w, 2))
beta_g, S_g, f_g = gustafson_analysis(ts_gen, "generated from Eq. (5), seed 42")
if ts_given is not None:
    beta_d, S_d, f_d = gustafson_analysis(ts_given, "third column already in weak.dat")

plt.figure(figsize=(6, 4.5))
plt.loglog(p_w, S_g, "o", label="from generated ts(p)")
if ts_given is not None:
    plt.loglog(p_w, S_d, "s", mfc="none", label="from weak.dat column 3")
pp = np.logspace(0, 8, 200, base=2)
plt.loglog(pp, f_g + (1 - f_g) * pp, "-", label=f"Gustafson, f = {f_g:.3f}")
plt.loglog(pp, pp, ":", color="grey", label="ideal (S = p)")
plt.xscale("log", base=2); plt.yscale("log", base=2)
plt.xlabel("processes p"); plt.ylabel("scaled speedup S(p)")
plt.title("Weak scaling (weak.dat)"); plt.legend(); plt.grid(alpha=.3)
plt.tight_layout(); plt.savefig("weak_scaling.png", dpi=150)

# ================================================================ CHALLENGE: USL
# X(N) = gamma N / (1 + alpha (N-1) + beta N (N-1))
#   alpha = contention (queueing for a shared thing), beta = coherency (cross-talk cost)
# Our X is normalised so X(1) = 1, which means gamma = 1; we fit alpha and beta.
def usl(N, alpha, beta, gamma=1.0):
    return gamma * N / (1 + alpha * (N - 1) + beta * N * (N - 1))

X_strong = t1 / t_s                    # relative throughput, fixed problem
X_weak = p_w * t_w[0] / t_w            # p times the work, in time t(p)
print("\n=== CHALLENGE: Universal Scaling Law ===")
fig, ax = plt.subplots(1, 2, figsize=(11, 4.5))
for a, N, X, name in [(ax[0], p_s, X_strong, "strong"), (ax[1], p_w, X_weak, "weak")]:
    par, _ = curve_fit(lambda N, a, b: usl(N, a, b), N, X, p0=[0.01, 1e-5],
                       bounds=(0, np.inf))
    resid = np.sqrt(np.mean((usl(N, *par) - X) ** 2))
    Npk = np.sqrt((1 - par[0]) / par[1]) if par[1] > 0 else np.inf
    print(f"{name}: alpha={par[0]:.4g} beta={par[1]:.3g}  "
          f"RMS error={resid:.2f}  peak throughput at N≈{Npk:.0f}")
    NN = np.logspace(0, 8, 200, base=2)
    a.loglog(N, X, "o", label="data")
    a.loglog(NN, usl(NN, *par), "-", label="USL fit")
    a.set_xscale("log", base=2); a.set_yscale("log", base=2)
    a.set_xlabel("N"); a.set_ylabel("relative throughput X(N)")
    a.set_title(f"USL fit, {name} data"); a.legend(); a.grid(alpha=.3)
# RMS error of the Amdahl fit, for comparison
print(f"(Amdahl RMS error on strong speedup: "
      f"{np.sqrt(np.mean((1/(f_all+(1-f_all)/p_s) - X_strong)**2)):.2f})")
plt.tight_layout(); plt.savefig("usl_fits.png", dpi=150)
print("\nSaved strong_scaling.png, weak_scaling.png, usl_fits.png")
