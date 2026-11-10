/* MOSS — the people of the village, and what they say each day. */
#pragma bank 11
#include <gb/gb.h>
#include "core.h"
#include "world.h"
#include "story.h"

void story_sleep(void) BANKED;

static uint8_t turn;   /* rotates optional lines */

static void talk_mae(void) {
    uint8_t d = game.day, f = game.found;
    face_player(WHO_MAE);
    world_wait(2);
    if (d == 1) {
        if (!FLAG(F_FIRST_PEBBLE)) {
            if (turn & 1) say("Mae: The pond? North, through the village and into the wood. Keep to the path.\fYou can't miss it. Well. You can. Your mother did, once.");
            else say("Mae: Go on, then. Supper's not till dark. Mind the nettles.");
        } else {
            say("Mae: Bed's made up. The one by the window, same as always.\fSleep when you're ready. The notebook will keep.");
        }
    } else if (d == 2) {
        if (!(f & BEH_WOOD)) say("Mae: Iris from the blue house has been asking after you. She'll be in the wood, I'd guess.\fI told her you'd grown. She said she'd judge for herself.");
        else if (turn & 1) say("Mae: Write it down, whatever it is. You think you'll remember. You won't, quite.");
        else say("Mae: Your grandad would have liked you. He'd sit on that hill for hours doing nothing. Said it wasn't nothing.");
    } else if (d == 3) {
        if (turn & 1) say("Mae: Rain! Good. The garden's been gasping.\fMr Vale will be up at the station. He goes in all weathers. Stubborn as a gatepost.");
        else say("Mae: Listen to it on the roof. Your mother used to say the cottage was clapping.");
    } else if (d == 4) {
        if (!(f & BEH_MEMORY)) {
            say("Mae: Don't mind the boxes. They look worse than they are.\fCould you fetch the wind chime down from the window? My arms don't go up like they did.");
        } else if (!FLAG(F_ANSWER)) {
            say("Mae: Go on up to your pond. I'll keep supper warm. Take as long as you like.");
        } else {
            say("Mae: You're quiet tonight. That's all right. Quiet's allowed.\fBed, I think. Big day tomorrow.");
        }
    } else {
        say("Mae: The car's coming at eight.\fGo on. Go and say goodbye. I'll finish up here.");
    }
    ++turn;
}

static void talk_iris(void) {
    uint8_t d = game.day;
    face_player(WHO_IRIS);
    world_wait(2);
    if (d == 2) {
        if (!FLAG(F_MET_IRIS)) {
            SET(F_MET_IRIS);
            compose_motif(MOTIF_IRIS, 0);
            say("Iris: Oh! You're Mae's grandson. I'm Iris. I live in the blue house.\fWell, it's grey. But it used to be blue, so it still counts.");
            say("Iris: Do you want to hear something? Knock on that log. Go on. With your knuckles.");
        } else if (!(game.found & BEH_WOOD)) {
            say("Iris: The log! Knock on it. It doesn't bite. Probably.");
        } else if (!(game.found & BEH_WATER)) {
            SET(F_STONES_HINT);
            say("Iris: There are three stones at the creek that sing if you knock them in the right order.\fGrandpa Hollis says they don't. He's wrong. The creek's east of the village.");
        } else if (turn & 1) {
            say("Iris: I come here every day. It's never the same twice. Last week there was a frog in the log.");
        } else {
            say("Iris: You listen funny. Like you're reading. I don't mean it in a bad way.");
        }
    } else if (d == 3) {
        if (!(game.found & BEH_WATER)) {
            say("Iris: Still haven't got the stones? Watch the water upstream. It makes rings. Big, little, middle.");
        } else if (!FLAG(F_IRIS_STORY)) {
            SET(F_IRIS_STORY);
            say("Iris: I like the creek best in the rain. It gets louder, like it's showing off.\fAre you sad about leaving? I'd be sad. And a bit excited. I've never left anywhere.");
        } else {
            say("Iris: The stones were singing all morning. I think they liked being listened to.");
        }
    } else if (d == 4) {
        if (!FLAG(F_IRIS_GOODBYE)) {
            SET(F_IRIS_GOODBYE);
            say("Iris: Mae says you're going tomorrow.\fI'll look after the pond. Not that it needs looking after. I'll just... look at it. That's a kind of looking after.");
            say("Iris: Write to me. If you can't spell a sound, draw it.");
        } else {
            say("Iris: Go on. It's your last evening. Don't waste it talking to me.");
        }
    }
    ++turn;
}

static void talk_hollis(void) {
    uint8_t d = game.day;
    face_player(WHO_HOLLIS);
    world_wait(2);
    if (cur_map == MAP_HILL) {
        say("Hollis: Shh. Lie still in the long grass. Give them a minute.\fThey stop when they hear you, and start again when they forget you.");
        return;
    }
    if (d == 1) {
        if (!FLAG(F_MET_HOLLIS)) {
            SET(F_MET_HOLLIS);
            say("Hollis: Ah. Mae's lad. You've got her ears, you know. Good big ones. Made for listening.");
        } else {
            say("Hollis: These beans don't know Mae's moving. They'll come up next year just the same.");
        }
    } else if (d == 2) {
        if (!(game.found & BEH_INSECTS))
            say("Hollis: Go up the hill, west of here, and lie down in the long grass.\fYou'll hear the whole meadow arguing. I'll be up there myself after my tea.");
        else
            say("Hollis: Heard them, did you? Crickets don't sing for us. We just happen to be in the room.");
    } else if (d == 3) {
        if (!(game.found & BEH_RAIN)) {
            SET(F_SHELTER_TALK);
            say("Hollis: Rain flattens the beans and I grumble. Then they're twice as tall by Thursday.\fStand under the shelter there a while. Listen to the roof. It's better than the wireless.");
        } else if (!FLAG(F_HOLLIS_STORY)) {
            SET(F_HOLLIS_STORY);
            say("Hollis: There'll be new folk in Mae's cottage by autumn.\fDoesn't undo anything that happened in it. Change isn't the same as ending, lad.");
        } else {
            say("Hollis: Rain's easing by morning. It always says so, if you know how to hear it.");
        }
    } else {
        if (!FLAG(F_HOLLIS_GOODBYE)) {
            SET(F_HOLLIS_GOODBYE);
            say("Hollis: Seasons don't end, lad. They hand over.\fCome back and see the beans sometime. They won't remember you. I will.");
        } else {
            say("Hollis: Go on. The light's good this time of day.");
        }
    }
    ++turn;
}

static void talk_ada(void) {
    uint8_t d = game.day;
    face_player(WHO_ADA);
    world_wait(2);
    if (!FLAG(F_MET_ADA)) {
        SET(F_MET_ADA);
        say("Ada: Hello, love! You'll be Mae's grandson. She's talked about this visit since Easter.\fHave a mint. Don't tell your gran I'm feeding you.");
        return;
    }
    if (d == 1) say("Ada: The shop shuts at five, but I don't. Knock if you need anything.");
    else if (d == 2) say("Ada: Quiet day. Quiet's good for a shop on a Tuesday. Bad on a Saturday.");
    else if (d == 3) {
        if (!FLAG(F_ADA_STORY)) {
            SET(F_ADA_STORY);
            say("Ada: Mae's given me her teapot. Says she won't need two where she's going.\fI said everybody needs two teapots. One for good days. One for the others.");
        } else say("Ada: Mind the puddles. The big one by the well's deeper than it looks. Like most people.");
    } else {
        if (!FLAG(F_ADA_GOODBYE)) {
            SET(F_ADA_GOODBYE);
            say("Ada: Tomorrow, is it? I'll wave from the window. Look for me.\fYou'll always be Mae's grandson here, you know. That doesn't move house.");
        } else say("Ada: Go on, love. Last evening. Spend it well.");
    }
}

static void talk_vale(void) {
    uint8_t d = game.day;
    face_player(WHO_VALE);
    world_wait(2);
    if (!FLAG(F_MET_VALE)) {
        SET(F_MET_VALE);
        compose_motif(MOTIF_TRAIN, 10);
        say("Vale: Afternoon. If you're waiting for the four-ten, you'll wait a while.\fLast one came through when your mother was smaller than you.");
        say("Vale: I still come up. Habit. A habit's just loyalty that's forgotten its reason.");
        return;
    }
    if (!FLAG(F_BELL_RUNG)) {
        if (d >= 3) say("Vale: That bell used to go when the train came round the bend. Two notes, it had.\fGo on. Ring it. Nobody's going to complain. Not any more.");
        else say("Vale: Come back when it's raining. The cutting sounds different in the rain.");
        return;
    }
    if (!FLAG(F_VALE_STORY)) {
        SET(F_VALE_STORY);
        say("Vale: Funny thing. Forty years on this platform and I never once noticed the train.\fOnly noticed it when it stopped coming. You hear a thing best by its gap.");
        return;
    }
    if (d == 4 && !FLAG(F_VALE_GOODBYE)) {
        SET(F_VALE_GOODBYE);
        say("Vale: Leaving tomorrow? Then ring it once more for me. And mind how you go, lad.");
        return;
    }
    say("Vale: It used to answer the train. Now it only answers itself.\fThat's not nothing.");
}

static void talk_cat(void) {
    if (cur_map == MAP_WOOD && game.day == 3 && !FLAG(F_PATH_OPEN)) {
        SET(F_CAT_LED);
        say("The cat looked at the ferns. Then at me. Then at the ferns again.");
        npc_face(WHO_CAT, DIR_LEFT);
        world_wait(30);
        say("Behind the ferns, very faintly, something was humming.");
        fx_notes(DIR_LEFT, 3);
        world_wait(40);
        SET(F_PATH_HEARD);
        SET(F_PATH_OPEN);
        world_patch(PATCH_WOOD_OPEN);
        npc_walk(WHO_CAT, DIR_UP, 1);
        npc_remove(WHO_CAT);
        say("The ferns weren't a wall. They were a door.");
        return;
    }
    if (cur_map == MAP_POND) {
        say("The cat was watching the water. It didn't look at me. That felt like a kind of welcome.");
        return;
    }
    switch (turn++ % 3) {
    case 0: say("The cat ignored me completely."); break;
    case 1: say("The cat let me stroke it exactly once, then walked off as if it had been my idea."); break;
    default: say("Nobody seems to own the cat. Ada says the cat owns the village."); break;
    }
}

void talk_person(uint8_t who) BANKED {
    switch (who) {
    case WHO_MAE: talk_mae(); break;
    case WHO_IRIS: talk_iris(); break;
    case WHO_HOLLIS: talk_hollis(); break;
    case WHO_ADA: talk_ada(); break;
    case WHO_VALE: talk_vale(); break;
    case WHO_CAT: talk_cat(); break;
    default: say("Something was there. Then it was only reeds."); break;
    }
}
