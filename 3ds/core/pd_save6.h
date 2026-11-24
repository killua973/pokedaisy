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
    uint32_t partyOff; // where the six stored PK6s sit; 0 = not mapped yet
};

// X / Y's: 0x65600 bytes with the FEEB footer; the party's six stored PK6s at
// 0x22600 (proven on a real main, see pd_save6.c). Other builds / languages:
// prove with pd_gen6_preview scan.
extern const struct pd_save6_layout pd_save6_xy;

struct pd_save6 {
    uint8_t raw[PD_SAVE6_SIZE];
    size_t len;
    const struct pd_save6_layout* f;
};

// NULL on success, else why not (in words for the player).
const char* pd_save6_parse(struct pd_save6* s, const uint8_t* bytes, size_t len, const struct pd_save6_layout* f);
// The i-th party Pokemon as a stored PK6 (level 0: a stored record carries no
// party stats). false when the offset isn't mapped or the slot is empty.
bool pd_save6_party_mon(const struct pd_save6* s, int i, struct pd_pk6* out);

#endif
