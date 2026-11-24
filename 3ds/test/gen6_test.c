// pd_pk6 / pd_gen6 / pd_save6 on synthetic records: a PK6 encoded the game's
// way (pd_pk6_encode) must decode back field for field, whatever the encryption
// constant (so every block order), a broken checksum must fail, the Gen 6 type
// chart's cells must match (Fairy added, Steel's Dark / Ghost resistances gone),
// and a synthetic "main" save must parse (0x65600 + the 0xFEEB footer) and read
// its party back. Real captures land with native-capture/n3ds_dump; none are in
// the repo.
//
//   build/pd_gen6_test
#include <stdio.h>
#include <string.h>

#include "../core/pd_pk6.h"
#include "../core/pd_gen6.h"
#include "../core/pd_save6.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static struct pd_pk6 sample(uint32_t ec) {
    struct pd_pk6 m;
    memset(&m, 0, sizeof m);
    m.ec = ec;
    m.species = 658; // Chespin
    m.form = 1;
    m.heldItem = 537;
    m.tid = 12345;
    m.sid = 54321;
    m.exp = 1059860;
    m.ability = 66;
    m.pid = 0xCAFEBABE;
    m.nature = 3;
    strcpy(m.nickname, "CHESPIN");
    m.moves[0] = 392; m.moves[1] = 45; m.moves[2] = 73; m.moves[3] = 0;
    m.pp[0] = 10; m.pp[1] = 35; m.pp[2] = 15; m.pp[3] = 0;
    for (int i = 0; i < 6; i++) m.ivs[i] = (uint8_t) (31 - i);
    for (int i = 0; i < 6; i++) m.evs[i] = (uint8_t) (i * 40);
    m.female = true;
    m.egg = false;
    m.status = 0;
    m.level = 100;
    m.hp = 219;
    m.maxHp = 300;
    m.stats[0] = 250; m.stats[1] = 180; m.stats[2] = 200; m.stats[3] = 190; m.stats[4] = 170;
    return m;
}

static void round_trip(uint32_t ec) {
    struct pd_pk6 in = sample(ec), out;
    uint8_t raw[PD_PK6_PARTY];
    pd_pk6_encode(&in, PD_PK6_PARTY, raw);
    CHECK(pd_pk6_decode(raw, PD_PK6_PARTY, &out), "ec %08x: decode failed", ec);
    CHECK(out.ec == ec && out.species == 658 && out.form == 1 && out.heldItem == 537, "ec %08x: header fields", ec);
    CHECK(out.tid == 12345 && out.sid == 54321 && out.exp == 1059860 && out.ability == 66, "ec %08x: ot / exp / ability", ec);
    CHECK(out.pid == 0xCAFEBABE && out.nature == 3 && out.female && !out.genderless && !out.egg, "ec %08x: pid / nature / flags", ec);
    CHECK(strcmp(out.nickname, "CHESPIN") == 0, "ec %08x: nickname '%s'", ec, out.nickname);
    CHECK(out.moves[0] == 392 && out.moves[1] == 45 && out.moves[2] == 73 && out.moves[3] == 0, "ec %08x: moves", ec);
    CHECK(out.pp[0] == 10 && out.pp[2] == 15, "ec %08x: pp", ec);
    for (int i = 0; i < 6; i++)
        CHECK(out.ivs[i] == 31 - i && out.evs[i] == i * 40, "ec %08x: iv/ev %d", ec, i);
    CHECK(out.level == 100 && out.hp == 219 && out.maxHp == 300 && out.status == 0, "ec %08x: party stats", ec);
    CHECK(out.stats[0] == 250 && out.stats[4] == 170, "ec %08x: stats", ec);
    CHECK(out.shiny == (((12345 ^ 54321 ^ 0xCAFE ^ 0xBABE) & 0xFFFF) < 16), "ec %08x: shiny", ec);
}

int main(void) {
    // Several encryption constants, so several block orders round-trip.
    round_trip(0x00000000);
    round_trip(0x00000001);
    round_trip(0x12345678);
    round_trip(0xFFFFFFFF);
    round_trip(0x08C54044);

    // A stored (save) record: no party stats, decodes the same.
    struct pd_pk6 in = sample(0xA5A5A5A5), out;
    uint8_t raw[PD_PK6_STORED];
    pd_pk6_encode(&in, PD_PK6_STORED, raw);
    CHECK(pd_pk6_decode(raw, PD_PK6_STORED, &out), "stored: decode failed");
    CHECK(out.species == 658 && out.level == 0 && out.hp == 0, "stored: species %d level %d", out.species, out.level);

    // A flipped byte breaks the checksum.
    raw[0x20] ^= 0xFF;
    CHECK(!pd_pk6_decode(raw, PD_PK6_STORED, &out), "corrupt record decoded");

    // An empty slot (species 0) is not a Pokemon.
    memset(&in, 0, sizeof in);
    in.ec = 0x11111111;
    pd_pk6_encode(&in, PD_PK6_STORED, raw);
    CHECK(!pd_pk6_decode(raw, PD_PK6_STORED, &out), "empty slot decoded");

    // The Gen 6 chart: Fairy in, Steel's Dark / Ghost resistances out.
    CHECK(pd_gen6_effectiveness(PD_TYPE_FAIRY, PD_TYPE_DRAGON) == 4, "fairy vs dragon");
    CHECK(pd_gen6_effectiveness(PD_TYPE_DRAGON, PD_TYPE_FAIRY) == 0, "dragon vs fairy");
    CHECK(pd_gen6_effectiveness(PD_TYPE_FAIRY, PD_TYPE_FIGHTING) == 4, "fairy vs fighting");
    CHECK(pd_gen6_effectiveness(PD_TYPE_FIGHTING, PD_TYPE_FAIRY) == 1, "fighting vs fairy");
    CHECK(pd_gen6_effectiveness(PD_TYPE_GHOST, PD_TYPE_STEEL) == 2, "ghost vs steel (Gen 6: neutral)");
    CHECK(pd_gen6_effectiveness(PD_TYPE_DARK, PD_TYPE_STEEL) == 2, "dark vs steel (Gen 6: neutral)");
    CHECK(pd_gen6_effectiveness(PD_TYPE_POISON, PD_TYPE_STEEL) == 0, "poison vs steel");
    CHECK(pd_gen6_effectiveness(PD_TYPE_FIGHTING, PD_TYPE_GHOST) == 0, "fighting vs ghost");
    CHECK(pd_gen6_effectiveness(PD_TYPE_FIRE, PD_TYPE_ROCK) == 1, "fire vs rock");
    CHECK(pd_gen6_effectiveness(PD_TYPE_NORMAL, PD_TYPE_NORMAL) == 2, "normal vs normal");

    // A synthetic main save: 0x65600 bytes, the FEEB footer, two PK6s at 0x1000.
    static uint8_t mainFile[PD_SAVE6_SIZE];
    memset(mainFile, 0, sizeof mainFile);
    memcpy(mainFile + PD_SAVE6_SIZE - 0x10, "FEEB", 4);
    struct pd_pk6 p1 = sample(0x01020304), p2 = sample(0x0A0B0C0D);
    p2.species = 661; // Fletchling
    strcpy(p2.nickname, "FLETCH");
    pd_pk6_encode(&p1, PD_PK6_STORED, mainFile + 0x1000);
    pd_pk6_encode(&p2, PD_PK6_STORED, mainFile + 0x1000 + PD_PK6_STORED);
    const struct pd_save6_layout testLayout = { .partyOff = 0x1000 };
    struct pd_save6 s;
    CHECK(pd_save6_parse(&s, mainFile, sizeof mainFile, &testLayout) == NULL, "save6 parse failed");
    struct pd_pk6 m;
    CHECK(pd_save6_party_mon(&s, 0, &m) && m.species == 658 && strcmp(m.nickname, "CHESPIN") == 0, "save6 party 0");
    CHECK(pd_save6_party_mon(&s, 1, &m) && m.species == 661, "save6 party 1 species %d", m.species);
    CHECK(!pd_save6_party_mon(&s, 2, &m), "save6 party 2 (empty) decoded");
    CHECK(pd_save6_parse(&s, mainFile, PD_SAVE6_SIZE - 1, &testLayout) != NULL, "short save accepted");
    mainFile[PD_SAVE6_SIZE - 0x10] = 0;
    CHECK(pd_save6_parse(&s, mainFile, sizeof mainFile, &testLayout) != NULL, "save without footer accepted");
    memcpy(mainFile + PD_SAVE6_SIZE - 0x10, "FEEB", 4);
    CHECK(pd_save6_parse(&s, mainFile, sizeof mainFile, &pd_save6_xy) == NULL, "xy layout parse failed");
    // X / Y's proven party offset (pd_save6.c); this synthetic file's party is at 0x1000, not there.
    CHECK(pd_save6_xy.partyOff == 0x22600 && !pd_save6_party_mon(&s, 0, &m), "xy layout read at an unproven offset");
    const struct pd_save6_layout none = { .partyOff = 0 };
    CHECK(pd_save6_parse(&s, mainFile, sizeof mainFile, &none) == NULL && !pd_save6_party_mon(&s, 0, &m),
          "unmapped layout read");

    if (failures) {
        printf("%d FAILURES\n", failures);
        return 1;
    }
    printf("gen6_test: all ok\n");
    return 0;
}
