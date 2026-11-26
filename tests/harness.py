"""Headless test harness for build/undercroft.nes using the nes-py emulator.

Gives scripted controller input, reads game RAM by symbol name (from the
ld65 label file) and saves screenshots.
"""
import os
import re
import warnings

import numpy as np
from PIL import Image

warnings.filterwarnings("ignore")
from nes_py.nes_env import NESEnv  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROM = os.path.join(ROOT, "build", "undercroft.nes")
LABELS = os.path.join(ROOT, "build", "undercroft.labels")

# nes-py controller bits
BTN = {"right": 0x80, "left": 0x40, "down": 0x20, "up": 0x10,
       "start": 0x08, "select": 0x04, "B": 0x02, "A": 0x01}


def load_labels(path=LABELS):
    syms = {}
    for line in open(path):
        m = re.match(r"al ([0-9A-F]+) \._?(\w+)", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    return syms


class Game:
    def __init__(self, rom=ROM):
        self.env = NESEnv(rom)
        self.env.reset()
        self.sym = load_labels()
        self.frames = 0

    def press(self, *buttons, frames=1):
        action = 0
        for b in buttons:
            action |= BTN[b]
        for _ in range(frames):
            self.env.step(action)
            self.frames += 1

    def idle(self, frames=1):
        self.press(frames=frames)

    def tap(self, *buttons, hold=2, after=2):
        self.press(*buttons, frames=hold)
        self.idle(after)

    def addr(self, name):
        return self.sym[name]

    def peek(self, name, offset=0):
        return int(self.env.ram[self.sym[name] + offset])

    def peek16(self, name, offset=0):
        a = self.sym[name] + offset
        return int(self.env.ram[a]) | (int(self.env.ram[a + 1]) << 8)

    def poke(self, name, value, offset=0):
        self.env.ram[self.sym[name] + offset] = value & 0xFF

    def poke16(self, name, value, offset=0):
        a = self.sym[name] + offset
        self.env.ram[a] = value & 0xFF
        self.env.ram[a + 1] = (value >> 8) & 0xFF

    def screen(self):
        return np.array(self.env.screen, copy=True)

    def shot(self, path, scale=2):
        img = Image.fromarray(self.screen())
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        img.save(path)
        return path
