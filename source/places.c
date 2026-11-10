/* MOSS — places: things to look at, things to listen to, the puzzles. */
#pragma bank 12
#include <gb/gb.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

void story_sleep(void) BANKED;

static SndEv e;
static void snd(uint8_t ch, uint8_t note, uint8_t env, uint8_t duty, uint8_t hold, uint8_t fx, uint8_t fxarg, uint8_t delay) {
    e.ch = ch; e.note = note; e.env = env; e.timbre = duty; e.hold = hold;
    e.prio = PRIO_MOTIF; e.fx = fx; e.fxarg = fxarg; e.pan = PAN_C;
    snd_play(&e, delay);
}

static uint8_t has_book(void) { return FLAG(F_NOTEBOOK) != 0; }
static void no_book(void) { say("I want to remember this. But I've nothing to write in."); }

/* ================================================================ scenes */
static void scene_arrival(void) {
    SET(F_INTRO);
    world_wait(30);
    say("Mae: There you are! Let me look at you. Taller. Thinner. Same ears.");
    npc_walk(WHO_MAE, DIR_DOWN, 1);
    say("Mae: Leave your bag by the bed. The one by the window, same as always.\fThis'll be the last summer in the cottage, you know. I expect your mum told you.");
    say("Mae: I'm not going far. A flat in town, near your aunt. Lifts and everything.\fBut there's a lot of cottage to fit into a flat. So. Boxes.");
    say("Mae: Go and have a wander before supper. When your mum was small she'd vanish up to the pond for hours.");
    say("I didn't remember a pond.");
    say("Mae: No? North of the village, through the wood. You were four. You threw your shoe in it.\fYou were very pleased with yourself. The shoe less so.");
    SET(F_MET_MAE);
    say("(Walk with the + pad. Press A to talk or to look at things. START opens the menu.)");
}

static void scene_notebook(void) {
    SET(F_NOTEBOOK);
    npc_place(WHO_MAE, 5, 5, DIR_DOWN);
    face_player(WHO_MAE);
    world_wait(20);
    say("Mae: There you are. I was about to send the cat.\fFound it, then? Your pond.");
    say("I told her about the stone, and the note, and the thing in the reeds that answered.");
    say("Mae: Did it now.");
    world_wait(40);
    say("Mae: Here. Your grandad kept one of these. Birds, mostly. And the weather.\fWrite down what you hear, not just what you see. Hearing's harder to keep.");
    compose_motif(MOTIF_MAE, 10);
    say("Mae gave me a small notebook with a soft green cover.\f(Press SELECT to open the notebook. It also says where you might go next.)");
    say("Mae: Bed's made up. Sleep when you're ready.");
}

static void scene_morning(void) {
    uint8_t d = game.day;
    world_wait(20);
    if (d == 2 && !FLAG(F_DAY2_INTRO)) {
        SET(F_DAY2_INTRO);
        say("Mae: Morning, sleepyhead. Bright one today.\fIris from the blue house was round asking after you. She went off to the wood.");
    } else if (d == 3 && !FLAG(F_DAY3_INTRO)) {
        SET(F_DAY3_INTRO);
        say("Rain on the roof. Lots of it.");
        say("Mae: Hollis said it would rain. He's never wrong about rain. It's the only thing.\fGo out in it if you like. You won't melt. Your mother tested that thoroughly.");
    } else if (d == 4 && !FLAG(F_DAY4_INTRO)) {
        SET(F_DAY4_INTRO);
        say("The cottage was full of boxes. The kitchen echoed in a way it hadn't yesterday.");
        say("Mae: Last whole day. I thought we'd make it a slow one.\fCould you fetch the wind chime down from the window, love? It's going in the box marked FRAGILE.");
    } else if (d == 5 && !FLAG(F_DAY5_INTRO)) {
        SET(F_DAY5_INTRO);
        say("Early. Mist on the window. Everything packed except the kettle.");
        say("Mae: The car's coming at eight.\fGo on. Go and say goodbye to it. I'll finish up here.");
    }
}

static void scene_pond_reveal(void) {
    SET(F_POND_SEEN);
    world_wait(30);
    say("There was a pond.");
    say("Small. Quiet. Two flat stones sat in the water like they'd been put there on purpose.");
    say("I didn't remember it at all. But my feet seemed to know the way to the bank.");
}

static void scene_first_pebble(void) {
    uint8_t i;
    say("I picked up a pebble. Smooth, and still warm from the sun.");
    if (ask("Drop it into the water?", "Drop it|Keep it") != 0) {
        say("I put the pebble in my pocket. Not yet.");
        return;
    }
    compose_hush(40);
    snd(CH_PEBBLE, 74, 0xC3, 2, 40, FX_SWEEP, 0x16, 0);
    fx_ripples(8, 5, 3);
    world_wait(70);
    say("Plip. A ring opened on the water and a note came out of it.\fNot a splash. A note.");
    world_wait(40);
    npc_place(WHO_REED, PT_POND_REED_X, PT_POND_REED_Y, DIR_DOWN);
    world_wait(20);
    compose_motif(MOTIF_POND, 0);
    for (i = 0; i < 4; ++i) {
        snd(CH_LIVING, 86 + (i & 1) * 5, 0x61, 0, 12, FX_TRILL, 0x33, (uint8_t)(i * 22 + 50));
    }
    world_wait(90);
    npc_remove(WHO_REED);
    world_wait(20);
    say("Something in the reeds answered.");
    say("When I looked properly, there was nothing there. Only reeds, and the ring still spreading.");
    SET(F_FIRST_PEBBLE);
    mmem.motifs |= 1;
    bgp_normal = 0xF9;
    fade_out();
    fade_in();
    say("The light had gone orange. Mae would be wondering where I was.\f(You can sit by the pond any time: face the water and press A.)");
}

/* ================================================================ enter */
void place_enter(uint8_t map) BANKED {
    if (map == MAP_COTTAGE) {
        if (game.day == 1 && !FLAG(F_INTRO)) scene_arrival();
        else if (game.day == 1 && FLAG(F_FIRST_PEBBLE) && !FLAG(F_NOTEBOOK)) scene_notebook();
        else if (game.day >= 2) scene_morning();
    } else if (map == MAP_POND) {
        if (game.day == 5 && !FLAG(F_STONE_LEFT)) { ending_run(); return; }
        if (!FLAG(F_POND_SEEN)) scene_pond_reveal();
        if (game.day == 4 && FLAG(F_ANSWER) && !FLAG(F_LAST_VISIT)) {
            SET(F_LAST_VISIT);
        }
    } else if (map == MAP_CLEARING && !FLAG(F_CLEARING)) {
        SET(F_CLEARING);
        world_wait(20);
        if (game.day == 3) say("The rain didn't seem to reach in here.");
        say("A clearing, round as a room. Moss everywhere, soft and thick. Nobody had walked here in a long time.");
    } else if (map == MAP_RAILWAY && game.day == 3 && !FLAG(F_BELL_RUNG)) {
        compose_motif(MOTIF_TRAIN, 30);
    }
}

void place_step(uint8_t map, uint8_t x, uint8_t y) BANKED {
    (void)map; (void)x; (void)y;
}

/* ================================================================ pond */
void place_water(void) BANKED {
    if (!FLAG(F_FIRST_PEBBLE)) { scene_first_pebble(); return; }
    if (game.day == 5) return;
    if (ask("Sit by the water and listen?", "Sit|Not now") != 0) return;
    pond_run(POND_STORY);
    if (pond_answer_heard && !FLAG(F_ANSWER)) {
        SET(F_ANSWER);
        say("The pond answered.\fNot with words. With the same two notes, turned round, and then a few more of its own.");
        say("Like it had been waiting to be asked properly.");
        say("When I stood up, my legs were stiff and the sky was nearly dark.");
        bgp_normal = 0xF9;
        save_write();
    }
    world_enter(MAP_POND, px, py, DIR_UP);
}

/* ================================================================ puzzles */
static uint8_t stones[3], stone_tries;
static const uint8_t stone_note[3] = {67, 62, 69};   /* middle (G), low (D), high (A) */

static void knock_stone(uint8_t s) {
    snd(CH_PEBBLE, stone_note[s], 0xB3, 2, 30, 0, 0, 0);
    snd(CH_ENV, 0x3B, 0x51, 0, 4, 0, 0, 0);
    fx_ripples((uint8_t)(4 + s * 2), 4, s == 1 ? 3 : s == 0 ? 2 : 1); /* ring size shows pitch */
    world_wait(24);
    stones[0] = stones[1]; stones[1] = stones[2]; stones[2] = s + 1;
    if (game.found & BEH_WATER) { say(s == 1 ? "The low stone." : s == 2 ? "The high stone." : "The middle stone."); return; }
    if (stones[0] == 2 && stones[1] == 3 && stones[2] == 1) {
        if (!has_book()) { no_book(); return; }
        world_wait(30);
        compose_hush(24);
        snd(CH_WATER, 50, 3, WAVE_SOFT, 60, FX_WDECAY, 0, 10);
        snd(CH_WATER, 57, 3, WAVE_SOFT, 60, FX_WDECAY, 0, 40);
        snd(CH_WATER, 55, 3, WAVE_SOFT, 80, FX_WDECAY, 0, 70);
        fx_ripples(2, 4, 3);
        world_wait(130);
        say("Deep, high, in between. Exactly what the water was saying.");
        say("Then the creek said it back. Lower, and slower, as if it had been keeping the song for someone.");
        SET(F_STONES_DONE);
        story_discover(BEH_WATER);
        return;
    }
    if (++stone_tries == 6) {
        say("Maybe I should listen to the water first. It seems to know the tune.");
    } else if (stone_tries == 12) {
        say("The water goes: deep, high, in between. The deepest stone is in the middle.");
    }
}

static void ring_bell(void) {
    compose_hush(30);
    compose_motif(MOTIF_BELL, 0);
    world_wait(60);
    if (game.day < 3) {
        say("The bell rang: low, then high. It sounded small out here, like it was waiting for something.");
        return;
    }
    /* the cutting answers */
    snd(CH_LIVING, 62, 0x63, 2, 30, 0, 0, 10);
    snd(CH_LIVING, 69, 0x63, 2, 30, 0, 0, 40);
    snd(CH_LIVING, 62, 0x33, 2, 30, 0, 0, 80);
    snd(CH_LIVING, 69, 0x33, 2, 30, 0, 0, 110);
    world_wait(150);
    SET(F_BELL_RUNG);
    if (!(game.found & BEH_ECHO)) {
        say("Low, then high. Then, from the cutting, a moment later, it rang again. Quieter. Then quieter still.");
        say("Two notes, low then high. I'd heard them somewhere before. I just couldn't think where.");
        if (has_book()) story_discover(BEH_ECHO); else no_book();
    } else {
        say("The bell, and the cutting, and the cutting again.");
    }
}

/* ================================================================ objects */
void place_object(uint8_t map, uint8_t obj) BANKED {
    uint8_t d = game.day;
    switch (map) {
    case MAP_COTTAGE:
        switch (obj) {
        case OBJ_COTTAGE_SHELF: case OBJ_COTTAGE_BOOKS:
            if (d >= 4) say("The shelf is empty. The books are in a box marked BOOKS (HEAVY) (SORRY).");
            else say("Mae's books. Birds. Bees. Bridges. A whole shelf just about rivers.");
            break;
        case OBJ_COTTAGE_CLOCK:
            snd(CH_ENV, 0x29, 0x31, 0, 3, 0, 0, 0); snd(CH_ENV, 0x2A, 0x31, 0, 3, 0, 0, 30);
            snd(CH_ENV, 0x29, 0x31, 0, 3, 0, 0, 60); snd(CH_ENV, 0x2A, 0x31, 0, 3, 0, 0, 90);
            SET(F_CLOCK);
            if (d == 5) say("The clock is wrapped in newspaper. I can still hear it ticking, muffled, like it's somewhere else.");
            else say("The kitchen clock. Five minutes fast for as long as anyone remembers.\fNobody fixes it. Everybody knows.");
            break;
        case OBJ_COTTAGE_WINDOW:
            if (d == 3) say("Rain running down the glass in long wobbly lines, racing each other.");
            else if (d == 5) say("Mist. The village has been rubbed out, gently, like a drawing.");
            else if (d == 4) say("Through the window: the garden, the lane, the wood. It all looks as if it's posing.");
            else say("Through the window: the garden, the lane, the wood. Something's moving in the hedge. The cat, probably.");
            break;
        case OBJ_COTTAGE_PHOTOS:
            SET(F_PHOTOS);
            if (d >= 4) say("Pale squares on the wallpaper where the photographs were. The wallpaper underneath is a brighter green.");
            else say("Photographs. Grandad on the hill. Mae laughing at something outside the picture.\fMum, small, beside water I almost recognise.");
            break;
        case OBJ_COTTAGE_CUPBOARD:
            snd(CH_ENV, 0x21, 0x52, 0, 20, FX_TRILL, 0x52, 0);
            say("Tins, string, candles, and a tin of buttons that rattles like rain on a roof.");
            break;
        case OBJ_COTTAGE_KETTLE:
            SET(F_KETTLE);
            snd(CH_PEBBLE, 79, 0x48, 2, 90, FX_SLIDE, 0x01, 0);
            say("The kettle. Before it boils it sings: one thin note that climbs and climbs and never quite arrives.");
            break;
        case OBJ_COTTAGE_STOVE:
            say("The old stove ticks as it cools down, like it's counting.");
            break;
        case OBJ_COTTAGE_TABLE:
            if (d == 5) say("One cup on the table, for the road. Everything else is packed.");
            else say("Two cups. The one with the chip in the handle is Mae's. She says the chip is where she holds it.");
            break;
        case OBJ_COTTAGE_RADIO:
            SET(F_RADIO);
            if (d >= 5) { say("The radio's packed. The table looks surprised without it."); break; }
            compose_hush(40);
            compose_motif(MOTIF_MAE, 4);
            world_wait(60);
            say("A crackly old song. Mae hums along to the bits she knows and makes up the rest.");
            break;
        case OBJ_COTTAGE_CHIME:
            if (d < 4) {
                say("Mae's wind chime hangs in the window, perfectly still. There's no wind indoors.");
            } else if (!(game.found & BEH_MEMORY)) {
                SET(F_CHIME_HEARD);
                say("I lifted the wind chime off its hook.");
                compose_hush(40);
                compose_motif(MOTIF_CHIME, 0);
                world_wait(60);
                say("It rang. Two notes. Low, then high.");
                say("The same two notes as the station bell.");
                npc_face(WHO_MAE, DIR_UP);
                say("Mae: Your grandad hung that. He said it played the same notes as the old station bell,\fso when it rang he'd know the wind was coming from the railway.");
                say("Mae: The trains stopped years ago. He kept saying it anyway. 'Wind's from the railway.' As if they might still come.");
                world_wait(30);
                say("Mae: You keep it. It's no good in a flat. Nowhere for the wind to come from.");
                story_discover(BEH_MEMORY);
                say("Mae: Now go on up to your pond. Take as long as you like.");
            } else if (d == 4) {
                say("The chime is in my pocket, wrapped in a sock so it won't talk.");
            } else {
                say("Just a hook where the chime was.");
            }
            break;
        case OBJ_COTTAGE_BED:
            if (game.postgame) { say("I'd rather stay in this afternoon a little longer."); break; }
            if (d == 1 && !FLAG(F_NOTEBOOK)) { say("Not tired yet. It's still light out."); break; }
            if (d == 2 && (game.found & (BEH_WIND | BEH_WATER | BEH_INSECTS | BEH_WOOD)) != (BEH_WIND | BEH_WATER | BEH_INSECTS | BEH_WOOD)) {
                say("Not tired yet. There's still a lot to hear today."); break;
            }
            if (d == 3 && (game.found & (BEH_RAIN | BEH_ECHO | BEH_STILL)) != (BEH_RAIN | BEH_ECHO | BEH_STILL)) {
                say("Not tired yet. The rain's still going. There's more out there."); break;
            }
            if (d == 4 && !FLAG(F_ANSWER)) { say("Not yet. Not before the pond."); break; }
            if (d >= 5) { say("The bed's stripped. Just a mattress with a map of old dents."); break; }
            if (ask("Go to sleep?", "Sleep|Not yet") == 0) story_sleep();
            break;
        case OBJ_COTTAGE_PLANT:
            say("A spider plant that has outlived three cats and a flood.");
            break;
        }
        break;

    case MAP_VILLAGE:
        switch (obj) {
        case OBJ_VILLAGE_SIGN:
            say("NORTH: Whispering Wood. WEST: the hill. EAST: the creek and the old station.\fSomeone has added, in pencil: and the pond, if you know.");
            break;
        case OBJ_VILLAGE_WELL:
            SET(F_WELL);
            snd(CH_PEBBLE, 72, 0x93, 2, 40, FX_SWEEP, 0x1E, 50);
            world_wait(70);
            if (game.found & BEH_ECHO) say("I dropped a pebble in. A long time, then: plunk. Then the well said plunk again, smaller.");
            else say("I dropped a pebble in. It took a long time to arrive. Plunk.");
            break;
        case OBJ_VILLAGE_IRIS_DOOR:
            if (d == 4) say("From inside: someone singing, and pans. Iris's mum, I think.");
            else say("The blue house that is grey. Nobody answers. Iris is probably out.");
            break;
        case OBJ_VILLAGE_SHOP: case OBJ_VILLAGE_SHOP_DOOR:
            say("ADA'S. Bread, stamps, string, seeds, and news. A teapot sign creaks over the door.");
            break;
        case OBJ_VILLAGE_IRIS_BOX:
            say("Ada's post box. There's a postcard of the sea taped inside, faded nearly white.");
            break;
        case OBJ_VILLAGE_MAE_BOX:
            if (d >= 4) say("Mae's post box. A new label says PLEASE FORWARD in careful capitals.");
            else say("Mae's post box. It says HOLLY COTTAGE. There's no holly. There's never been holly.");
            break;
        case OBJ_VILLAGE_HOLLIS_DOOR:
            say("Mr Hollis's door. A pair of muddy boots waits outside, toes together, very patient.");
            break;
        case OBJ_VILLAGE_CROPS:
            say("Runner beans, climbing anything that stands still long enough.");
            break;
        case OBJ_VILLAGE_FLOWERBED:
            say("Marigolds. Mae says they keep the bugs away. The bugs don't seem to have heard.");
            break;
        case OBJ_VILLAGE_SHELTER:
            if (d == 3 && !(game.found & BEH_RAIN)) say("Mr Hollis's potting shelter: a wooden roof on four posts. The rain is drumming on it.\f(Stand still for a while to listen.)");
            else say("Mr Hollis's potting shelter. Pots, twine, and a radio with no knobs.");
            break;
        }
        break;

    case MAP_WOOD:
        switch (obj) {
        case OBJ_WOOD_LOG: case OBJ_WOOD_LOG2:
            snd(CH_ENV, 0x3C, 0xA1, 0, 6, 0, 0, 0);
            snd(CH_ENV, 0x3B, 0x91, 0, 6, 0, 0, 14);
            snd(CH_ENV, 0x4B, 0xA1, 0, 6, 0, 0, 28);
            world_wait(40);
            if (!(game.found & BEH_WOOD)) {
                if (!has_book()) { say("I knocked on the fallen log. It answered with a round, hollow sound."); no_book(); break; }
                say("I knocked. The log answered with a round, hollow sound, like a word in a language I almost knew.");
                if (FLAG(F_MET_IRIS) && d == 2) {
                    face_player(WHO_IRIS);
                    say("Iris: See? It's hollow all the way through. Something lived in there.\fMaybe it still does. Maybe we just knocked on its door.");
                }
                story_discover(BEH_WOOD);
                if (FLAG(F_MET_IRIS) && d == 2) {
                    SET(F_STONES_HINT);
                    say("Iris: There are three stones down at the creek that sing if you knock them in the right order.\fGrandpa Hollis says they don't. He's wrong. The creek's east of the village.");
                }
            } else {
                say("Knock, knock, knock. The log says the same thing three different ways.");
            }
            break;
        case OBJ_WOOD_FERNS: case OBJ_WOOD_FERNS2:
            say("The ferns are very thick here. Something about them is too neat, like a curtain.");
            break;
        case OBJ_WOOD_MARSH:
            say("A marshy patch, full of reeds. The water's so still it's hard to tell where it starts.");
            break;
        case OBJ_WOOD_MUSHROOM:
            say("Mushrooms, standing about in a little crowd. Don't eat them, Mae would say. I wasn't going to.");
            break;
        case OBJ_WOOD_SIGN_TREE:
            say("Two letters carved into the bark, long ago. The tree has grown around them like it's holding them.");
            break;
        }
        break;

    case MAP_POND:
        if (obj == OBJ_POND_WILLOW || obj == OBJ_POND_WILLOW2)
            say("An old willow. Its branches hang down to the grass, like it's leaning in to hear the water.");
        else
            say("One of the two flat stones. It looks as if it's always been exactly there.");
        break;

    case MAP_CREEK:
        if (obj == OBJ_CREEK_STONE1) knock_stone(0);
        else if (obj == OBJ_CREEK_STONE2) knock_stone(1);
        else if (obj == OBJ_CREEK_STONE3) knock_stone(2);
        else {
            SET(F_STONES_HINT);
            fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 3);
            world_wait(30);
            fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 1);
            world_wait(30);
            fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 2);
            world_wait(30);
            say("Upstream the water folds over a hidden stone and makes rings.\fA big ring. A little ring. A middle-sized ring. Over and over.");
        }
        break;

    case MAP_RAILWAY:
        switch (obj) {
        case OBJ_RAILWAY_SIGN:
            say("The station name has nearly weathered away. Something-ALE. Mr Vale says it was never very exciting.");
            break;
        case OBJ_RAILWAY_BENCH: case OBJ_RAILWAY_BENCH2:
            say("A bench facing the rails. Facing the place where something would arrive.");
            break;
        case OBJ_RAILWAY_BELL:
            ring_bell();
            break;
        case OBJ_RAILWAY_LAMP:
            say("A lamp with no bulb. Moths still visit it in the evening, out of habit.");
            break;
        case OBJ_RAILWAY_EDGE:
            say("The edge of the platform. Mr Vale says never stand past the white line.\fThere's no white line any more. I don't anyway.");
            break;
        default:
            say("The tunnel breathes out cold air. Inside it's so dark that it sounds dark.");
            break;
        }
        break;

    case MAP_HILL:
        say("A big old oak. Mae says Grandad asked her to marry him under it, in the rain.\fShe says she said yes to get out of the rain. She's smiling when she says it.");
        break;

    case MAP_CLEARING:
        if (obj == OBJ_CLEARING_RING || obj == OBJ_CLEARING_RING2)
            say("A ring of mushrooms, perfectly round, as if someone had drawn it with a compass.");
        else
            say("A stone furred with pale moss. In the shadow, it almost glows.");
        break;
    }
}

/* ================================================================ listening */
void place_listen(uint8_t map, uint8_t zone) BANKED {
    uint8_t d = game.day, i;
    if (zone == 0) return;
    switch (map) {
    case MAP_COTTAGE:
        if (d < 4) say("No wind. The chime is quiet. I waited anyway, in case.");
        break;
    case MAP_VILLAGE:
        if (zone == LISTEN_VILLAGE_SHELTER) {
            if (d == 3 && !(game.found & BEH_RAIN)) {
                compose_hush(60);
                for (i = 0; i < 12; ++i) {
                    snd(CH_ENV, (uint8_t)(0x3B + (i % 3 == 0 ? 0 : 0x10)), (uint8_t)((i % 3 == 0 ? 0x81 : 0x51)), 0, 6, 0, 0, (uint8_t)(i * 14));
                }
                world_wait(120);
                say("The rain on the shelter roof had a rhythm. Not one rhythm. Several, taking turns.");
                say("Big drops from the tree. Small drops from the sky. A slow drip off the corner, keeping time for all of them.");
                if (has_book()) story_discover(BEH_RAIN); else no_book();
            } else if (d != 3) {
                say("Under the shelter roof everything sounds closer, like the world has stepped inside.");
            }
        } else if (zone == LISTEN_VILLAGE_WELL && (game.found & BEH_ECHO)) {
            say("The well hums. Everything in the village goes down into it and comes back up a little rounder.");
        }
        break;
    case MAP_WOOD:
        if (zone == LISTEN_WOOD_REEDS) {
            if (!(game.found & BEH_WIND)) {
                if (!has_book()) { say("The reeds hiss and tick when the air moves."); no_book(); break; }
                for (i = 0; i < 4; ++i) snd(CH_ENV, 0x56, 0x37, 0, 40, 0, 0, (uint8_t)(i * 50));
                fx_notes(DIR_RIGHT, 2);
                world_wait(90);
                say("The reeds sounded different when the wind moved through them.\fNot louder. Different. Like the wind was choosing which ones to play.");
                story_discover(BEH_WIND);
            } else {
                say("The wind is playing the reeds again. It never plays the same ones twice.");
            }
        } else if (zone == LISTEN_WOOD_HIDDEN && d >= 2 && !FLAG(F_PATH_OPEN)) {
            fx_notes(DIR_LEFT, 4);
            compose_motif(MOTIF_ANSWER, 20);
            world_wait(80);
            SET(F_PATH_HEARD);
            say("A faint sound, from the west. Like humming behind a closed door.");
            say("The ferns on the left are moving. Not from wind. There's a way through.");
            SET(F_PATH_OPEN);
            world_patch(PATCH_WOOD_OPEN);
        } else if (zone == LISTEN_WOOD_LOG) {
            say("Something small is moving inside the log. Or the log is settling. Wood does that, Mae says.");
        }
        break;
    case MAP_POND:
        if (!FLAG(F_FIRST_PEBBLE)) say("The water is so still it looks like you could stand on it.");
        else say("The reeds tick. A fish, maybe. Or the pond, thinking.\f(Face the water and press A to sit and listen.)");
        break;
    case MAP_CREEK:
        SET(F_STONES_HINT);
        snd(CH_WATER, 50, 2, WAVE_SOFT, 40, FX_WDECAY, 0, 10);
        snd(CH_WATER, 57, 2, WAVE_SOFT, 40, FX_WDECAY, 0, 50);
        snd(CH_WATER, 55, 2, WAVE_SOFT, 40, FX_WDECAY, 0, 90);
        fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 3);
        world_wait(40);
        fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 1);
        world_wait(40);
        fx_ripples(PT_CREEK_RIPPLE_X, PT_CREEK_RIPPLE_Y, 2);
        world_wait(40);
        say("Listening properly, the water has a song: deep... high... in between.\fDeep... high... in between. Big ring, little ring, middle ring.");
        break;
    case MAP_RAILWAY:
        SET(F_ECHO_HINT);
        say("No train. Wind in the cutting. A crow. The rails ticking as they warm up.\fYou could make a whole song out of what isn't here.");
        break;
    case MAP_HILL:
        if (zone == LISTEN_HILL_MEADOW) {
            if (!(game.found & BEH_INSECTS)) {
                if (!has_book()) { say("Crickets, somewhere in the long grass."); no_book(); break; }
                for (i = 0; i < 6; ++i)
                    snd(CH_LIVING, (uint8_t)(91 + (i & 1) * 3), 0x61, 0, 14, FX_TRILL, 0x43, (uint8_t)(i * 26));
                fx_sparkle(8, 3, 3);
                world_wait(160);
                say("At first, one cricket. Then I noticed it was answering another one.\fThen I couldn't stop hearing them. The whole meadow, taking turns.");
                story_discover(BEH_INSECTS);
            } else {
                say("The crickets leave little gaps for each other. Nobody talks over anybody.");
            }
        } else if (zone == LISTEN_HILL_TOP) {
            SET(F_HILL_VIEW);
            compose_hush(20);
            world_wait(60);
            say("From up here the village looks like it was put down carefully, so as not to wake anything.");
            say("I could hear the creek, and the wood breathing, and somebody's radio.\fAll at once. None of them in charge.");
        }
        break;
    case MAP_CLEARING:
        if (!(game.found & BEH_STILL)) {
            compose_hush(120);
            comp.quiet = 1;
            fx_fireflies(1);
            world_wait(200);
            say("At first I heard nothing.");
            world_wait(90);
            say("Then I heard everything.");
            fx_sparkle(PT_CLEARING_CENTER_X, PT_CLEARING_CENTER_Y, 4);
            world_wait(60);
            say("Lights were lifting out of the moss. Slowly, as if they had been asleep and my stillness woke them.\fI don't know what they were. I didn't move until they'd gone.");
            fx_fireflies(0);
            comp.quiet = 0;
            if (has_book()) story_discover(BEH_STILL); else no_book();
        } else {
            fx_fireflies(1);
            world_wait(120);
            fx_fireflies(0);
        }
        break;
    }
}
