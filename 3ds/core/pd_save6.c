// pd_save6.c - see pd_save6.h.
#include "pd_save6.h"

#include <string.h>

const struct pd_save6_layout pd_save6_xy = { .partyOff = 0 };

static uint32_t get32(const uint8_t* p, size_t off) {
    return (uint32_t) (p[off] | (p[off + 1] << 8) | ((uint32_t) p[off + 2] << 16) | ((uint32_t) p[off + 3] << 24));
}

const char* pd_save6_parse(struct pd_save6* s, const uint8_t* bytes, size_t len, const struct pd_save6_layout* f) {
    if (len != PD_SAVE6_SIZE) return "not a Gen 6 main save (wrong size)";
    memcpy(s->raw, bytes, PD_SAVE6_SIZE);
    s->len = len;
    s->f = f;
    // The footer block carries the 0xFEEB magic near the end of the file.
    bool footer = false;
    for (size_t off = PD_SAVE6_SIZE - 0x2000; off + 4 <= PD_SAVE6_SIZE; off += 4)
        if (get32(s->raw, off) == PD_SAVE6_FOOTER) footer = true;
    if (!footer) return "no 0xFEEB footer block";
    return NULL;
}

bool pd_save6_party_mon(const struct pd_save6* s, int i, struct pd_pk6* out) {
    if (!s->f || s->f->partyOff == 0 || i < 0 || i >= PD_SAVE6_PARTY) return false;
    size_t off = s->f->partyOff + (size_t) i * PD_PK6_STORED;
    if (off + PD_PK6_STORED > s->len) return false;
    return pd_pk6_decode(s->raw + off, PD_PK6_STORED, out);
}
