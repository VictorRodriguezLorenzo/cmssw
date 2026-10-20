
from __future__ import print_function

import argparse
import math
import sys
import ROOT
from DataFormats.FWLite import Events, Handle, Runs, Lumis

parser = argparse.ArgumentParser()
parser.add_argument("infile", help="Input EDM file")
parser.add_argument("--source", choices=["genWeights", "lheWeights"], default="genWeights")
parser.add_argument("--require-groups", action="store_true", help="Fail if no weight groups are found")
args = parser.parse_args()

ROOT.gSystem.Load("libFWCoreFWLite")
ROOT.FWLiteEnabler.enable()
ROOT.gSystem.Load("libSimDataFormatsGeneratorProducts")

errors = 0

def fail(message):
    global errors
    errors += 1
    print("[FAIL]", message)

# GEN metadata is stored per lumi; LHE metadata is stored per run.
collection = Lumis(args.infile) if args.source == "genWeights" else Runs(args.infile)
try:
    block = next(iter(collection))
except StopIteration:
    sys.exit("[FAIL] No runs/luminosity blocks found")

infoHandle = Handle("GenWeightInfoProduct")
block.getByLabel(args.source, infoHandle)
if not infoHandle.isValid():
    sys.exit("[FAIL] Missing GenWeightInfoProduct from " + args.source)

infoProduct = infoHandle.product()
nGroups = infoProduct.numberOfGroups()
print("Source:", args.source)
print("Number of weight groups:", nGroups)

if args.require_groups and nGroups == 0:
    fail("No weight groups found")

events = Events(args.infile)
try:
    event = next(iter(events))
except StopIteration:
    sys.exit("[FAIL] No events found")

weightHandle = Handle("GenWeightProduct")
event.getByLabel(args.source, weightHandle)
if not weightHandle.isValid():
    sys.exit("[FAIL] Missing GenWeightProduct from " + args.source)

weightProduct = weightHandle.product()
weightGroups = weightProduct.weights()

print("Central weight:", weightProduct.centralWeight())
print("Number of event weight groups:", len(weightGroups))

if len(weightGroups) != nGroups:
    fail("Event group count does not match metadata group count")

for j, weights in enumerate(weightGroups):
    if j >= nGroups:
        fail("Unexpected weight group index {}".format(j))
        continue

    group = infoProduct.orderedWeightGroupInfo(j)
    nIds = group.nIdsContained()

    print("\n--- Group {} ---".format(j))
    print("Name:", group.name())
    print("Type:", group.weightType())
    print("Well formed:", group.isWellFormed())
    print("Description:", group.description())
    print("Stored weights:", len(weights))
    print("Metadata entries:", nIds)

    if nIds > 0 and len(weights) > nIds:
        fail("Group {} has more weights than metadata entries".format(j))

    for i, weight in enumerate(weights):
        if math.isnan(weight) or math.isinf(weight):
            fail("Non-finite weight in group {}, index {}".format(j, i))

        if i >= nIds:
            print("  [{}] weight={} (no metadata entry)".format(i, weight))
            continue

        try:
            meta = group.weightMetaInfo(i)
            print("  [{}] value={} id={} globalIndex={} label={}".format(
                i, weight, meta.id, meta.globalIndex, meta.label
            ))
            if meta.localIndex != i:
                fail("Incorrect local index in group {}, entry {}".format(j, i))
        except Exception as e:
            fail("Cannot read metadata for group {}, entry {}: {}".format(j, i, e))

print("\nResult: {} error(s)".format(errors))
sys.exit(1 if errors else 0)

