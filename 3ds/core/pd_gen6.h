// Gen 6 (X / Y) live memory, read-only: the C twin of the app's Gen6Reader
// (app/.../n3ds/Gen6Reader.kt, docs/3DS_GAMES_PLAN.md). Every read goes through
// pd_read_fn, so the same code runs on a desktop dump of Azahar's process memory
// (desktop/pd_gen6_preview), on the app's N3dsNative.readMemory, ...
#ifndef PD_GEN6_H
#define PD_GEN6_H

#include "pd_pk6.h"
#include "pd_snapshot.h" // pd_read_fn, enum pd_pocket, struct pd_pocket_items

// The type ids, Gen 6's order (the Fairy type was added in Gen 6).
enum pd_gen6_type {
    PD_TYPE_NORMAL, PD_TYPE_FIGHTING, PD_TYPE_FLYING, PD_TYPE_POISON, PD_TYPE_GROUND, PD_TYPE_ROCK,
    PD_TYPE_BUG, PD_TYPE_GHOST, PD_TYPE_STEEL, PD_TYPE_FIRE, PD_TYPE_WATER, PD_TYPE_GRASS,
    PD_TYPE_ELECTRIC, PD_TYPE_PSYCHIC, PD_TYPE_ICE, PD_TYPE_DRAGON, PD_TYPE_DARK, PD_TYPE_FAIRY,
    PD_GEN6_TYPE_COUNT,
};

extern const uint8_t pd_gen6_type_chart[PD_GEN6_TYPE_COUNT][PD_GEN6_TYPE_COUNT]; // pd_gen6_gen.c
// 0 immune, 1 not very effective, 2 neutral, 4 super effective.
int pd_gen6_effectiveness(int attackType, int defendType);

#define PD_GEN6_PARTY 6
#define PD_GEN6_POCKETS 6
#define PD_GEN6_BATTLE_UI_BYTES 0x18
#define PD_GEN6_BATTLE_MON_BYTES 0x244

struct pd_gen6_pocket {
    uint32_t off; // from the bag root
    int capacity;
    enum pd_pocket pocket;
};

// Where a Gen 6 game keeps what the companion reads. Per build and language; the
// values below are the proven ones (Gen6Reader.kt's POKEMON_X_USA).
struct pd_gen6_config {
    uint32_t party, partyStride, partyStatsAt; // the six live slots
    uint32_t myStatus, languageAt;
    uint32_t zone;
    uint32_t bag;
    struct pd_gen6_pocket pockets[PD_GEN6_POCKETS];
    int pocketCount;
    uint32_t battleUi, battleMovesAt;
    int battlePanelPlayer, battlePanelFoe; // the battler records' offsets in the command panel
};

// Pokemon X (USA); Y runs X's code and memory, every address X's.
extern const struct pd_gen6_config pd_gen6_xy_usa;

struct pd_gen6_battle_move {
    int id, pp, maxPp;
};

struct pd_gen6_battle_mon {
    bool present;
    int species, level, hp, maxHp;
    struct pd_gen6_battle_move moves[PD_PK6_MOVES];
};

struct pd_gen6_snapshot {
    bool valid;
    int partyCount;
    struct pd_pk6 party[PD_GEN6_PARTY];
    int language, zone; // -1 = not read
    bool bagOk;
    struct pd_pocket_items bag[PD_POCKET_COUNT];
    enum pd_battle_state battleState;
    struct pd_gen6_battle_mon battlerPlayer, battlerFoe;
};

void pd_gen6_snapshot_read(struct pd_gen6_snapshot* s, const struct pd_gen6_config* cfg, pd_read_fn read, void* ctx);

#endif
