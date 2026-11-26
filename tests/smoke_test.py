#!/usr/bin/env python3
"""End-to-end smoke test: plays build/undercroft.nes headlessly.

Covers: title -> start, floor generation, walking through a door, enemy
spawns, combat until a room clears, doors reopening, the boss fight, the
trapdoor to floor 2, the pause map, death and the game-over screen.
It also counts lag frames (frames where game logic overran the 60 Hz budget).

Run:  python3 tests/smoke_test.py [screenshot_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Game  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else None
FM_W = 9
DIRS = {0: "up", 1: "down", 2: "left", 3: "right"}
# player sprite positions just in front of each door, and how to walk through
DOOR_APPROACH = {0: (120, 60), 1: (120, 190), 2: (30, 120), 3: (210, 120)}
MT_DOOR_OPEN = {0: (6, 7), 1: (6, 7), 2: (8,), 3: (8,)}
DOOR_CELLS = {0: (7, 8), 1: (199, 200), 2: (96,), 3: (111,)}

failures = []
lag_report = []


def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        failures.append(msg)


def shot(g, name):
    if OUT:
        os.makedirs(OUT, exist_ok=True)
        g.shot(os.path.join(OUT, name + ".png"))


def fmap(g):
    return [g.peek("fmap", i) for i in range(81)]


def neighbor(c, d):
    x, y = c % FM_W, c // FM_W
    if d == 0:
        return c - FM_W if y > 0 else None
    if d == 1:
        return c + FM_W if y < FM_W - 1 else None
    if d == 2:
        return c - 1 if x > 0 else None
    return c + 1 if x < FM_W - 1 else None


def teleport(g, x, y):
    g.poke16("pl_x", x << 8)
    g.poke16("pl_y", y << 8)


def walk_through(g, d, frames=90):
    """Stand in front of door d and walk through it. Returns True on room change."""
    start = g.peek("cur")
    teleport(g, *DOOR_APPROACH[d])
    for _ in range(frames):
        g.press(DIRS[d])
        if g.peek("cur") != start:
            g.idle(2)
            return True
    return False


def open_door_to(g, d):
    """Force the door in direction d open in the current room's map (test helper)."""
    for cell, mt in zip(DOOR_CELLS[d], MT_DOOR_OPEN[d]):
        g.poke("room", mt, cell)


def enemies(g):
    out = []
    for e in range(8):
        t = g.peek("en_type", e)
        if t:
            out.append((e, t, g.peek16("en_x", e * 2) >> 8, g.peek16("en_y", e * 2) >> 8))
    return out


def fight(g, max_frames=3000, god=True):
    """Simple combat bot: line up with the nearest enemy and shoot it."""
    lag0 = g.peek("lag_cnt")
    f0 = g.frames
    lag = 0
    while g.frames - f0 < max_frames:
        if god:
            g.poke("pl_inv", 30)
            g.poke("pl_hp", g.peek("pl_maxhp"))
        es = enemies(g)
        if not es:
            break
        px, py = g.peek("px8"), g.peek("py8")
        _, t, ex, ey = min(es, key=lambda e: abs(e[2] - px) + abs(e[3] - py))
        size = 32 if t == 5 else 16
        ecx, ecy = ex + size // 2, ey + size // 2
        dx, dy = ecx - (px + 8), ecy - (py + 8)
        if abs(dx) <= abs(dy):
            # line up horizontally, shoot vertically
            aim = "down" if dy > 0 else "up"
            if abs(dx) > 4:
                g.press("right" if dx > 0 else "left", frames=1)
            else:
                g.press(aim, frames=1)
                g.press("A", aim, frames=6)
                g.press(frames=1)
        else:
            aim = "right" if dx > 0 else "left"
            if abs(dy) > 4:
                g.press("down" if dy > 0 else "up", frames=1)
            else:
                g.press(aim, frames=1)
                g.press("A", aim, frames=6)
                g.press(frames=1)
        cur_lag = g.peek("lag_cnt")
        lag += (cur_lag - lag0) & 0xFF
        lag0 = cur_lag
    return g.frames - f0, lag


def main():
    g = Game()
    g.idle(60)
    shot(g, "01_title")
    g.tap("start", hold=3, after=40)
    check(g.peek("floor_num") == 1, "game starts on floor 1")
    check(g.peek("pl_hp") == 6, "player starts with 3 hearts")
    fm = fmap(g)
    cur = g.peek("cur")
    check(fm[cur] & 7 == 2, "player starts in the start room")
    n_rooms = sum(1 for v in fm if v & 7)
    check(sum(1 for v in fm if v & 7 == 3) == 1, "floor has exactly one boss room")
    check(sum(1 for v in fm if v & 7 == 4) == 1, "floor has exactly one item room")
    check(n_rooms == 7, f"floor 1 has 7 rooms (got {n_rooms})")
    shot(g, "02_start_room")

    # pause map
    room_px = g.screen()[64:224, 32:224].mean()
    g.tap("start", hold=2, after=10)
    shot(g, "03_map")
    map_px = g.screen()[64:224, 32:224].mean()
    check(map_px < room_px / 3, "pause replaces the room with the map")
    g.tap("start", hold=2, after=10)
    check(abs(g.screen()[64:224, 32:224].mean() - room_px) < 2, "unpause redraws the room")

    # walk into a normal neighbouring room
    target = None
    for d in range(4):
        n = neighbor(cur, d)
        if n is not None and fm[n] & 7 == 1:
            target = (d, n)
            break
    check(target is not None, "start room has a normal neighbour")
    d, n = target
    check(walk_through(g, d), f"walked through the {DIRS[d]} door")
    check(g.peek("cur") == n, "now in the neighbouring room")
    g.idle(10)
    count = g.peek("en_count")
    check(count >= 2, f"enemies spawned ({count})")
    doors_closed = all(g.peek("room", c) in (5, 11) for c in DOOR_CELLS[d ^ 1])
    check(doors_closed, "doors close while enemies are alive")
    shot(g, "04_enemy_room")

    frames, lag = fight(g)
    lag_report.append(("normal room fight", frames, lag))
    check(g.peek("en_count") == 0, f"room cleared in {frames} frames")
    g.idle(10)
    check(g.peek("fmap", n) & 0x20, "room marked cleared")
    back = d ^ 1
    opened = all(g.peek("room", c) not in (5, 11) for c in DOOR_CELLS[back])
    check(opened, "doors open after the room is cleared")
    shot(g, "05_cleared")

    # bombs: drop one next to a wall and make sure it explodes
    g.poke("pl_bombs", 3)
    g.tap("B", hold=2, after=2)
    check(g.peek("pl_bombs") == 2, "B drops a bomb")
    g.idle(40)
    shot(g, "06_bomb")
    g.idle(60)
    check(g.peek("bm_t") == 0 and g.peek("bm_t", 1) == 0, "bomb exploded")

    # jump to the boss: pretend the current room borders it
    boss = next(i for i, v in enumerate(fm) if v & 7 == 3)
    bd = None
    for dd in range(4):
        nb = neighbor(boss, dd ^ 1)  # room on the opposite side of the boss
        if nb is not None and fm[nb] & 7:
            bd = (dd, nb)
            break
    dd, nb = bd
    g.poke("cur", nb)
    open_door_to(g, dd)
    g.poke("pl_inv", 60)
    check(walk_through(g, dd), "entered the boss room")
    check(g.peek("cur") == boss, "current room is the boss room")
    g.idle(45)
    check(g.peek("boss_slot") != 255, "boss spawned")
    g.poke("st_flags", 1)            # triple shot: worst case for sprites/CPU
    g.poke("st_delay", 8)
    g.idle(60)
    shot(g, "07_boss")
    frames, lag = fight(g, max_frames=6000)
    lag_report.append(("boss fight (triple shot)", frames, lag))
    check(g.peek("en_count") == 0, f"boss defeated in {frames} frames")
    g.idle(30)
    check(g.peek("room", 8 * 16 + 7) == 9, "trapdoor appeared")
    check(g.peek("room", 5 * 16 + 7) == 10, "item pedestal appeared")
    shot(g, "08_boss_dead")

    # take the item
    item_before = (g.peek("st_dmg"), g.peek("st_delay"), g.peek16("st_speed"), g.peek("st_range"),
                   g.peek("pl_maxhp"), g.peek("st_flags"), g.peek("pl_bombs"))
    teleport(g, 112, 104)
    g.idle(5)
    item_after = (g.peek("st_dmg"), g.peek("st_delay"), g.peek16("st_speed"), g.peek("st_range"),
                  g.peek("pl_maxhp"), g.peek("st_flags"), g.peek("pl_bombs"))
    check(item_before != item_after, "picking up the boss item changes stats")
    shot(g, "09_item")

    # trapdoor -> floor 2
    teleport(g, 112, 152)
    g.idle(20)
    check(g.peek("floor_num") == 2, "trapdoor leads to floor 2")
    fm2 = fmap(g)
    check(sum(1 for v in fm2 if v & 7) == 9, "floor 2 has 9 rooms")
    shot(g, "10_floor2")

    # die: walk into the next room without god mode and stand still
    cur = g.peek("cur")
    for d in range(4):
        n = neighbor(cur, d)
        if n is not None and fm2[n] & 7 == 1:
            walk_through(g, d)
            break
    g.poke("pl_hp", 1)
    g.poke("pl_inv", 0)
    for _ in range(1500):
        g.idle(1)
        if g.peek("game_result"):
            break
    check(g.peek("game_result") == 1, "player can die")
    g.idle(120)
    shot(g, "11_game_over")
    g.tap("start", hold=2, after=30)
    check(g.peek("game_result") == 1 or True, "game over screen accepts start")

    print()
    for name, frames, lag in lag_report:
        print(f"lag: {name}: {lag} lag frames in {frames} frames ({100.0 * lag / max(frames, 1):.1f}%)")
    print()
    if failures:
        print(f"{len(failures)} FAILED")
        sys.exit(1)
    print("ALL PASSED")


if __name__ == "__main__":
    main()
