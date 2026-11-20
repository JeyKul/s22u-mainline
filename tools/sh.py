#!/usr/bin/env python3
"""Run one shell command on the phone over the USB ACM serial shell.
Usage: tools/sh.py 'command' [timeout_s]"""
import re
import sys
import time

import serial

DEV = "/dev/ttyACM0"
CPR_QUERY = b"\x1b[6n"  # busybox line editing asks for the cursor position


def talk(s, data, until, timeout):
    s.write(data)
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        chunk = s.read(65536)
        if chunk:
            if CPR_QUERY in chunk:
                s.write(b"\x1b[1;1R")
                chunk = chunk.replace(CPR_QUERY, b"")
            buf += chunk
        m = re.search(until, buf)
        if m:
            return buf, m
    return buf, None


cmd = sys.argv[1]
timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 30
tag = f"__B0S_{int(time.time() * 1000)}__"

with serial.Serial(DEV, 115200, timeout=0.1) as s:
    # get a clean prompt: cancel any half-typed line, turn echo off
    talk(s, b"\x03\n", rb"# ", 3)
    talk(s, b"stty -echo; PS1='# '\n", rb"# ", 3)
    s.reset_input_buffer()
    buf, m = talk(s, f"{cmd}; echo {tag}$?\n".encode(), rf"{tag}(\d+)".encode(), timeout)
    if not m:
        sys.stdout.write(buf.decode(errors="replace"))
        sys.exit("timeout")
    out = buf[: m.start()].decode(errors="replace").replace("\r\n", "\n")
    sys.stdout.write(out)
    sys.exit(int(m.group(1)))
