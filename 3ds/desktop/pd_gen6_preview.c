// pd_gen6_preview - the Gen 6 (X / Y) read-only companion's desktop harness: it
// reads a RAM dump of Azahar's emulated process (native-capture/n3ds_dump's
// `dump <dir>`, one <base-hex>.bin per mapped region) or a save's main file, and
// prints what pd_gen6 decodes. No emulator, no ROM, no save in the repo.
//
//   pd_gen6_preview dump <dir>                 RAM regions from n3ds_dump's dump
//   pd_gen6_preview save <main> [partyOffHex]  a save's main (0x65600, FEEB footer);
//                                              partyOffHex only once the party's
//                                              offset is proven on a fixture
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pd_gen6.h"
#include "../core/pd_save6.h"

#define MAX_REGIONS 64

struct region {
    uint32_t base;
    uint8_t* data;
    size_t len;
};

static struct region regions[MAX_REGIONS];
static int regionCount;

static bool read_dump(void* ctx, uint32_t addr, void* out, size_t len) {
    (void) ctx;
    for (int i = 0; i < regionCount; i++) {
        struct region* r = &regions[i];
        if (addr >= r->base && (uint64_t) addr + len <= (uint64_t) r->base + r->len) {
            memcpy(out, r->data + (addr - r->base), len);
            return true;
        }
    }
    return false;
}

static int load_dump(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return -1;
    struct dirent* e;
    while ((e = readdir(d)) && regionCount < MAX_REGIONS) {
        char* dot = strrchr(e->d_name, '.');
        if (!dot || strcmp(dot, ".bin") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n <= 0) {
            fclose(f);
            continue;
        }
        uint8_t* data = malloc((size_t) n);
        if (fread(data, 1, (size_t) n, f) != (size_t) n) {
            free(data);
            fclose(f);
            continue;
        }
        fclose(f);
        regions[regionCount].base = (uint32_t) strtoul(e->d_name, NULL, 16);
        regions[regionCount].data = data;
        regions[regionCount].len = (size_t) n;
        regionCount++;
    }
    closedir(d);
    return regionCount;
}

static void print_mon(const struct pd_pk6* m) {
    printf("  species %d form %d \"%s\" Lv%d %d/%d HP status %u %s%s%s\n", m->species, m->form, m->nickname,
           m->level, m->hp, m->maxHp, m->status, m->shiny ? "shiny " : "", m->egg ? "egg " : "",
           m->female ? "female" : (m->genderless ? "genderless" : "male"));
    printf("    item %d ability %d nature %d exp %u pid %08x\n", m->heldItem, m->ability, m->nature, m->exp, m->pid);
    printf("    moves %d/%d, %d/%d, %d/%d, %d/%d\n", m->moves[0], m->pp[0], m->moves[1], m->pp[1],
           m->moves[2], m->pp[2], m->moves[3], m->pp[3]);
    printf("    IVs %d/%d/%d/%d/%d/%d  EVs %d/%d/%d/%d/%d/%d  stats %d/%d/%d/%d/%d\n",
           m->ivs[0], m->ivs[1], m->ivs[2], m->ivs[3], m->ivs[4], m->ivs[5],
           m->evs[0], m->evs[1], m->evs[2], m->evs[3], m->evs[4], m->evs[5],
           m->stats[0], m->stats[1], m->stats[2], m->stats[3], m->stats[4]);
}

static const char* battle_state_name(enum pd_battle_state b) {
    switch (b) {
        case PD_BATTLE_ACTION: return "action select";
        case PD_BATTLE_MOVE: return "move select";
        case PD_BATTLE_BAG_OPEN: return "bag open";
        case PD_BATTLE_PARTY_OPEN: return "party open";
        default: return "none";
    }
}

static void print_battle_mon(const char* who, const struct pd_gen6_battle_mon* m) {
    if (!m->present) return;
    printf("  %s: species %d Lv%d %d/%d HP moves %d(%d/%d) %d(%d/%d) %d(%d/%d) %d(%d/%d)\n", who, m->species,
           m->level, m->hp, m->maxHp,
           m->moves[0].id, m->moves[0].pp, m->moves[0].maxPp, m->moves[1].id, m->moves[1].pp, m->moves[1].maxPp,
           m->moves[2].id, m->moves[2].pp, m->moves[2].maxPp, m->moves[3].id, m->moves[3].pp, m->moves[3].maxPp);
}

static int dump_mode(const char* dir) {
    int n = load_dump(dir);
    if (n <= 0) {
        fprintf(stderr, "pd_gen6_preview: no <base-hex>.bin regions in %s (n3ds_dump's `dump %s`)\n", dir, dir);
        return 1;
    }
    printf("%d regions\n", n);
    struct pd_gen6_snapshot s;
    pd_gen6_snapshot_read(&s, &pd_gen6_xy_usa, read_dump, NULL);
    printf("party %d, language %d, zone %d, bag %s, battle %s\n", s.partyCount, s.language, s.zone,
           s.bagOk ? "read" : "unread", battle_state_name(s.battleState));
    for (int i = 0; i < s.partyCount; i++) print_mon(&s.party[i]);
    for (int p = 0; p < PD_POCKET_COUNT; p++)
        if (s.bag[p].count) printf("  pocket %d: %d items, first id %d x%d\n", p, s.bag[p].count, s.bag[p].items[0].id, s.bag[p].items[0].quantity);
    print_battle_mon("player", &s.battlerPlayer);
    print_battle_mon("foe", &s.battlerFoe);
    printf("(names shown as ids: the cart's RomFS text, Gen6Text's GARC a/0/7/<n>, lands with the text reader)\n");
    return 0;
}

// Scans a main file for runs of consecutive checksum-valid PK6s (the party and
// the PC boxes both qualify): prints the start of each run of >= 4 valid slots
// holding at least one real species. The party is the first such run.
static int scan_mode(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "pd_gen6_preview: can't open %s\n", path);
        return 1;
    }
    static uint8_t buf[PD_SAVE6_SIZE + 1];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n != PD_SAVE6_SIZE) {
        fprintf(stderr, "pd_gen6_preview: %s: 0x%zx bytes, not a Gen 6 main (0x65600)\n", path, n);
        return 1;
    }
    // One pass of run flags, then print only each run's first offset (the party
    // is the first run holding real species; the PC boxes follow as more runs).
    static bool run[(PD_SAVE6_SIZE / 4) + 1];
    static int speciesAt[(PD_SAVE6_SIZE / 4) + 1][PD_SAVE6_PARTY];
    for (size_t off = 0; off + (size_t) PD_SAVE6_PARTY * PD_PK6_STORED <= n; off += 4) {
        int valid = 0, real = 0;
        for (int i = 0; i < PD_SAVE6_PARTY; i++) {
            struct pd_pk6 m;
            if (!pd_pk6_decode(buf + off + (size_t) i * PD_PK6_STORED, PD_PK6_STORED, &m)) break;
            if (m.species > 721) break;
            speciesAt[off / 4][i] = m.species;
            valid++;
            if (m.species) real++;
        }
        run[off / 4] = valid >= 4 && real >= 1;
    }
    int found = 0;
    for (size_t off = 0; off + (size_t) PD_SAVE6_PARTY * PD_PK6_STORED <= n; off += 4) {
        size_t k = off / 4;
        bool prev = off >= PD_PK6_STORED && run[k - PD_PK6_STORED / 4];
        if (run[k] && !prev) {
            printf("run at 0x%zx: species", off);
            for (int i = 0; i < PD_SAVE6_PARTY; i++) printf(" %d", speciesAt[k][i]);
            printf("%s\n", found == 0 ? "  <- first run: the party" : "");
            found++;
        }
    }
    if (!found) printf("no party-like PK6 run found\n");
    return 0;
}

static int save_mode(const char* path, const char* partyOff) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "pd_gen6_preview: can't open %s\n", path);
        return 1;
    }
    static uint8_t buf[PD_SAVE6_SIZE + 1];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    struct pd_save6_layout layout = pd_save6_xy;
    if (partyOff) layout.partyOff = (uint32_t) strtoul(partyOff, NULL, 0);
    struct pd_save6 s;
    const char* why = pd_save6_parse(&s, buf, n, &layout);
    if (why) {
        fprintf(stderr, "pd_gen6_preview: %s: %s\n", path, why);
        return 1;
    }
    printf("save ok (0x%zx bytes, FEEB footer, party at 0x%x)\n", s.len, layout.partyOff);
    if (layout.partyOff == 0) {
        printf("party offset not mapped yet: pass it as the third argument once proven on a fixture\n");
        return 0;
    }
    for (int i = 0; i < PD_SAVE6_PARTY; i++) {
        struct pd_pk6 m;
        if (pd_save6_party_mon(&s, i, &m)) print_mon(&m);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && strcmp(argv[1], "dump") == 0) return dump_mode(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "save") == 0) return save_mode(argv[2], argc >= 4 ? argv[3] : NULL);
    if (argc >= 3 && strcmp(argv[1], "scan") == 0) return scan_mode(argv[2]);
    fprintf(stderr, "usage: %s dump <dir> | save <main> [partyOffHex] | scan <main>\n", argv[0]);
    return 2;
}
