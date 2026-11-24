// pd_save6.c - see pd_save6.h.
#include "pd_save6.h"

#include <string.h>

// X / Y's: the six stored PK6s sit at 0x22600, proven on a real main (scan:
// six consecutive checksum-valid PK6s there, species all <= 721; the block at
// 0x4A00 decodes as six empty slots, the PC storage follows right after).
const struct pd_save6_layout pd_save6_xy = { .partyOff = 0x22600 };

const char* pd_save6_parse(struct pd_save6* s, const uint8_t* bytes, size_t len, const struct pd_save6_layout* f) {
    if (len != PD_SAVE6_SIZE) return "not a Gen 6 main save (wrong size)";
    memcpy(s->raw, bytes, PD_SAVE6_SIZE);
    s->len = len;
    s->f = f;
    // The footer block carries the "FEEB" magic near the end of the file (0x65410 in X/Y's).
    bool footer = false;
    for (size_t off = PD_SAVE6_SIZE - 0x2000; off + 4 <= PD_SAVE6_SIZE; off++)
        if (memcmp(s->raw + off, "FEEB", 4) == 0) footer = true;
    if (!footer) return "no FEEB footer block";
    return NULL;
}

bool pd_save6_party_mon(const struct pd_save6* s, int i, struct pd_pk6* out) {
    if (!s->f || s->f->partyOff == 0 || i < 0 || i >= PD_SAVE6_PARTY) return false;
    size_t off = s->f->partyOff + (size_t) i * PD_PK6_STORED;
    if (off + PD_PK6_STORED > s->len) return false;
    return pd_pk6_decode(s->raw + off, PD_PK6_STORED, out);
}
