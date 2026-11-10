"""High-level automated play for MOSS QA: path-finding over the live
collision map, text advancing, menu answers. Used by tests/emu_playthrough.py."""
import collections
from emu import Session, DIRS

DXY = {'down': (0, 1), 'up': (0, -1), 'left': (-1, 0), 'right': (1, 0)}


ORDER = ['cottage', 'village', 'wood', 'pond', 'creek', 'railway', 'hill', 'clearing', 'pondscreen']


def load_warps():
    import pathlib
    root = pathlib.Path(__file__).resolve().parents[1] / 'assets/maps'
    out = {}
    for i, n in enumerate(ORDER):
        out[i] = set()
        for ln in (root / f'{n}.txt').read_text().splitlines():
            t = ln.split()
            if t and t[0] == 'warp':
                out[i].add((int(t[1]), int(t[2])))
    return out


WARPS = load_warps()


class Player(Session):
    def text_open(self):
        # dialogue box: window shown at the bottom (WY=104)
        return self.window_up() and self.pb.memory[0xFF4A] == 104

    def advance(self, limit=60, choose=None):
        """Press A through dialogue until the box closes."""
        n = 0
        while self.dialog_up() and n < limit:
            self.press('a', 3, 10)
            n += 1
        self.tick(4)
        return n

    def dialog_up(self):
        # dialogue boxes and questions sit at WY=104; full pages (notebook
        # entries) at WY=0 WX=7. The pond HUD (WY=128) and side menus are not.
        if not self.window_up(): return False
        wy, wx = self.pb.memory[0xFF4A], self.pb.memory[0xFF4B]
        return wy == 104 or (wy == 0 and wx == 7)

    def settle(self, frames=30):
        for _ in range(frames // 5):
            if self.dialog_up():
                self.advance()
            self.tick(5)

    def grid(self):
        w, h = self.u8('_map_w'), self.u8('_map_h')
        base = self.sym('_coll')
        cells = [[self.pb.memory[base + y * w + x] for x in range(w)] for y in range(h)]
        npcs = set()
        nb = self.sym('_npcs')
        for i in range(6):
            who, x, y = self.pb.memory[nb + i * 8], self.pb.memory[nb + i * 8 + 1], self.pb.memory[nb + i * 8 + 2]
            hidden = self.pb.memory[nb + i * 8 + 6]
            if who and not hidden: npcs.add((x, y))
        return w, h, cells, npcs

    def path(self, tx, ty, adjacent=False):
        w, h, cells, npcs = self.grid()
        _, px, py, _ = self.pos()
        start = (px, py)
        prev = {start: None}
        q = collections.deque([start])
        goal = None
        while q:
            c = q.popleft()
            if (not adjacent and c == (tx, ty)) or (adjacent and abs(c[0] - tx) + abs(c[1] - ty) == 1):
                goal = c; break
            for d, (dx, dy) in DXY.items():
                n = (c[0] + dx, c[1] + dy)
                if not (0 <= n[0] < w and 0 <= n[1] < h): continue
                if n in prev: continue
                if cells[n[1]][n[0]] & 1 or n in npcs: continue
                if n in WARPS.get(self.pos()[0], ()) and n != (tx, ty): continue
                prev[n] = (c, d)
                q.append(n)
        if goal is None:
            return None
        steps = []
        c = goal
        while prev[c]:
            c, d = prev[c][0], prev[c][1]
            steps.append(d)
        return list(reversed(steps)), goal

    def walk(self, d):
        before = self.pos()
        self.pb.button_press(d)
        for _ in range(40):
            self.tick(1)
            if self.pos() != before and self.pos()[3] == DIRS[d] and self.pos()[1:3] != before[1:3]:
                break
            if self.dialog_up():
                break
        self.pb.button_release(d)
        self.tick(10)

    def goto(self, tx, ty, adjacent=False, face=None):
        m0 = self.pos()[0]
        for attempt in range(6):
            r = self.path(tx, ty, adjacent)
            if r is None:
                raise RuntimeError(f'no path to {tx},{ty} on map {self.pos()}')
            steps, goal = r
            for d in steps:
                self.walk(d)
                if self.dialog_up():
                    self.advance()
                if self.pos()[0] != m0:
                    self.tick(40); return 'warped'
            if self.pos()[1:3] == goal:
                break
        if adjacent:
            _, px, py, _ = self.pos()
            d = {(1, 0): 'right', (-1, 0): 'left', (0, 1): 'down', (0, -1): 'up'}[(tx - px, ty - py)]
            self.face(d)
        elif face:
            self.face(face)
        return 'ok'

    def face(self, d):
        if self.pos()[3] != DIRS[d]:
            self.press(d, 2, 8)

    def use(self, x, y):
        self.goto(x, y, adjacent=True)
        self.press('a', 3, 20)

    def warp(self, x, y, expect_map):
        self.goto(x, y)
        for _ in range(60):
            self.tick(5)
            if self.dialog_up(): self.advance()
            if self.pos()[0] == expect_map and self.u8('_world_exit') == 0:
                break
        self.tick(30)
        self.settle()
        return self.pos()[0] == expect_map

    def flag(self, n):
        g = self.sym('_game')
        # offset of flags in SaveData: magic(2) version(1) seq(1) day postgame map x y dir (6) -> 10
        return bool(self.pb.memory[g + 10 + (n >> 3)] & (1 << (n & 7)))

    def game_u8(self, off):
        return self.pb.memory[self.sym('_game') + off]

    def day(self): return self.game_u8(4)
    def found(self): return self.game_u8(18)
