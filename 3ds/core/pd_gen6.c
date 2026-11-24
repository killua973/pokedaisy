// pd_gen6.c - see pd_gen6.h. The addresses are Gen6Reader.kt's POKEMON_X_USA
// (proven headless with native-capture/n3ds_dump and on device, docs/3DS_GAMES_PLAN.md).
#include "pd_gen6.h"

#include <string.h>

int pd_gen6_effectiveness(int attackType, int defendType) {
    if (attackType < 0 || attackType >= PD_GEN6_TYPE_COUNT || defendType < 0 || defendType >= PD_GEN6_TYPE_COUNT) return 2;
    return pd_gen6_type_chart[attackType][defendType];
}

const struct pd_gen6_config pd_gen6_xy_usa = {
    .party = 0x08CE1CE8,
    .partyStride = 0x1E4,
    .partyStatsAt = 0x158,
    .myStatus = 0x08C79C2C,
    .languageAt = 0x2D,
    .zone = 0x08C6709E,
    .bag = 0x08C67154,
    .pockets = {
        { 0x400, 400, PD_POCKET_ITEMS },
        { 0xA40, 96, PD_POCKET_KEY },
        { 0xBC0, 106, PD_POCKET_TMHM },
        { 0xD68, 64, PD_POCKET_MEDICINE },
        { 0xE68, 64, PD_POCKET_BERRIES },
    },
    .pocketCount = 5,
    .battleUi = 0x08515980,
    .battleMovesAt = 0x116,
    .battlePanelPlayer = 0xDC,
    .battlePanelFoe = 0xE4,
};

// A pointer into Gen 6's heap (Sun / Moon keep theirs at 0x30000000 up).
static bool in_heap(uint32_t a) {
    return a >= 0x08000000 && a < 0x10000000;
}

static uint16_t get16(const uint8_t* p, int off) {
    return (uint16_t) (p[off] | (p[off + 1] << 8));
}

static uint32_t get32(const uint8_t* p, int off) {
    return (uint32_t) (p[off] | (p[off + 1] << 8) | ((uint32_t) p[off + 2] << 16) | ((uint32_t) p[off + 3] << 24));
}

static void read_party(struct pd_gen6_snapshot* s, const struct pd_gen6_config* cfg, pd_read_fn read, void* ctx) {
    uint8_t buf[PD_GEN6_PARTY * 0x200];
    size_t len = (size_t) cfg->partyStride * PD_GEN6_PARTY;
    if (len > sizeof buf || cfg->partyStatsAt + PD_PK6_PARTY - PD_PK6_STORED > cfg->partyStride) return;
    if (!read(ctx, cfg->party, buf, len)) return;
    for (int i = 0; i < PD_GEN6_PARTY; i++) {
        uint8_t raw[PD_PK6_PARTY];
        memcpy(raw, buf + i * cfg->partyStride, PD_PK6_STORED);
        memcpy(raw + PD_PK6_STORED, buf + i * cfg->partyStride + cfg->partyStatsAt, PD_PK6_PARTY - PD_PK6_STORED);
        if (pd_pk6_decode(raw, PD_PK6_PARTY, &s->party[s->partyCount])) s->partyCount++;
    }
    s->valid = s->partyCount > 0;
}

static void read_bag(struct pd_gen6_snapshot* s, const struct pd_gen6_config* cfg, pd_read_fn read, void* ctx) {
    bool any = false;
    for (int p = 0; p < cfg->pocketCount; p++) {
        const struct pd_gen6_pocket* pk = &cfg->pockets[p];
        if (pk->pocket < 0 || pk->pocket >= PD_POCKET_COUNT || pk->capacity > PD_POCKET_SLOTS) continue;
        uint8_t buf[PD_POCKET_SLOTS * 4];
        if ((size_t) pk->capacity * 4 > sizeof buf) continue;
        if (!read(ctx, cfg->bag + pk->off, buf, (size_t) pk->capacity * 4)) continue;
        any = true;
        struct pd_pocket_items* out = &s->bag[pk->pocket];
        out->count = 0;
        for (int i = 0; i < pk->capacity; i++) {
            uint16_t id = get16(buf, i * 4);
            if (id == 0) break;
            out->items[out->count].id = id;
            out->items[out->count].quantity = get16(buf, i * 4 + 2);
            out->count++;
        }
    }
    s->bagOk = any;
}

static void read_battle(struct pd_gen6_snapshot* s, const struct pd_gen6_config* cfg, pd_read_fn read, void* ctx) {
    s->battleState = PD_BATTLE_NONE;
    uint8_t ui[PD_GEN6_BATTLE_UI_BYTES];
    if (!read(ctx, cfg->battleUi, ui, sizeof ui)) return;
    switch (ui[0x17]) {
        case 1: s->battleState = PD_BATTLE_ACTION; break;
        case 2: s->battleState = PD_BATTLE_MOVE; break;
        default:
            if (get32(ui, 0) == 0 && get32(ui, 4) == 0 && get32(ui, 0xC) == 0) {
                if (get32(ui, 0x10) == 1) s->battleState = PD_BATTLE_BAG_OPEN;
                else if (get32(ui, 0x10) == 2) s->battleState = PD_BATTLE_PARTY_OPEN;
            }
            break;
    }
    if (s->battleState == PD_BATTLE_NONE) return;
    uint32_t panel = get32(ui, 0);
    if (!in_heap(panel)) return;
    uint8_t ptrs[0xE8];
    if (!read(ctx, panel + cfg->battlePanelPlayer, ptrs, cfg->battlePanelFoe + 4 - cfg->battlePanelPlayer)) return;
    uint32_t playerRec = get32(ptrs, 0);
    uint32_t foeRec = get32(ptrs, cfg->battlePanelFoe - cfg->battlePanelPlayer);
    int movesBytes = cfg->battleMovesAt + 4 * 14;
    if (movesBytes > PD_GEN6_BATTLE_MON_BYTES) return;
    uint8_t rec[PD_GEN6_BATTLE_MON_BYTES];
    struct pd_gen6_battle_mon* mons[2] = { &s->battlerPlayer, &s->battlerFoe };
    uint32_t recs[2] = { playerRec, foeRec };
    for (int b = 0; b < 2; b++) {
        struct pd_gen6_battle_mon* m = mons[b];
        memset(m, 0, sizeof *m);
        if (!in_heap(recs[b]) || !read(ctx, recs[b], rec, (size_t) movesBytes)) continue;
        m->species = get16(rec, 0xC);
        if (m->species == 0 || m->species > 809) continue;
        m->maxHp = get16(rec, 0xE);
        m->hp = get16(rec, 0x10);
        m->level = rec[0x18];
        m->present = true;
        for (int i = 0; i < PD_PK6_MOVES; i++) {
            int o = cfg->battleMovesAt + i * 14;
            m->moves[i].id = get16(rec, o);
            m->moves[i].pp = rec[o + 2];
            m->moves[i].maxPp = rec[o + 3];
        }
    }
}

void pd_gen6_snapshot_read(struct pd_gen6_snapshot* s, const struct pd_gen6_config* cfg, pd_read_fn read, void* ctx) {
    memset(s, 0, sizeof *s);
    s->language = s->zone = -1;
    s->battleState = PD_BATTLE_NONE;
    read_party(s, cfg, read, ctx);
    uint8_t b;
    if (read(ctx, cfg->myStatus + cfg->languageAt, &b, 1)) s->language = b;
    uint8_t z[2];
    if (read(ctx, cfg->zone, z, 2)) s->zone = get16(z, 0);
    read_bag(s, cfg, read, ctx);
    read_battle(s, cfg, read, ctx);
}
