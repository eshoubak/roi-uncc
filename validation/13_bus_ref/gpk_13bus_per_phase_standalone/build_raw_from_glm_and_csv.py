#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import sys
import csv
from collections import defaultdict

if len(sys.argv) != 3:
    raise SystemExit(
        "Usage: python3 build_raw_from_glm_and_csv.py gld_13bus.glm line_impedances.csv"
    )

GLM = sys.argv[1]
CSVF = sys.argv[2]


def strip_comments(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if "//" in line:
            line = line.split("//", 1)[0]
        lines.append(line)
    return "\n".join(lines)


def get(prop, body):
    m = re.search(rf"\b{re.escape(prop)}\s+([^;]+);", body)
    return m.group(1).strip() if m else None


def cpx(s):
    return complex(s.replace("i", "j"))


text = open(GLM, "r", encoding="utf-8").read()
text = strip_comments(text)

obj_pat = re.compile(r"object\s+(\w+)\s*{([^}]*)}", re.S)

nodes = set()
edges = []
loads = defaultdict(lambda: {"A": 0 + 0j, "B": 0 + 0j, "C": 0 + 0j})

for typ, body in obj_pat.findall(text):
    typ = typ.strip()

    if typ in ["node", "meter"]:
        name = get("name", body)
        if name:
            nodes.add(name)

    elif typ == "overhead_line":
        f = get("from", body)
        t = get("to", body)
        if f and t:
            edges.append((f, t))
            nodes.add(f)
            nodes.add(t)

    elif typ == "load":
        name = get("name", body)
        parent = get("parent", body)

        # Parented load goes on parent bus.
        # Non-parented load stays on its own named bus.
        bus_name = parent if parent else name
        if not bus_name:
            continue

        nodes.add(bus_name)

        for ph in ["A", "B", "C"]:
            val = get(f"constant_power_{ph}", body)
            if val:
                loads[bus_name][ph] += cpx(val)


# --- keep only connected buses (plus Meter1 if present) ---
connected = set()
for f, t in edges:
    connected.add(f)
    connected.add(t)

if "Meter1" in nodes:
    connected.add("Meter1")

# If a load bus is not connected by any branch, warn and exclude it.
isolated_load_buses = sorted(set(loads.keys()) - connected)
for b in isolated_load_buses:
    print(f"WARNING: excluding isolated load bus {b!r} (has load but no active branch connection)")

nodes = {n for n in nodes if n in connected}
loads = defaultdict(lambda: {"A": 0 + 0j, "B": 0 + 0j, "C": 0 + 0j}, {
    k: v for k, v in loads.items() if k in connected
})

# Preserve feeder order as much as possible from edge list, with Meter1 first if present
bus_map = {}
bus_list = []


def add_bus(name):
    if name not in bus_map:
        bus_map[name] = len(bus_map) + 1
        bus_list.append(name)


if "Meter1" in nodes:
    add_bus("Meter1")

for f, t in edges:
    add_bus(f)
    add_bus(t)

for n in sorted(nodes):
    add_bus(n)


# Read exact per-phase branch impedances from CSV
imp_map = {}
with open(CSVF, "r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        key = (row["from"], row["to"])
        imp_map[key] = {
            "A": (float(row["phaseA_R_pu"]), float(row["phaseA_X_pu"])),
            "B": (float(row["phaseB_R_pu"]), float(row["phaseB_X_pu"])),
            "C": (float(row["phaseC_R_pu"]), float(row["phaseC_X_pu"])),
        }


def get_impedance(fbus, tbus, phase):
    if (fbus, tbus) in imp_map:
        return imp_map[(fbus, tbus)][phase]
    if (tbus, fbus) in imp_map:
        return imp_map[(tbus, fbus)][phase]
    raise RuntimeError(f"Missing impedance for branch {fbus} <-> {tbus}")


# Write helpful debug files
with open("bus_map.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["bus_number", "bus_name"])
    for name in bus_list:
        w.writerow([bus_map[name], name])

with open("active_loads.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["bus_name", "phase", "P_MW", "Q_MVAr"])
    for name in bus_list:
        for ph in ["A", "B", "C"]:
            S = loads[name][ph] / 1e6  # VA -> MW/MVAr
            if abs(S) > 0:
                w.writerow([name, ph, S.real, S.imag])


def write_raw(phase):
    fname = f"IEEE13_phase_{phase}.raw"
    with open(fname, "w", encoding="utf-8") as f:
        f.write("0, 100.000\n")
        f.write(f"Auto-generated Phase {phase} (GLM + CSV exact branch impedances)\n")
        f.write("Created 2026-03-25\n")

        # Bus data
        for name in bus_list:
            num = bus_map[name]

            # RAW bus load fields are MW / MVAr, not pu
            S = loads[name][phase] / 1e6
            P = S.real
            Q = S.imag

            bustype = 3 if name == "Meter1" else 1
            f.write(
                f"{num:7d}, {bustype}, {P:13.8f}, {Q:13.8f},"
                f"    0.0000,    0.0000,   1,1.00000,   0.0000,"
                f"'{name:<12}',2.4018,   1\n"
            )

        f.write("0\n")

        # Generator/slack data
        slack = bus_map["Meter1"]
        f.write(
            f"{slack:7d},'1 ',     0.000,   0.000, 99990.000, -9999.000,"
            f"1.00000,     0,   100.000,   0.00000,   1.00000,   0.00000,"
            f"   0.00000,   1.00000,1, 100.0,     0.000,     0.000\n"
        )

        f.write("0 / END OF GENERATOR DATA, BEGIN BRANCH DATA\n")

        # Branch data
        for fbus, tbus in edges:
            fb = bus_map[fbus]
            tb = bus_map[tbus]
            R, X = get_impedance(fbus, tbus, phase)
            f.write(
                f"{fb:7d},{tb:7d},'BL', {R:12.8f}, {X:12.8f},  0.00000,"
                f"   0.00,   0.00,   0.00,1.00000,  0.000, 0.00000, 0.00000,"
                f" 0.00000, 0.00000, 1  / {fbus}-{tbus}\n"
            )

        f.write("0 / END OF BRANCH DATA, BEGIN TRANSFORMER DATA\n")
        f.write("0 / END OF TRANSFORMER DATA, BEGIN TRANSFORMER ADJUSTMENT DATA\n")
        f.write("0 / END OF TRANSFORMER ADJUSTMENT DATA, BEGIN AREA DATA\n")
        f.write("   1,      0,     0.0,  0.000,'            '\n")
        f.write("0 / END OF AREA DATA, BEGIN TWO-TERMINAL DC DATA\n")
        f.write("0 / END OF TWO-TERMINAL DC DATA, BEGIN SWITCHED SHUNT DATA\n")
        f.write("0 / END OF SWITCHED SHUNT DATA, BEGIN IMPEDANCE CORRECTION DATA\n")
        f.write("0 / END OF IMPEDANCE CORRECTION DATA, BEGIN MULTI-TERMINAL DC DATA\n")
        f.write("0 / END OF MULTI-TERMINAL DC DATA, BEGIN MULTI-SECTION LINE DATA\n")
        f.write("0 / END OF MULTI-SECTION LINE DATA, BEGIN ZONE DATA\n")
        f.write("    1,'ZONE_1      '\n")
        f.write("0 / END OF ZONE DATA, BEGIN INTER-AREA TRANSFER DATA\n")
        f.write("    1,    1,'1 ',    0.00\n")
        f.write("0 / END OF INTER-AREA TRANSFER DATA, BEGIN OWNER DATA\n")
        f.write("    1,'OWNER_1     '\n")
        f.write("0 / END OF OWNER DATA, BEGIN FACTS DEVICE DATA\n")
        f.write("0\n")

    print(f"Wrote {fname}")


for ph in ["A", "B", "C"]:
    write_raw(ph)

print("Wrote bus_map.csv")
print("Wrote active_loads.csv")
