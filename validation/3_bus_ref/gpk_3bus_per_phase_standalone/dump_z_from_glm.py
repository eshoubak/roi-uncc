#!/usr/bin/env python3
import re, math
import numpy as np
from pathlib import Path

# Carson/overhead line constants for 60 Hz (common GridLAB-D style)
K = 0.12134   # ohm/mile multiplier on ln() term
C = 7.93402   # dimensionless constant

def z_self(r_ohm_per_mile, gmr_ft):
    return r_ohm_per_mile + 1j * K * (math.log(1.0 / gmr_ft) + C)

def z_mutual(d_ft):
    return 1j * K * (math.log(1.0 / d_ft) + C)

def parse_objects(glm_text):
    objs = []
    pat = re.compile(r'object\s+(\w+)\s*{([^}]*)}', re.S)
    for m in pat.finditer(glm_text):
        cls = m.group(1)
        body = m.group(2)
        props = {}
        for line in body.splitlines():
            line = line.strip().rstrip(';')
            if not line or line.startswith('//'):
                continue
            if line.startswith('name'):
                _, val = line.split(None, 1)
                props['name'] = val.strip()
            elif ' ' in line:
                k, rest = line.split(None, 1)
                props[k] = rest.strip().strip("'\"")
        objs.append((cls, props))
    return objs

def kron_reduce(Z4):
    Zpp = Z4[:3, :3]
    Zpn = Z4[:3, 3:]
    Znp = Z4[3:, :3]
    Znn = Z4[3:, 3:]
    return Zpp - Zpn @ np.linalg.inv(Znn) @ Znp

def miles_from_length(length_str):
    # In your review_3.glm, lengths are plain numbers like 500, 100, 50.
    # GridLAB-D overhead_line length default is typically feet if unit not specified.
    # We'll assume feet. If you later discover it's meters, adjust here.
    ft = float(length_str)
    return ft / 5280.0

def main(glm_path, vll_kv=4.16, sbase_mva=100.0):
    text = Path(glm_path).read_text()

    objs = parse_objects(text)
    bycls = {}
    for cls, p in objs:
        bycls.setdefault(cls, []).append(p)

    conductors = {p['name']: p for p in bycls.get('overhead_line_conductor', [])}
    spacings   = {p['name']: p for p in bycls.get('line_spacing', [])}
    configs    = {p['name']: p for p in bycls.get('line_configuration', [])}
    lines      = bycls.get('overhead_line', [])

    if not configs:
        raise SystemExit("No line_configuration objects found in GLM.")
    if not conductors or not spacings:
        raise SystemExit("Missing overhead_line_conductor or line_spacing objects.")

    # Base impedance (ohms) for per-unit conversion on LL base
    zbase = (vll_kv ** 2) / sbase_mva

    def build_Z4(cfg):
        # Order A,B,C,N
        ca = conductors[cfg['conductor_A']]
        cb = conductors[cfg['conductor_B']]
        cc = conductors[cfg['conductor_C']]
        cn = conductors[cfg['conductor_N']]
        sp = spacings[cfg['spacing']]

        rA, gA = float(ca['resistance']), float(ca['geometric_mean_radius'])
        rB, gB = float(cb['resistance']), float(cb['geometric_mean_radius'])
        rC, gC = float(cc['resistance']), float(cc['geometric_mean_radius'])
        rN, gN = float(cn['resistance']), float(cn['geometric_mean_radius'])

        DAB = float(sp['distance_AB'])
        DAC = float(sp['distance_AC'])
        DBC = float(sp['distance_BC'])
        DAN = float(sp['distance_AN'])
        DBN = float(sp['distance_BN'])
        DCN = float(sp['distance_CN'])

        Z = np.zeros((4,4), dtype=complex)
        Z[0,0] = z_self(rA, gA)
        Z[1,1] = z_self(rB, gB)
        Z[2,2] = z_self(rC, gC)
        Z[3,3] = z_self(rN, gN)

        Z[0,1] = Z[1,0] = z_mutual(DAB)
        Z[0,2] = Z[2,0] = z_mutual(DAC)
        Z[1,2] = Z[2,1] = z_mutual(DBC)
        Z[0,3] = Z[3,0] = z_mutual(DAN)
        Z[1,3] = Z[3,1] = z_mutual(DBN)
        Z[2,3] = Z[3,2] = z_mutual(DCN)
        return Z

    print(f"Using VLL base = {vll_kv} kV, Sbase = {sbase_mva} MVA => Zbase = {zbase:.6f} ohm")
    print()

    # 1) Print Kron-reduced Zabc for each configuration
    Zabc_by_cfg = {}
    for name, cfg in configs.items():
        Z4 = build_Z4(cfg)
        Zabc = kron_reduce(Z4)  # ohm/mile
        Zabc_by_cfg[name] = Zabc
        print(f"=== {name} (Kron-reduced Zabc, ohm/mile) ===")
        for i, row in enumerate(Zabc):
            print("  " + "  ".join(f"{z.real:.6f}+j{z.imag:.6f}" for z in row))
        print()

    # 2) For each overhead_line branch, compute per-phase self Z (diagonal) and convert to pu
    print("=== Per-branch diagonal (self) impedance used for single-phase RAW equivalents ===")
    for ln in lines:
        cfgname = ln['configuration']
        L_mi = miles_from_length(ln['length'])
        Zabc = Zabc_by_cfg[cfgname]  # ohm/mile
        Ztot = Zabc * L_mi           # ohms
        Zpu  = Ztot / zbase          # pu
        print(f"{ln['name']}: {ln['from']} -> {ln['to']}  length={ln['length']} (assumed ft => {L_mi:.6f} mi)  cfg={cfgname}")
        print(f"  Phase A: Rpu={Zpu[0,0].real:.8f}  Xpu={Zpu[0,0].imag:.8f}")
        print(f"  Phase B: Rpu={Zpu[1,1].real:.8f}  Xpu={Zpu[1,1].imag:.8f}")
        print(f"  Phase C: Rpu={Zpu[2,2].real:.8f}  Xpu={Zpu[2,2].imag:.8f}")
        print()

    print("NOTE: If your GLD length units are NOT feet, edit miles_from_length() accordingly.")

if __name__ == "__main__":
    main("review_3.glm", vll_kv=4.16, sbase_mva=100.0)
