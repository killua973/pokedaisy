// A Gen 6 Pokemon (X / Y / ORAS / Sun / Moon's PK6): the C twin of the app's
// Gen6Reader decrypt/decode (app/.../n3ds/Gen6Reader.kt, docs/3DS_GAMES_PLAN.md).
// The stored record is 0xE8 bytes; a live party slot adds 0x1C bytes of party
// stats (status, level, HP, the five stats) whose cipher starts again from the
// encryption constant. Read-only.
#ifndef PD_PK6_H
#define PD_PK6_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PD_PK6_STORED 0xE8 // the stored record (save, PC)
#define PD_PK6_PARTY 0x104 // stored + the 0x1C party stats (live RAM slot)
#define PD_PK6_MOVES 4
#define PD_PK6_NICKNAME 32

struct pd_pk6 {
    uint32_t ec, pid, tid, sid, exp, status;
    int species, form, heldItem, ability, nature;
    int level, hp, maxHp;   // the party stats block only (0 in a stored record)
    int stats[5];           // Attack, Defense, Speed, Sp. Atk, Sp. Def
    int moves[PD_PK6_MOVES], pp[PD_PK6_MOVES];
    uint8_t ivs[6], evs[6]; // HP, Attack, Defense, Speed, Sp. Atk, Sp. Def
    bool female, genderless, egg, shiny;
    char nickname[PD_PK6_NICKNAME]; // UTF-8, from the record's UTF-16LE
};

// Decrypts [len] (PD_PK6_STORED or PD_PK6_PARTY) bytes of [raw] into [plain] and
// unshuffles the four 0x38-byte blocks; false on a bad checksum.
bool pd_pk6_decrypt(const uint8_t* raw, size_t len, uint8_t* plain);
// Decodes [raw]; false when the slot is empty (species 0) or the checksum is off.
bool pd_pk6_decode(const uint8_t* raw, size_t len, struct pd_pk6* out);
// The inverse of pd_pk6_decrypt (tests and fixtures), for [len] bytes.
void pd_pk6_encode(const struct pd_pk6* m, size_t len, uint8_t* raw);

#endif
