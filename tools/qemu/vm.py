#!/usr/bin/env python3
"""High-level driver harness for the MacSurf OS 9 QEMU VM.

Turns raw QMP into ergonomic verbs so an operator (human or agent) can drive
the guest turn-by-turn:

    screenshot      grab the framebuffer to PNG (any display backend)
    key             send a key chord (e.g. "meta_l-b" for Cmd-B / Make)
    type            type a literal string (handles shift for caps/symbols)
    move / click    absolute pointer move + click  (REQUIRES usb-tablet; see note)
    settle          wait until the screen stops changing (dialog appeared/render done)
    snapshot/revert qcow2 internal snapshot save/load via the monitor
    powerdown/quit  graceful guest shutdown / hard QEMU exit

ABSOLUTE POINTER NOTE: move/click emit absolute input events, which only land
if the VM exposes an *absolute* pointing device (`-device usb-tablet`). The
default `-device usb-mouse` is RELATIVE and ignores absolute coords. OS 9's HID
support for usb-tablet is historically flaky — run `vm.py probe-tablet` once to
find out whether THIS guest tracks it. Keyboard verbs always work.

Usage:
    vm.py [--sock PATH] [--w 1024] [--h 768] <verb> [args...]

Examples:
    vm.py screenshot /tmp/s.png
    vm.py key meta_l-q
    vm.py type "Dev machine"
    vm.py click 512 384
    vm.py settle
    vm.py snapshot os9_cw_ready
    vm.py revert os9_cw_ready
"""
import json
import os
import socket
import sys
import time

DEFAULT_SOCK = os.path.expanduser("~/macsurf-qemu/qmp.sock")

# --- char -> (qcode, needs_shift) for type() -------------------------------
_LOWER = "abcdefghijklmnopqrstuvwxyz"
_DIGIT = "0123456789"
KEYMAP = {}
for c in _LOWER:
    KEYMAP[c] = (c, False)
    KEYMAP[c.upper()] = (c, True)
for d in _DIGIT:
    KEYMAP[d] = (d, False)
KEYMAP[" "] = ("spc", False)
KEYMAP["\n"] = ("ret", False)
KEYMAP["\t"] = ("tab", False)
# unshifted punctuation -> qcode
_PUNCT = {
    "-": "minus", "=": "equal", "[": "bracket_left", "]": "bracket_right",
    "\\": "backslash", ";": "semicolon", "'": "apostrophe", "`": "grave_accent",
    ",": "comma", ".": "dot", "/": "slash",
}
for ch, qc in _PUNCT.items():
    KEYMAP[ch] = (qc, False)
# shifted symbols -> (base qcode, shift)
_SHIFTED = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
    "*": "8", "(": "9", ")": "0", "_": "minus", "+": "equal", "{": "bracket_left",
    "}": "bracket_right", "|": "backslash", ":": "semicolon", '"': "apostrophe",
    "~": "grave_accent", "<": "comma", ">": "dot", "?": "slash",
}
for ch, qc in _SHIFTED.items():
    KEYMAP[ch] = (qc, True)


class VM:
    def __init__(self, sock=DEFAULT_SOCK, w=1024, h=768):
        self.sock_path = sock
        self.w = w
        self.h = h
        self._s = None
        self._f = None

    def connect(self, timeout=60.0):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.time() + timeout
        while True:
            try:
                s.connect(self.sock_path)
                break
            except (FileNotFoundError, ConnectionRefusedError, OSError):
                if time.time() > deadline:
                    raise
                time.sleep(0.4)
        self._s = s
        self._f = s.makefile("rw", encoding="utf-8", newline="\n")
        self._f.readline()  # greeting
        self._cmd("qmp_capabilities")
        return self

    def _cmd(self, execute, arguments=None):
        msg = {"execute": execute}
        if arguments:
            msg["arguments"] = arguments
        self._f.write(json.dumps(msg) + "\n")
        self._f.flush()
        while True:
            line = self._f.readline()
            if not line:
                return None
            obj = json.loads(line)
            if "return" in obj or "error" in obj:
                if "error" in obj:
                    raise RuntimeError(obj["error"])
                return obj["return"]

    def hmp(self, command_line):
        """Run a human-monitor command via QMP."""
        return self._cmd("human-monitor-command", {"command-line": command_line})

    # --- verbs --------------------------------------------------------------
    def screenshot(self, path):
        self._cmd("screendump", {"filename": path, "format": "png"})
        return path

    def status(self):
        return self._cmd("query-status")

    def key(self, chord):
        """chord like 'meta_l-b' or 'ctrl-alt-delete' -> simultaneous press."""
        keys = [{"type": "qcode", "data": k} for k in chord.split("-")]
        self._cmd("send-key", {"keys": keys})

    def type(self, text):
        for ch in text:
            if ch not in KEYMAP:
                continue
            qc, shift = KEYMAP[ch]
            keys = [{"type": "qcode", "data": qc}]
            if shift:
                keys = [{"type": "qcode", "data": "shift"}] + keys
            self._cmd("send-key", {"keys": keys})
            time.sleep(0.02)

    def _abs(self, px, py):
        ax = int(px * 0x7FFF / self.w)
        ay = int(py * 0x7FFF / self.h)
        self._cmd("input-send-event", {"events": [
            {"type": "abs", "data": {"axis": "x", "value": ax}},
            {"type": "abs", "data": {"axis": "y", "value": ay}},
        ]})

    def rel(self, dx, dy, step=0):
        """Relative pointer motion (for usb-mouse). step>0 splits into N-px
        increments in one batch to reduce OS 9 acceleration overshoot."""
        evs = []
        if step and step > 0:
            def axis_steps(total, axis):
                n = abs(total); s = 1 if total >= 0 else -1
                while n > 0:
                    d = min(step, n) * s
                    evs.append({"type": "rel", "data": {"axis": axis, "value": d}})
                    n -= step
            axis_steps(dx, "x"); axis_steps(dy, "y")
        else:
            if dx:
                evs.append({"type": "rel", "data": {"axis": "x", "value": dx}})
            if dy:
                evs.append({"type": "rel", "data": {"axis": "y", "value": dy}})
        if evs:
            self._cmd("input-send-event", {"events": evs})

    def press(self, button="left", double=False, dwell=0.15):
        """Click at the CURRENT cursor position (no move). `dwell` must exceed one
        OS 9 HID poll interval (~16ms VBL) or the polled driver misses the press;
        real cocoa clicks 'dwell' for tens of ms, which is why instant down+up fails."""
        def p(d):
            self._cmd("input-send-event", {"events": [
                {"type": "btn", "data": {"button": button, "down": d}}]})
        p(True); time.sleep(dwell); p(False)
        if double:
            time.sleep(0.12); p(True); time.sleep(dwell); p(False)

    def move(self, px, py):
        self._abs(px, py)

    def click(self, px, py, button="left", double=False):
        self._abs(px, py)
        time.sleep(0.05)
        def press(down):
            self._cmd("input-send-event", {"events": [
                {"type": "btn", "data": {"button": button, "down": down}}]})
        press(True); time.sleep(0.05); press(False)
        if double:
            time.sleep(0.08); press(True); time.sleep(0.05); press(False)

    def settle(self, tmp="/tmp/_vm_settle.png", interval=2.0, stable=2, timeout=120):
        """Block until two consecutive screenshots are identical (screen settled)."""
        last = None; same = 0; end = time.time() + timeout
        while time.time() < end:
            self.screenshot(tmp)
            data = open(tmp, "rb").read()
            if data == last:
                same += 1
                if same >= stable:
                    return True
            else:
                same = 0; last = data
            time.sleep(interval)
        return False

    def snapshot(self, tag):
        return self.hmp("savevm %s" % tag)

    def revert(self, tag):
        return self.hmp("loadvm %s" % tag)

    def snapshots(self):
        return self.hmp("info snapshots")

    def powerdown(self):
        return self._cmd("system_powerdown")

    def quit(self):
        try:
            return self._cmd("quit")
        except Exception:
            return None


def main(argv):
    sock = DEFAULT_SOCK; w = 1024; h = 768
    args = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--sock":
            sock = argv[i + 1]; i += 2
        elif a == "--w":
            w = int(argv[i + 1]); i += 2
        elif a == "--h":
            h = int(argv[i + 1]); i += 2
        else:
            args.append(a); i += 1
    if not args:
        print(__doc__); return 2
    verb, rest = args[0], args[1:]
    vm = VM(sock, w, h).connect()
    if verb == "screenshot":
        print(vm.screenshot(rest[0] if rest else "/tmp/shot.png"))
    elif verb == "status":
        print(json.dumps(vm.status()))
    elif verb == "key":
        vm.key(rest[0])
    elif verb == "type":
        vm.type(rest[0])
    elif verb == "move":
        vm.move(int(rest[0]), int(rest[1]))
    elif verb == "rmove":
        step = int(rest[2]) if len(rest) > 2 else 0
        vm.rel(int(rest[0]), int(rest[1]), step=step)
    elif verb == "press":
        vm.press(double=("--double" in rest))
    elif verb == "click":
        vm.click(int(rest[0]), int(rest[1]), double=("--double" in rest))
    elif verb == "settle":
        print("settled" if vm.settle() else "timeout")
    elif verb == "snapshot":
        print(vm.snapshot(rest[0]))
    elif verb == "revert":
        print(vm.revert(rest[0]))
    elif verb == "snapshots":
        print(vm.snapshots())
    elif verb == "powerdown":
        vm.powerdown()
    elif verb == "quit":
        vm.quit()
    elif verb == "probe-tablet":
        # move to center then a corner; caller screenshots to see if cursor tracked.
        vm.move(w // 2, h // 2); time.sleep(0.4); vm.move(40, 40)
        print("moved cursor center->(40,40); screenshot to verify it tracked")
    else:
        print("unknown verb: %s" % verb); return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
