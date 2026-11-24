// pd_pk6.c - see pd_pk6.h. The cipher and the block order are the game's own
// (Gen6Reader.kt's decrypt): an LCRNG seeded with the encryption constant
// (0x41C64E6D / 0x6073, the high half of each step XORed in), the four 0x38-byte
// blocks stored in the order BLOCK_ORDERS[(ec >> 13 & 31) % 24], the checksum a
// u16 sum of the ordered data. The live party's stats block at +0x158 restarts
// the cipher from the EC (pd_gen6 copies it to +0xE8 before decrypting).
#include "pd_pk6.h"

#include <string.h>

static const char BLOCK_ORDERS[24][5] = {
    "ABCD", "ABDC", "ACBD", "ACDB", "ADBC", "ADCB", "BACD", "BADC", "BCAD", "BCDA", "BDAC", "BDCA",
    "CABD", "CADB", "CBAD", "CBDA", "CDAB", "CDBA", "DABC", "DACB", "DBAC", "DBCA", "DCAB", "DCBA",
};

static uint16_t get16(const uint8_t* p, int off) {
    return (uint16_t) (p[off] | (p[off + 1] << 8));
}

static uint32_t get32(const uint8_t* p, int off) {
    return (uint32_t) (p[off] | (p[off + 1] << 8) | ((uint32_t) p[off + 2] << 16) | ((uint32_t) p[off + 3] << 24));
}

static void put16(uint8_t* p, int off, uint16_t v) {
    p[off] = (uint8_t) v;
    p[off + 1] = (uint8_t) (v >> 8);
}

static void put32(uint8_t* p, int off, uint32_t v) {
    p[off] = (uint8_t) v;
    p[off + 1] = (uint8_t) (v >> 8);
    p[off + 2] = (uint8_t) (v >> 16);
    p[off + 3] = (uint8_t) (v >> 24);
}

// The cipher over [len] bytes, symmetric: at PD_PK6_STORED the seed restarts
// from the EC (the live party's stats block).
static void crypt(const uint8_t* in, size_t len, uint32_t ec, uint8_t* out) {
    uint32_t seed = ec;
    memcpy(out, in, 8); // the EC and the checksum travel in the clear
    for (size_t i = 8; i < len; i += 2) {
        if (i == PD_PK6_STORED) seed = ec;
        seed = seed * 0x41C64E6Du + 0x6073u;
        put16(out, (int) i, get16(in, (int) i) ^ (uint16_t) (seed >> 16));
    }
}

static void unshuffle(uint8_t* plain, uint32_t ec) {
    uint8_t old[PD_PK6_STORED];
    memcpy(old, plain, PD_PK6_STORED);
    memcpy(plain, old, 8);
    const char* order = BLOCK_ORDERS[((ec >> 13) & 31) % 24];
    for (int k = 0; k < 4; k++) {
        int dest = 0;
        while ("ABCD"[dest] != order[k]) dest++;
        memcpy(plain + 8 + 56 * dest, old + 8 + 56 * k, 56);
    }
}

bool pd_pk6_decrypt(const uint8_t* raw, size_t len, uint8_t* plain) {
    if (len != PD_PK6_STORED && len != PD_PK6_PARTY) return false;
    uint32_t ec = get32(raw, 0);
    crypt(raw, len, ec, plain);
    unshuffle(plain, ec); // the stats block past PD_PK6_STORED stays in place
    uint32_t sum = 0;
    for (int j = 8; j < PD_PK6_STORED; j += 2) sum = (sum + get16(plain, j)) & 0xFFFF;
    return sum == get16(plain, 6);
}

// The record's UTF-16LE nickname (24 bytes at +0x40) as UTF-8.
static void nickname(const uint8_t* p, char* out, size_t cap) {
    size_t n = 0;
    for (int i = 0; i < 12 && n + 4 < cap; i++) {
        uint32_t c = get16(p, 0x40 + i * 2);
        if (c == 0) break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < 12) {
            uint32_t lo = get16(p, 0x40 + (i + 1) * 2);
            if (lo >= 0xDC00 && lo < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
        }
        if (c < 0x80) out[n++] = (char) c;
        else if (c < 0x800) {
            out[n++] = (char) (0xC0 | (c >> 6));
            out[n++] = (char) (0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out[n++] = (char) (0xE0 | (c >> 12));
            out[n++] = (char) (0x80 | ((c >> 6) & 0x3F));
            out[n++] = (char) (0x80 | (c & 0x3F));
        } else {
            out[n++] = (char) (0xF0 | (c >> 18));
            out[n++] = (char) (0x80 | ((c >> 12) & 0x3F));
            out[n++] = (char) (0x80 | ((c >> 6) & 0x3F));
            out[n++] = (char) (0x80 | (c & 0x3F));
        }
    }
    out[n] = 0;
}

bool pd_pk6_decode(const uint8_t* raw, size_t len, struct pd_pk6* out) {
    uint8_t plain[PD_PK6_PARTY];
    if (!pd_pk6_decrypt(raw, len, plain)) return false;
    memset(out, 0, sizeof *out);
    out->ec = get32(plain, 0);
    out->species = get16(plain, 0x08);
    if (out->species == 0) return false;
    out->heldItem = get16(plain, 0x0A);
    out->tid = get16(plain, 0x0C);
    out->sid = get16(plain, 0x0E);
    out->exp = get32(plain, 0x10);
    out->ability = plain[0x14];
    out->pid = get32(plain, 0x18);
    out->nature = plain[0x1C];
    out->form = plain[0x1D] >> 3;
    out->female = (plain[0x1D] & 2) != 0;
    out->genderless = (plain[0x1D] & 4) != 0;
    nickname(plain, out->nickname, sizeof out->nickname);
    for (int i = 0; i < PD_PK6_MOVES; i++) {
        out->moves[i] = get16(plain, 0x5A + i * 2);
        out->pp[i] = plain[0x62 + i];
    }
    uint32_t iv = get32(plain, 0x74);
    for (int i = 0; i < 6; i++) out->ivs[i] = (uint8_t) ((iv >> (i * 5)) & 31);
    out->egg = (iv >> 30) & 1;
    for (int i = 0; i < 6; i++) out->evs[i] = (uint8_t) get16(plain, 0x78 + i * 2);
    out->shiny = (uint16_t) (out->tid ^ out->sid ^ (out->pid >> 16) ^ (out->pid & 0xFFFF)) < 16;
    if (len == PD_PK6_PARTY) {
        out->status = get32(plain, 0xE8);
        out->level = plain[0xEC];
        out->hp = get16(plain, 0xF0);
        out->maxHp = get16(plain, 0xF2);
        for (int i = 0; i < 5; i++) out->stats[i] = get16(plain, 0xF4 + i * 2);
    }
    return true;
}

// One UTF-8 code point from [s], advancing [*i]. Returns 0 at the end.
static uint32_t utf8_next(const char* s, size_t* i) {
    uint8_t c = (uint8_t) s[*i];
    if (c < 0x80) {
        (*i)++;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && s[*i + 1]) {
        uint32_t v = ((c & 0x1F) << 6) | ((uint8_t) s[*i + 1] & 0x3F);
        *i += 2;
        return v;
    }
    if ((c & 0xF0) == 0xE0 && s[*i + 1] && s[*i + 2]) {
        uint32_t v = ((c & 0x0F) << 12) | (((uint8_t) s[*i + 1] & 0x3F) << 6) | ((uint8_t) s[*i + 2] & 0x3F);
        *i += 3;
        return v;
    }
    (*i)++;
    return 0xFFFD;
}

void pd_pk6_encode(const struct pd_pk6* m, size_t len, uint8_t* raw) {
    if (len != PD_PK6_STORED && len != PD_PK6_PARTY) return;
    uint8_t plain[PD_PK6_PARTY];
    memset(plain, 0, sizeof plain);
    put32(plain, 0, m->ec);
    put16(plain, 0x08, (uint16_t) m->species);
    put16(plain, 0x0A, (uint16_t) m->heldItem);
    put16(plain, 0x0C, (uint16_t) m->tid);
    put16(plain, 0x0E, (uint16_t) m->sid);
    put32(plain, 0x10, m->exp);
    plain[0x14] = (uint8_t) m->ability;
    put32(plain, 0x18, m->pid);
    plain[0x1C] = (uint8_t) m->nature;
    plain[0x1D] = (uint8_t) ((m->form << 3) | (m->female ? 2 : 0) | (m->genderless ? 4 : 0));
    size_t i = 0;
    for (int k = 0; k < 12; k++) {
        uint32_t c = m->nickname[i] ? utf8_next(m->nickname, &i) : 0;
        put16(plain, 0x40 + k * 2, (uint16_t) c);
    }
    for (int k = 0; k < PD_PK6_MOVES; k++) {
        put16(plain, 0x5A + k * 2, (uint16_t) m->moves[k]);
        plain[0x62 + k] = (uint8_t) m->pp[k];
    }
    uint32_t iv = (m->egg ? 1u << 30 : 0);
    for (int k = 0; k < 6; k++) iv |= (uint32_t) (m->ivs[k] & 31) << (k * 5);
    put32(plain, 0x74, iv);
    for (int k = 0; k < 6; k++) put16(plain, 0x78 + k * 2, m->evs[k]);
    if (len == PD_PK6_PARTY) {
        put32(plain, 0xE8, m->status);
        plain[0xEC] = (uint8_t) m->level;
        put16(plain, 0xF0, (uint16_t) m->hp);
        put16(plain, 0xF2, (uint16_t) m->maxHp);
        for (int k = 0; k < 5; k++) put16(plain, 0xF4 + k * 2, (uint16_t) m->stats[k]);
    }
    uint32_t sum = 0;
    for (int j = 8; j < PD_PK6_STORED; j += 2) sum = (sum + get16(plain, j)) & 0xFFFF;
    put16(plain, 6, (uint16_t) sum);
    uint8_t shuffled[PD_PK6_PARTY];
    memcpy(shuffled, plain, 8);
    const char* order = BLOCK_ORDERS[((m->ec >> 13) & 31) % 24];
    for (int d = 0; d < 4; d++) {
        int k = 0;
        while (order[k] != "ABCD"[d]) k++;
        memcpy(shuffled + 8 + 56 * k, plain + 8 + 56 * d, 56);
    }
    if (len == PD_PK6_PARTY) memcpy(shuffled + PD_PK6_STORED, plain + PD_PK6_STORED, PD_PK6_PARTY - PD_PK6_STORED);
    crypt(shuffled, len, m->ec, raw);
}
