#!/usr/bin/env python3
"""Generate the ROM-backed evolution-condition registry from species-info source data."""

import json
import re
import sys
from pathlib import Path


CONDITIONS_RE = re.compile(r"CONDITIONS\((.*?)\)", re.DOTALL)
CONDITION_RE = re.compile(r"\{\s*(IF_[A-Z0-9_]+)\s*(?:,\s*([^,{}]+?))?\s*(?:,\s*([^,{}]+?))?\s*(?:,\s*([^,{}]+?))?\s*\}")


def normalize_conditions(contents):
    conditions = []
    for match in CONDITION_RE.finditer(contents):
        values = [value.strip() if value else "0" for value in match.groups()]
        conditions.append(values)
    if not conditions:
        raise ValueError(f"Could not parse CONDITIONS({contents})")
    return tuple(tuple(condition) for condition in conditions)


def signature(conditions):
    return "|".join(",".join(condition) for condition in conditions)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: make_condition_sets.py <species-info-dir> <manifest.json> <output.h>")

    source_dir = Path(sys.argv[1])
    manifest_path = Path(sys.argv[2])
    output_path = Path(sys.argv[3])
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"sets": []}
    known = {entry["signature"]: entry for entry in manifest["sets"]}
    discovered = {}

    for source_path in sorted(source_dir.glob("gen_*_families.h")):
        for match in CONDITIONS_RE.finditer(source_path.read_text()):
            conditions = normalize_conditions(match.group(1))
            discovered[signature(conditions)] = conditions

    for condition_signature in sorted(discovered):
        if condition_signature not in known:
            known[condition_signature] = {
                "id": len(manifest["sets"]) + 1,
                "signature": condition_signature,
            }
            manifest["sets"].append(known[condition_signature])

    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

    entries = sorted(manifest["sets"], key=lambda entry: entry["id"])
    lines = [
        "#ifndef GUARD_EVOLUTION_CONDITION_SETS_H",
        "#define GUARD_EVOLUTION_CONDITION_SETS_H",
        "",
        "static const struct EvolutionParam *const gEvolutionConditionSets[] =",
        "{",
        "    NULL,",
    ]
    for entry in entries:
        conditions = discovered.get(entry["signature"])
        if conditions is None:
            raise ValueError(f"Manifest ID {entry['id']} is absent from current source data")
        args = ", ".join("{ " + ", ".join(condition) + " }" for condition in conditions)
        lines.append(f"    (const struct EvolutionParam[]) {{ {args}, {{ CONDITIONS_END }} }},")
    lines.extend(["};", "", "#endif // GUARD_EVOLUTION_CONDITION_SETS_H", ""])

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines))


if __name__ == "__main__":
    main()