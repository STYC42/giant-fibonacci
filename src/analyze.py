import re

def parse_trace(path):
    steps = []
    total = None
    with open(path) as f:
        for line in f:
            m = re.search(r'fib\s+n=(\d+)\s+done\s+([\d.]+) s', line)
            if m:
                steps.append((int(m.group(1)), float(m.group(2))))
            m = re.search(r'Doubling done in ([\d.]+) s', line)
            if m:
                total = float(m.group(1))
    return steps, total

def parse_io(path):
    ios = []
    with open(path) as f:
        for line in f:
            m = re.search(r'(spill|reload)\s+\S+\s+([\d.]+) MB\s+([\d.]+) s\s+([\d.]+) MB/s', line)
            if m:
                ios.append((m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4))))
    return ios

path = 'trace_3e11.log'
steps, total = parse_trace(path)
ios = parse_io(path)

print(f"Total doubling: {total} s")
print("\nDerniers niveaux:")
for n, t in steps[-8:]:
    print(f"  n={n:<18}  {t:.3f} s")

# Karatsuba ?
with open(path) as f:
    content = f.read()
karatsuba_lines = [l for l in content.splitlines() if 'Karatsuba' in l]
print(f"\nAppels Karatsuba out-of-core: {len(karatsuba_lines)}")
for l in karatsuba_lines:
    print(" ", l.strip())

# I/O grands transferts
big = [(op, mb, dt, rate) for op, mb, dt, rate in ios if mb >= 100]
print(f"\nTransferts >= 100 MB: {len(big)}")
total_io = sum(dt for *_, dt, rate in big)
spill_r  = [rate for op, mb, dt, rate in big if op=='spill']
reload_r = [rate for op, mb, dt, rate in big if op=='reload']
print(f"  Temps I/O cumulé: {total_io:.1f} s")
print(f"  spill  moy={sum(spill_r)/len(spill_r):.0f} MB/s  (n={len(spill_r)})")
print(f"  reload moy={sum(reload_r)/len(reload_r):.0f} MB/s  (n={len(reload_r)})")

import math
print("\n--- Ratios de doublement (niveaux CPU-dominant) ---")
for i in range(1, min(6, len(steps))):
    n1, t1 = steps[-(i+1)]
    n2, t2 = steps[-i]
    if t1 > 0:
        print(f"  n={n2:<15} / n={n1:<15}  ratio={t2/t1:.3f}  (attendu ~2.0-2.1)")
