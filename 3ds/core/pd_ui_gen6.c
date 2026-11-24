// pd_ui_gen6.c - see pd_ui_gen6.h. Read-only: nothing here writes game memory.
#include "pd_ui_gen6.h"

#include <stdio.h>
#include <string.h>

#define BG 0xFF182038
#define PANEL 0xFF232B4D
#define PANEL_SEL 0xFF3A4A7A
#define INK 0xFFF0F0F0
#define DIM 0xFF9AA0C0
#define ACCENT 0xFF6CC070
#define WARN 0xFFE0B040
#define BAD 0xFFE06060

static void hp_bar(struct pd_canvas* c, int x, int y, int w, int hp, int maxHp) {
    pd_fill(c, x, y, w, 5, 0xFF404860);
    int frac = maxHp > 0 ? hp * w / maxHp : 0;
    uint32_t col = hp * 2 > maxHp ? ACCENT : (hp * 4 > maxHp ? WARN : BAD);
    if (frac > 0) pd_fill(c, x, y, frac, 5, col);
}

static const char* status_name(uint32_t st) {
    if (st & 0x7) return "SLP";
    if (st & 0x8) return "PSN";
    if (st & 0x10) return "BRN";
    if (st & 0x20) return "FRZ";
    if (st & 0x40) return "PAR";
    return NULL;
}

void pd_ui_gen6_party(struct pd_canvas* c, const struct pd_gen6_snapshot* s, int selected) {
    pd_fill(c, 0, 0, c->w, c->h, BG);
    char line[96];
    snprintf(line, sizeof line, "X/Y companion (read-only)   zone %d  lang %d  battle %d", s->zone, s->language,
             (int) s->battleState);
    pd_text(c, 8, 6, line, DIM, 0xFF000000);

    int listW = 190;
    pd_fill(c, 4, 24, listW, c->h - 28, PANEL);
    int slotH = (c->h - 32) / PD_GEN6_PARTY;
    for (int i = 0; i < s->partyCount && i < PD_GEN6_PARTY; i++) {
        const struct pd_pk6* m = &s->party[i];
        int y = 28 + i * slotH;
        if (i == selected) pd_fill(c, 6, y, listW - 4, slotH - 3, PANEL_SEL);
        snprintf(line, sizeof line, "%s%s%s", m->egg ? "EGG " : "", m->shiny ? "*" : "", m->nickname);
        pd_text(c, 12, y + 2, line, i == selected ? INK : DIM, 0xFF000000);
        snprintf(line, sizeof line, "#%d Lv%d %s", m->species, m->level, status_name(m->status) ? status_name(m->status) : "");
        pd_text(c, 12, y + 18, line, DIM, 0xFF000000);
        char hp[32];
        snprintf(hp, sizeof hp, "%d/%d", m->hp, m->maxHp);
        pd_text(c, 12, y + 34, hp, INK, 0xFF000000);
        hp_bar(c, 60, y + 36, listW - 70, m->hp, m->maxHp);
    }

    int dx = listW + 10, dw = c->w - dx - 6;
    pd_fill(c, dx, 24, dw, c->h - 28, PANEL);
    if (selected < 0 || selected >= s->partyCount) {
        pd_text(c, dx + 10, 34, "no Pokemon selected", DIM, 0xFF000000);
        return;
    }
    const struct pd_pk6* m = &s->party[selected];
    int y = 32, lh = 17;
    snprintf(line, sizeof line, "#%d form %d %s%s", m->species, m->form,
             m->female ? "F" : (m->genderless ? "-" : "M"), m->shiny ? " shiny" : "");
    pd_text(c, dx + 10, y, line, INK, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "item %d   ability %d   nature %d", m->heldItem, m->ability, m->nature);
    pd_text(c, dx + 10, y, line, DIM, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "exp %u   pid %08x", m->exp, m->pid);
    pd_text(c, dx + 10, y, line, DIM, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "moves %d/%d %d/%d %d/%d %d/%d", m->moves[0], m->pp[0], m->moves[1], m->pp[1], m->moves[2],
             m->pp[2], m->moves[3], m->pp[3]);
    pd_text(c, dx + 10, y, line, DIM, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "IVs %d/%d/%d/%d/%d/%d", m->ivs[0], m->ivs[1], m->ivs[2], m->ivs[3], m->ivs[4], m->ivs[5]);
    pd_text(c, dx + 10, y, line, DIM, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "EVs %d/%d/%d/%d/%d/%d", m->evs[0], m->evs[1], m->evs[2], m->evs[3], m->evs[4], m->evs[5]);
    pd_text(c, dx + 10, y, line, DIM, 0xFF000000);
    y += lh;
    snprintf(line, sizeof line, "stats %d/%d/%d/%d/%d", m->stats[0], m->stats[1], m->stats[2], m->stats[3], m->stats[4]);
    pd_text(c, dx + 10, y, line, ACCENT, 0xFF000000);
    y += lh + 4;
    if (s->battleState != PD_BATTLE_NONE) {
        snprintf(line, sizeof line, "battle %d: foe #%d Lv%d %d/%d", (int) s->battleState,
                 s->battlerFoe.species, s->battlerFoe.level, s->battlerFoe.hp, s->battlerFoe.maxHp);
        pd_text(c, dx + 10, y, line, WARN, 0xFF000000);
    }
}
