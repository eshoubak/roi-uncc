#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dump_z_from_glm_updated.py

Updated for your new review_3.glm style where:
  - object line_configuration provides z11..z33 directly (ohm/mile)
  - object overhead_line provides from/to/length/configuration

It prints:
  - Zabc for each configuration (ohm/mile)
  - For each overhead_line: total Z (ohm) and per-unit Z on a (VLL kV, Sbase MVA) base
  - Per-phase diagonal Rpu/Xpu (useful for per-phase RAW equivalents)

Defaults:
  - If overhead_line length has no explicit unit, assumes feet (GridLAB-D common default).
  - Supports length with explicit units: ft, mi, m, km (e.g., "500 ft", "1 mi").
"""

import argparse
import re
from pathlib import Path
import numpy as np


def _strip_inline_comment(s: str) -> str:
    if s is None:
        return ""
    return s.split("//", 1)[0].strip()


def parse_complex(s: str) -> complex:
    """Parse GridLAB-D complex strings (supports j or i)."""
    if s is None:
        raise ValueError("Empty complex string")
    ss = _strip_inline_comment(str(s)).strip().strip(";").strip()
    ss = ss.strip("'\"").replace("i", "j").rstrip(",")
    if ss == "":
        raise ValueError(f"Empty complex after stripping: {s!r}")
    return complex(ss)


def parse_objects(glm_text: str):
    """
    Lightweight parser for:
      object <class> { ... };
    Returns list of (class, props dict)
    """
    objs = []
    pat = re.compile(r'object\s+(\w+)\s*{([^}]*)}', re.S)
    for m in pat.finditer(glm_text):
        cls = m.group(1).strip()
        body = m.group(2)
        props = {}
        for raw in body.splitlines():
            line = raw.strip()
            if not line or line.startswith("//"):
                continue
            line = _strip_inline_comment(line)
            if not line:
                continue
            if line.endswith(";"):
                line = line[:-1].rstrip()
            if " " not in line:
                continue
            k, v = line.split(None, 1)
            props[k.strip()] = v.strip().strip("'\"")
        objs.append((cls, props))
    return objs


def miles_from_length(length_str: str, default_unit: str = "ft") -> float:
    """
    Convert overhead_line length to miles.

    - If length has no unit token (e.g., "5280"), assume default_unit (ft by default).
    - If length has explicit unit (e.g., "1 mi", "500 ft", "1000 m"), convert accordingly.
    """
    s = str(length_str).strip()
    parts = s.split()
    if len(parts) == 1:
        val = float(parts[0])
        unit = default_unit.lower()
    else:
        val = float(parts[0])
        unit = parts[1].lower()

    if unit in ("ft", "feet"):
        return val / 5280.0
    if unit in ("mi", "mile", "miles"):
        return val
    if unit in ("m", "meter", "meters"):
        return val / 1609.344
    if unit in ("km", "kilometer", "kilometers"):
        return val / 1.609344

    raise ValueError(f"Unknown length unit in {length_str!r}. Supported: ft, mi, m, km.")


def Zabc_from_line_config(cfg_props: dict) -> np.ndarray:
    """
    Build 3x3 Zabc (ohm/mile) from z11..z33 entries.
    Missing entries default to 0 (common for mutual terms).
    """
    def getz(key: str) -> complex:
        return parse_complex(cfg_props[key]) if key in cfg_props else 0.0 + 0.0j

    Z = np.zeros((3, 3), dtype=complex)
    Z[0, 0] = getz("z11")
    Z[0, 1] = getz("z12")
    Z[0, 2] = getz("z13")
    Z[1, 0] = getz("z21")
    Z[1, 1] = getz("z22")
    Z[1, 2] = getz("z23")
    Z[2, 0] = getz("z31")
    Z[2, 1] = getz("z32")
    Z[2, 2] = getz("z33")
    return Z


def main():
    ap = argparse.ArgumentParser(
        description="Dump per-branch impedances from GLM line_configuration z11..z33 (ohm/mile)."
    )
    ap.add_argument("glm", nargs="?", default="review_3.glm", help="Path to GLM file (default: review_3.glm)")
    ap.add_argument("--vll-kv", type=float, default=4.16, help="Line-to-line base voltage in kV (default: 4.16)")
    ap.add_argument("--sbase-mva", type=float, default=100.0, help="System base power in MVA (default: 100)")
    ap.add_argument(
        "--default-length-unit",
        default="ft",
        choices=["ft", "mi", "m", "km"],
        help="Assumed unit if overhead_line length has no unit token (default: ft)",
    )
    ap.add_argument("--print-full-z", action="store_true", help="Print full 3x3 Zpu matrices per branch")
    args = ap.parse_args()

    glm_path = Path(args.glm)
    text = glm_path.read_text()

    objs = parse_objects(text)
    bycls = {}
    for cls, p in objs:
        bycls.setdefault(cls, []).append(p)

    configs = {p.get("name", ""): p for p in bycls.get("line_configuration", []) if "name" in p}
    lines = bycls.get("overhead_line", [])

    if not configs:
        raise SystemExit("No line_configuration objects found (expected z11..z33).")
    if not lines:
        raise SystemExit("No overhead_line objects found.")

    # Base impedance (ohm) on LL base: Zbase = (kV^2)/MVA
    zbase = (args.vll_kv ** 2) / args.sbase_mva

    print(f"GLM: {glm_path}")
    print(f"Using VLL base = {args.vll_kv} kV, Sbase = {args.sbase_mva} MVA => Zbase = {zbase:.6f} ohm")
    print(f"Assuming overhead_line length unit = {args.default_length_unit} when unit is not specified.")
    print()

    # 1) Print Zabc per configuration
    Zabc_by_cfg = {}
    print("=== line_configuration Zabc (ohm/mile) ===")
    for cfg_name, cfg in configs.items():
        Zabc = Zabc_from_line_config(cfg)
        key = cfg_name.strip('"')
        Zabc_by_cfg[key] = Zabc
        print(f"{cfg_name}:")
        for r in range(3):
            print("  " + "  ".join(f"{Zabc[r,c].real:.6f}+j{Zabc[r,c].imag:.6f}" for c in range(3)))
        print()

    # 2) Per-branch totals
    print("=== Per-branch impedance (from overhead_line objects) ===")
    for ln in lines:
        name = ln.get("name", "(unnamed)")
        frm = ln.get("from", "?")
        to = ln.get("to", "?")
        cfgname = ln.get("configuration", "").strip('"')
        length = ln.get("length", None)

        if cfgname not in Zabc_by_cfg:
            # sometimes configuration may be stored with quotes or whitespace differences
            cfgname2 = cfgname.strip('"').strip()
            if cfgname2 in Zabc_by_cfg:
                cfgname = cfgname2
            else:
                raise SystemExit(f"Line {name} references configuration {cfgname!r}, but it was not found.")
        if length is None:
            raise SystemExit(f"Line {name} has no 'length' property.")

        L_mi = miles_from_length(length, default_unit=args.default_length_unit)
        Zabc = Zabc_by_cfg[cfgname]          # ohm/mile
        Ztot = Zabc * L_mi                   # ohm
        Zpu = Ztot / zbase                   # pu

        print(f"{name}: {frm} -> {to}  length={length}  ({L_mi:.6f} mi)  cfg={cfgname}")
        print(f"  Phase A: R_ohm={Ztot[0,0].real:.6f}  X_ohm={Ztot[0,0].imag:.6f}   |  Rpu={Zpu[0,0].real:.8f}  Xpu={Zpu[0,0].imag:.8f}")
        print(f"  Phase B: R_ohm={Ztot[1,1].real:.6f}  X_ohm={Ztot[1,1].imag:.6f}   |  Rpu={Zpu[1,1].real:.8f}  Xpu={Zpu[1,1].imag:.8f}")
        print(f"  Phase C: R_ohm={Ztot[2,2].real:.6f}  X_ohm={Ztot[2,2].imag:.6f}   |  Rpu={Zpu[2,2].real:.8f}  Xpu={Zpu[2,2].imag:.8f}")
        if args.print_full_z:
            print("  Full Zpu matrix:")
            for r in range(3):
                print("    " + "  ".join(f"{Zpu[r,c].real:+.8f}{Zpu[r,c].imag:+.8f}j" for c in range(3)))
        print()


if __name__ == "__main__":
    main()
