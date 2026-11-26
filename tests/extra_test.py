#!/usr/bin/env python3
"""More checks: floor generation over many seeds, the item room, and winning.

Run:  python3 tests/extra_test.py [screenshot_dir]
"""
import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Game  # noqa: E402
from smoke_test import (check, failures, fmap, neighbor, walk_through,  # noqa: E402
                        open_door_to, fight, teleport, shot)

OUT = sys.argv[1] if len(sys.argv) > 1 else None


def reachable(fm, start):
    seen = {start}
    q = deque([start])
    while q:
        c = q.popleft()
        for d in range(4):
            n = neighbor(c, d)
            if n is not None and fm[n] & 7 and n not in seen:
                seen.add(n)
                q.append(n)
    return seen


def seeds():
    bad = 0
    for k in range(25):
        g = Game()
        g.idle(20 + k * 7)
        g.tap("start", hold=2, after=30)
        fm = fmap(g)
        rooms = [i for i, v in enumerate(fm) if v & 7]
        ok = (len(rooms) == 7
              and sum(1 for i in rooms if fm[i] & 7 == 3) == 1
              and sum(1 for i in rooms if fm[i] & 7 == 4) == 1
              and set(rooms) == reachable(fm, 40)
              and g.peek("cur") == 40)
        boss = next(i for i in rooms if fm[i] & 7 == 3)
        item = next(i for i in rooms if fm[i] & 7 == 4)
        # boss and item rooms are dead ends (one neighbour)
        for special in (boss, item):
            nbs = [neighbor(special, d) for d in range(4)]
            if sum(1 for n in nbs if n is not None and fm[n] & 7) != 1:
                ok = False
        if not ok:
            bad += 1
            print("bad floor for seed", k, fm)
    check(bad == 0, "25 random floors are valid (7 rooms, 1 boss, 1 item, all connected)")


def item_room():
    g = Game()
    g.idle(77)
    g.tap("start", hold=2, after=30)
    fm = fmap(g)
    item = next(i for i, v in enumerate(fm) if v & 7 == 4)
    for d in range(4):
        nb = neighbor(item, d ^ 1)
        if nb is not None and fm[nb] & 7:
            break
    g.poke("cur", nb)
    open_door_to(g, d)
    check(walk_through(g, d), "entered the item room")
    g.idle(5)
    shot(g, "20_item_room") if OUT else None
    check(g.peek("en_count") == 0, "item room has no enemies")
    before = [g.peek(n) for n in ("st_dmg", "st_delay", "st_range", "pl_maxhp", "st_flags", "pl_bombs")]
    before.append(g.peek16("st_speed"))
    teleport(g, 112, 120)
    g.idle(5)
    after = [g.peek(n) for n in ("st_dmg", "st_delay", "st_range", "pl_maxhp", "st_flags", "pl_bombs")]
    after.append(g.peek16("st_speed"))
    check(before != after, "item room item changes stats")
    check(g.peek("fextra", item) == 0, "item is gone once taken")
    shot(g, "21_item_taken") if OUT else None


def win():
    g = Game()
    g.idle(50)
    g.tap("start", hold=2, after=30)
    g.poke("floor_num", 5)
    fm = fmap(g)
    boss = next(i for i, v in enumerate(fm) if v & 7 == 3)
    for d in range(4):
        nb = neighbor(boss, d ^ 1)
        if nb is not None and fm[nb] & 7:
            break
    g.poke("cur", nb)
    open_door_to(g, d)
    g.poke("pl_inv", 60)
    check(walk_through(g, d), "entered the floor 5 boss room")
    g.idle(45)
    check(g.peek("en_hp", g.peek("boss_slot")) == 150, "floor 5 boss has 150 HP")
    shot(g, "22_final_boss") if OUT else None
    frames, lag = fight(g, max_frames=8000)
    print(f"lag: final boss (base stats): {lag} lag frames in {frames} frames")
    check(g.peek("en_count") == 0, f"final boss defeated in {frames} frames")
    g.idle(20)
    teleport(g, 112, 152)
    g.idle(30)
    check(g.peek("game_result") == 2, "trapdoor on floor 5 wins the game")
    g.idle(60)
    shot(g, "23_win") if OUT else None


if __name__ == "__main__":
    if OUT:
        import smoke_test
        smoke_test.OUT = OUT
    seeds()
    item_room()
    win()
    print()
    if failures:
        print(f"{len(failures)} FAILED")
        sys.exit(1)
    print("ALL PASSED")
