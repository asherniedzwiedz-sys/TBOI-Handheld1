#!/usr/bin/env python3
"""CPU stress: 8 enemies + triple shot + enemy bullets, counts lag frames.

A lag frame is a vblank where the game logic was not finished (the NES
runs at ~29,780 CPU cycles per frame). Prints the lag percentage.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Game  # noqa: E402
from smoke_test import fmap, neighbor, walk_through  # noqa: E402


def run(floor=5, frames=900, triple=True):
    g = Game()
    g.idle(33)
    g.tap("start", hold=2, after=30)
    g.poke("floor_num", floor)
    fm = fmap(g)
    cur = g.peek("cur")
    for d in range(4):
        n = neighbor(cur, d)
        if n is not None and fm[n] & 7 == 1:
            walk_through(g, d)
            break
    # fill all 8 enemy slots: bats, gazers, slimes, a beetle
    types = [1, 1, 3, 3, 2, 2, 4, 1]
    for e, t in enumerate(types):
        g.poke("en_type", t, e)
        g.poke("en_hp", 200, e)
        g.poke16("en_x", (40 + e * 22) << 8, e * 2)
        g.poke16("en_y", (70 + (e % 3) * 40) << 8, e * 2)
        g.poke("en_t", 5 + e, e)
    g.poke("en_count", 8)
    if triple:
        g.poke("st_flags", 1)
        g.poke("st_delay", 8)
    lag0 = g.peek("lag_cnt")
    lag = 0
    seq = ["up", "down"]
    for i in range(frames):
        g.poke("pl_inv", 30)
        g.poke("pl_hp", 6)
        g.press("A", seq[(i // 30) % 2])
        cur_lag = g.peek("lag_cnt")
        lag += (cur_lag - lag0) & 0xFF
        lag0 = cur_lag
    return lag, frames, g


if __name__ == "__main__":
    lag, frames, g = run()
    print(f"stress: {lag} lag frames in {frames} ({100.0 * lag / frames:.1f}%)")
    if len(sys.argv) > 1:
        g.shot(sys.argv[1])
