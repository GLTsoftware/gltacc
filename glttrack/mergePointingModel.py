#!/usr/bin/env python3
"""Merge dated pointingModel-YYMMDD-OpticalRadio-<freq> files into one
5-column pointingModel: Parameter Optical 86 230 86+230 345.
Optical comes from the newest file overall; each radio column from the newest
file for that frequency. 86+230 = mean of the 86 and 230 columns (placeholder
until measured). Usage: mergePointingModel.py [outfile]  (default pointingModel.new)
"""
import glob, re, sys
files = {}
for f in glob.glob("pointingModel-??????-OpticalRadio-*"):
    m = re.fullmatch(r"pointingModel-(\d{6})-OpticalRadio-(86|230|345)", f)
    if m: files[f] = (m.group(1), m.group(2))
def key(d):  # YYMMDD -> sortable YYYYMMDD
    return ("20" if int(d[:2]) < 80 else "19") + d
def load(f):
    rows, order = {}, []
    for ln in open(f):
        if ln.startswith("#") or not ln.split(): continue
        t = ln.split(); rows[t[0]] = t[1:3]; order.append(t[0])
    return rows, order
newest = {fr: max((f for f in files if files[f][1] == fr), key=lambda f: key(files[f][0])) for fr in ("86","230","345")}
newest_all = max(files, key=lambda f: key(files[f][0]))
data = {fr: load(f)[0] for fr, f in newest.items()}
opt, order = load(newest_all)
w = lambda *a: " ".join(f"{x:>11}" for x in a)
out = ["# Pointing Model Parameters for Antenna-1", "# All values are in arcseconds.",
       f"# Optical from {newest_all}",
       *[f"# {fr} GHz from {newest[fr]}" for fr in newest],
       "# 86+230 column = mean of 86 and 230 (placeholder; replace when measured).",
       "# Parameter".ljust(14) + w("Optical","86","230","86+230","345")]
for p in order:
    o = opt[p][0]; v = {fr: data[fr][p][1] for fr in data}
    try: mid = f"{(float(v['86'])+float(v['230']))/2:.3f}"
    except ValueError: mid = v["230"]   # Date / non-numeric rows
    if p == "TiltFlag": mid = v["230"]
    out.append(p.ljust(14) + w(o, v["86"], v["230"], mid, v["345"]))
open(sys.argv[1] if len(sys.argv) > 1 else "pointingModel.new", "w").write("\n".join(out) + "\n")
