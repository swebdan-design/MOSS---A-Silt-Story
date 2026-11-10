#!/usr/bin/env python3
"""Full automated playthrough of the compiled MOSS ROM in PyBoy.

Plays all five days through real button input (path-finding on the live
collision map), solves every puzzle, collects all eight discoveries, plays
the pond, reaches the ending, then checks the post-game title option.
Writes screenshots and build/qa/playthrough.json. Exit status 1 on failure.
"""
import sys, os, json, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from play import Player

ROM = sys.argv[1] if len(sys.argv) > 1 else 'release/MOSS_by_Synth_Centric.gb'
OUT = 'build/qa' + ('_sameboy' if os.environ.get('MOSS_EMU') == 'sameboy' else '')
os.makedirs(OUT, exist_ok=True)
M = {n: i for i, n in enumerate(['cottage', 'village', 'wood', 'pond', 'creek', 'railway', 'hill', 'clearing', 'pondscreen'])}
BEH = dict(WIND=1, WATER=2, INSECTS=4, WOOD=8, RAIN=16, ECHO=32, STILL=64, MEMORY=128)
results = []
p = None


def check(name, cond, detail=''):
    results.append({'check': name, 'result': 'PASS' if cond else 'FAIL', 'detail': detail})
    print(('PASS ' if cond else 'FAIL ') + name + (f'  ({detail})' if detail else ''), flush=True)
    return cond


def shot(name):
    p.shot(f'{OUT}/{name}.png')


def recover():
    # if a stray A press sat us down at the pond, stand up again
    for _ in range(3):
        if p.pos()[0] != M['pondscreen']: return
        p.press('start', 3, 30); p.press('a', 3, 120); p.settle(200)


def go(mapname, x, y):
    recover()
    ok = p.warp(x, y, M[mapname])
    if not ok:
        print('  warp failed, at', p.pos())
    return ok


def listen(frames=260):
    p.tick(frames)
    p.settle(120)


def to_map(path):
    for (m, x, y) in path:
        if p.pos()[0] == M[m]: continue
        go(m, x, y)


HOME = {  # routes back to the cottage from each map
    'pond': [('wood', 6, 10), ('village', 8, 15), ('cottage', 3, 9)],
    'wood': [('village', 8, 15), ('cottage', 3, 9)],
    'creek': [('village', 0, 7), ('cottage', 3, 9)],
    'hill': [('village', 11, 6), ('cottage', 3, 9)],
    'railway': [('creek', 11, 8), ('village', 0, 7), ('cottage', 3, 9)],
    'clearing': [('wood', 9, 4), ('village', 8, 15), ('cottage', 3, 9)],
    'village': [('cottage', 3, 9)],
}
NAMES = {v: k for k, v in M.items()}


def home():
    recover()
    m = NAMES[p.pos()[0]]
    for (dest, x, y) in HOME.get(m, []):
        go(dest, x, y)


def sleep():
    recover()
    p.use(8, 3)          # the bed, from the side
    p.tick(30)
    p.press('a', 3, 20)  # "Sleep"
    for _ in range(80):
        p.tick(10)
        if p.dialog_up(): p.advance()
        if p.pos()[0] == M['cottage'] and p.u8('_world_exit') == 0 and not p.window_up() and p.day() > d0[0]:
            break
    p.settle(200)


t0 = time.time()
p = Player(ROM)
p.tick(240)
shot('00_title')
check('boot to title', p.pb.memory[0xFF47] != 0, 'BGP=%02X' % p.pb.memory[0xFF47])
p.press('a'); p.tick(90)
shot('01_opening_prose')
for _ in range(40):
    if p.pos()[0] == M['cottage']: break
    p.press('a', 3, 47)
p.tick(60); p.settle(300)
shot('02_cottage_mae')
check('new game starts in the cottage on day 1', p.pos()[0] == M['cottage'] and p.day() == 1, str(p.pos()))
check('Mae introduced', p.flag(0) and p.flag(1))

# ---------------- DAY 1
go('village', 4, 8); shot('03_village')
check('village reached', p.pos()[0] == M['village'])
go('wood', 7, 0); shot('04_wood')
go('pond', 8, 0); p.settle(200); shot('05_pond_reveal')
check('pond discovered', p.flag(2))
p.goto(8, 7, face='up'); p.press('a', 3, 30); p.press('a', 3, 10)
for _ in range(40):
    p.tick(10)
    if p.dialog_up(): break
shot('06_first_pebble')
p.advance(); p.tick(60); p.advance(); p.tick(30)
for _ in range(10):
    if p.dialog_up(): p.advance()
    p.tick(20)
check('first pebble answered from the reeds', p.flag(3))
p.tick(120); p.settle(200); recover()
shot('07_dusk_pond')
home(); p.settle(400)
check('notebook received from Mae', p.flag(4))
shot('08_notebook_scene')
d0 = [1]
sleep()
check('day 2 begins', p.day() == 2, str(p.day()))
shot('09_day2_morning')

# ---------------- DAY 2
d0 = [2]
go('village', 4, 8); go('wood', 7, 0)
p.use(4, 11); p.settle(200)         # Iris
check('met Iris', p.flag(6))
p.use(2, 11); p.settle(400)         # knock on the log
shot('10_wood_log')
check('HOLLOW WOOD discovered', p.found() & BEH['WOOD'], hex(p.found()))
p.goto(10, 5); listen(); shot('11_reeds')
check('WIND discovered by listening in the reeds', p.found() & BEH['WIND'], hex(p.found()))
go('village', 8, 15); go('creek', 15, 6)
p.use(1, 5); p.settle(200)           # watch the water rings
for s in (6, 8, 4):                  # low, high, middle
    p.goto(s, 6, face='up'); p.press('a', 3, 40); p.settle(60)
p.settle(500)
shot('12_creek_stones')
check('THREE STONES puzzle -> WATER discovered', p.found() & BEH['WATER'], hex(p.found()))
go('village', 0, 7); go('hill', 0, 6)
p.goto(8, 3); listen(300); shot('13_hill_meadow')
check('INSECTS discovered in the meadow', p.found() & BEH['INSECTS'], hex(p.found()))
p.goto(3, 2); listen(260); shot('14_hill_view')
check('hill viewpoint', p.flag(22))
go('village', 11, 6); go('cottage', 3, 9)
sleep()
check('day 3 begins (rain)', p.day() == 3)

# ---------------- DAY 3
d0 = [3]
go('village', 4, 8); shot('15_rain_village')
p.goto(10, 11); listen(400); shot('16_shelter')
check('RAIN SHELTER -> RAIN discovered', p.found() & BEH['RAIN'], hex(p.found()))
go('creek', 15, 6); go('railway', 11, 0)
p.use(9, 5); p.settle(200)           # Mr Vale
p.goto(6, 6, face='up'); p.press('a', 3, 30); p.settle(500)
shot('17_last_bell')
check('LAST BELL -> ECHO discovered', p.found() & BEH['ECHO'], hex(p.found()))
go('creek', 11, 8); go('village', 0, 7); go('wood', 7, 0)
p.goto(3, 8); listen(300)
check('HIDDEN PATH revealed by listening', p.flag(16))
shot('18_hidden_path')
go('clearing', 0, 8); p.settle(200)
p.goto(5, 4); listen(700); shot('19_still_clearing')
check('STILL CLEARING -> STILLNESS discovered', p.found() & BEH['STILL'], hex(p.found()))
home()
sleep()
check('day 4 begins', p.day() == 4)
shot('20_day4_boxes')

# ---------------- DAY 4
d0 = [4]
p.goto(6, 2, face='up'); p.press('a', 3, 30); p.settle(800)
check('wind chime -> MEMORY discovered', p.found() & BEH['MEMORY'], hex(p.found()))
check('all eight ways of listening found', p.found() == 0xFF, hex(p.found()))
go('village', 4, 8); go('wood', 7, 0); go('pond', 8, 0)
p.goto(8, 7, face='up'); p.press('a', 3, 30); p.press('a', 3, 60)   # Sit
p.settle(300)
check('pond instrument entered', p.pos()[0] == M['pondscreen'])
shot('21_pond_instrument')
# the bell's interval: lowest row, then the second row
p.press('down', 8, 8); p.press('down', 8, 8)        # cursor row 4
p.press('a', 8, 40)
p.press('up', 8, 8); p.press('up', 8, 8); p.press('up', 8, 8)  # row 1
p.press('a', 8, 400)
shot('22_pond_answer')
p.tick(600)
heard = p.u8('_pond_answer_heard')
check("THE POND'S ANSWER", heard == 1)
p.press('start', 3, 30); p.press('a', 3, 60)       # Stand up
p.settle(600)
check('answer recorded in the story', p.flag(19))
shot('23_evening')
home()
sleep()
check('day 5 begins (departure)', p.day() == 5)

# ---------------- DAY 5
d0 = [5]
p.settle(200)
go('village', 4, 8); go('wood', 7, 0)
p.goto(8, 0)
# the ending sequence runs by itself; the player only reads
names = {20: '24_final_pond', 60: '25_last_page', 75: '26_stone_left', 140: '27_boy_gone',
         205: '28_always_here', 270: '29_ending_card'}
for i in range(700):
    p.tick(30)
    if p.dialog_up() and p.pb.memory[0xFF4A] == 104: p.press('a', 3, 10)
    if i in names: shot(names[i])
    if p.game_u8(5) == 1 and p.u8('_world_exit') == 1: break
check('ending reached, stone left at the pond', p.flag(40))
p.tick(600)
shot('30_title_postgame')
with open(f'{OUT}/postgame_title.state', 'wb') as f:
    try: p.pb.save_state(f)
    except Exception: pass
check('post-game unlocked', p.game_u8(5) == 1)

# ---------------- POST-GAME
p.press('down', 3, 10); p.press('down', 3, 10)
p.press('a', 3, 200)
p.settle(60)
check('Return to the pond opens the instrument', p.pos()[0] == M['pondscreen'])
shot('31_return_to_pond')
p.press('start', 3, 30)
for _ in range(10): p.press('down', 3, 6)
p.press('right', 3, 30)   # change preset
shot('32_pond_menu')
p.press('b', 3, 30)
p.tick(600)
shot('33_preset_playing')
check('instrument keeps playing autonomously', p.u16('_audio_played') > 100, str(p.u16('_audio_played')))

ok = all(r['result'] == 'PASS' for r in results)
json.dump({'rom': ROM, 'emulator': os.environ.get('MOSS_EMU', 'pyboy'), 'seconds': round(time.time() - t0),
           'frames': p.pb.frame_count, 'results': results}, open(f'{OUT}/playthrough.json', 'w'), indent=1)
print('\n%d/%d checks passed, %d emulated frames (%.0f minutes of play)' %
      (sum(r['result'] == 'PASS' for r in results), len(results), p.pb.frame_count, p.pb.frame_count / 3584))
sys.exit(0 if ok else 1)
