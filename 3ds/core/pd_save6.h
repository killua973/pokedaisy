// A Gen 6 save (X / Y's "main" file: 0x65600 bytes with a 0xFEEB footer block)
// - read-only for now: parse the container, decode the party's stored PK6s.
// The C twin of the app's N3dsSaves / Gen6Reader (docs/3DS_GAMES_PLAN.md).
#ifndef PD_SAVE6_H
#define PD_SAVE6_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pd_pk6.h"

#define PD_SAVE6_SIZE 0x65600
#define PD_SAVE6_FOOTER "FEEB" // the footer block's magic, near the end (0x65410 in X/Y's)
#define PD_SAVE6_PARTY 6

struct pd_save6_layout {
    uint32_t partyOff;    // where the six party records sit; 0 = not mapped yet
    uint32_t partyStride; // 0x104: each record is a PK6 + its 0x1C party stats (level/HP
                          // available, like a live RAM slot); 0xE8: plain stored PK6s
};

// X / Y's: 0x65600 bytes with the FEEB footer; the party's six records at 0x14200,
// stride 0x104 (cross-proved against a live RAM dump: identical species/level/HP;
// the PC box storage starts at 0x22600, stride 0xE8). Other builds / languages:
// prove with pd_gen6_preview scan.
extern const struct pd_save6_layout pd_save6_xy;

struct pd_save6 {
    uint8_t raw[PD_SAVE6_SIZE];
    size_t len;
    const struct pd_save6_layout* f;
};

// NULL on success, else why not (in words for the player).
const char* pd_save6_parse(struct pd_save6* s, const uint8_t* bytes, size_t len, const struct pd_save6_layout* f);
// The i-th party Pokemon. With stride 0x104 the record carries the party stats
// (level, HP, stats filled); with 0xE8 it's a plain stored PK6 (level 0).
// false when the offset isn't mapped or the slot is empty.
bool pd_save6_party_mon(const struct pd_save6* s, int i, struct pd_pk6* out);

#endif
