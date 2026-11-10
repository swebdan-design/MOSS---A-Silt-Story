"""Symbol lookup for MOSS debugging (reads build/moss.noi)."""
import re
def load(path='build/moss.noi'):
    syms = []
    for ln in open(path):
        p = ln.split()
        if len(p) == 3 and p[0] == 'DEF':
            v = int(p[2], 16)
            syms.append((v, p[1]))
    return sorted(syms)
def addr(syms, name):
    for v, n in syms:
        if n == name: return v
    raise KeyError(name)
def near(syms, a, bank=None):
    best = None
    for v, n in syms:
        b, off = v >> 16, v & 0xFFFF
        if off <= a and (off < 0x4000 or bank is None or b == bank):
            if best is None or off > best[0] & 0xFFFF: best = (v, n)
    return best
