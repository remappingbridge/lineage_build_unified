#!/usr/bin/env python3
"""Check the emitted HID wire layout and Linux's 128-main-field budget."""
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
offset = 0
glob = {"size": 0, "count": 0, "id": 0, "page": 0}
stack = []
bits = {}
fields = {}
while offset < len(data):
    tag = data[offset]
    offset += 1
    assert tag != 0xfe, "unexpected long item"
    length = [0, 1, 2, 4][tag & 3]
    assert offset + length <= len(data)
    value = int.from_bytes(data[offset:offset + length], "little")
    offset += length
    key = tag & 0xfc
    if key in (0x74, 0x94, 0x84, 0x04):
        glob[{0x74: "size", 0x94: "count", 0x84: "id", 0x04: "page"}[key]] = value
    elif key == 0xa4:
        stack.append(glob.copy())
    elif key == 0xb4:
        glob = stack.pop()
    elif key in (0x80, 0xb0):
        report = key, glob["id"]
        bits[report] = bits.get(report, 0) + glob["size"] * glob["count"]
        fields[report] = fields.get(report, 0) + 1
assert not stack
assert bits == {(0x80, 1): 178 * 8, (0xb0, 2): 8}, bits
assert fields[(0x80, 1)] == 115 < 128, fields
assert len(data) <= 4096
print("PASS: HID descriptor report sizes, global stack, 115/128 Linux input fields")
