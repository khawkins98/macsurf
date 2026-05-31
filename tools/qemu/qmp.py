#!/usr/bin/env python3
"""Minimal QMP (QEMU Machine Protocol) client for the MacSurf build harness.

Connects to a running qemu-system-ppc QMP unix socket and runs one command,
printing the JSON reply. Skips async events and waits for the matching
return/error. Used by the launch/automation scripts to screendump, snapshot,
send keys, and power the guest down unattended.

Usage:
    qmp.py <sock> <command> [key=value ...]
    qmp.py <sock> screendump filename=/tmp/shot.png format=png
    qmp.py <sock> query-status
    qmp.py <sock> system_powerdown
    qmp.py <sock> stop
    qmp.py <sock> cont

value parsing: ints/true/false/null are JSON-coerced; everything else is a string.
"""
import json
import socket
import sys
import time


def connect(path, timeout=60.0):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    deadline = time.time() + timeout
    while True:
        try:
            s.connect(path)
            break
        except (FileNotFoundError, ConnectionRefusedError, OSError):
            if time.time() > deadline:
                raise
            time.sleep(0.5)
    f = s.makefile("rw", encoding="utf-8", newline="\n")
    f.readline()  # QMP greeting
    f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n")
    f.flush()
    f.readline()  # capabilities ack
    return s, f


def run(f, execute, arguments=None):
    msg = {"execute": execute}
    if arguments:
        msg["arguments"] = arguments
    f.write(json.dumps(msg) + "\n")
    f.flush()
    while True:
        line = f.readline()
        if not line:
            return None
        obj = json.loads(line)
        if "return" in obj or "error" in obj:
            return obj
        # otherwise an async event; keep reading


def coerce(v):
    low = v.lower()
    if low in ("true", "false"):
        return low == "true"
    if low == "null":
        return None
    try:
        return int(v)
    except ValueError:
        return v


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    sock, command = argv[1], argv[2]
    args = {}
    for kv in argv[3:]:
        if "=" not in kv:
            print("bad arg (need key=value): %s" % kv, file=sys.stderr)
            return 2
        k, v = kv.split("=", 1)
        args[k] = coerce(v)
    _, f = connect(sock)
    reply = run(f, command, args or None)
    print(json.dumps(reply, indent=2))
    return 0 if reply and "error" not in reply else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
