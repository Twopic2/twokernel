#!/usr/bin/env python3
"""Wrap TwoKernel's debug-console JSONL capture as a Perfetto JSON trace."""

import argparse
import json
from pathlib import Path
import sys
import tempfile


def validate(event, line_number):
    if not isinstance(event, dict) or event.get("ph") not in ("C", "X", "M"):
        raise ValueError(f"line {line_number}: expected a counter, interval, or metadata event")
    if not isinstance(event.get("name"), str):
        raise ValueError(f"line {line_number}: missing event name")
    fields = ["pid"]
    if event["ph"] != "M":
        fields += ["tid", "ts"]
    if event["ph"] == "X":
        fields.append("dur")
    for field in fields:
        if type(event.get(field)) is not int or event[field] < 0:
            raise ValueError(f"line {line_number}: {field} must be a nonnegative integer")
    if event["ph"] in ("C", "M") and not isinstance(event.get("args"), dict):
        raise ValueError(f"line {line_number}: missing event arguments")


def convert(source, destination):
    if source.resolve() == destination.resolve():
        raise ValueError("input and output must be different files")

    count = dropped = 0
    incomplete = False
    temporary = None
    try:
        with source.open("rb") as capture:
            with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=destination.parent,
                prefix=f".{destination.name}.", delete=False,
            ) as output:
                temporary = Path(output.name)
                output.write('{"traceEvents":[')
                for line_number, line in enumerate(capture, 1):
                    if not line.strip():
                        continue
                    try:
                        event = json.loads(line)
                    except (json.JSONDecodeError, UnicodeDecodeError) as error:
                        if not line.endswith(b"\n"):
                            incomplete = True
                            break
                        raise ValueError(f"line {line_number}: malformed JSON: {error}") from error
                    validate(event, line_number)
                    if count:
                        output.write(",\n")
                    json.dump(event, output, ensure_ascii=True, separators=(",", ":"))
                    count += 1
                    if event["ph"] == "C" and event["name"] == "Trace dropped events":
                        value = event["args"].get("value")
                        if type(value) is int:
                            dropped = max(dropped, value)
                output.write("]}\n")
        if count == 0:
            raise ValueError("capture contains no complete trace events")
        temporary.replace(destination)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return count, incomplete, dropped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="JSONL file written by QEMU -debugcon")
    parser.add_argument("output", nargs="?", type=Path, help="output JSON (defaults to capture.json)")
    args = parser.parse_args()
    destination = args.output or args.capture.with_suffix(".json")
    try:
        count, incomplete, dropped = convert(args.capture, destination)
    except (OSError, ValueError) as error:
        parser.exit(1, f"trace conversion failed: {error}\n")
    if incomplete:
        print("warning: ignored an incomplete final record", file=sys.stderr)
    if dropped:
        print(f"warning: trace reports {dropped} dropped events", file=sys.stderr)
    print(f"Wrote {count} events to {destination}; open it at https://ui.perfetto.dev")


if __name__ == "__main__":
    main()
