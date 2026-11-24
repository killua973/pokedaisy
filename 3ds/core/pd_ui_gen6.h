// The Gen 6 (X / Y) companion's read-only PARTY screen, drawn on a pd_canvas:
// the six slots (nickname, level, HP bar, status) and the selected Pokemon's
// page (item, ability, nature, moves, IVs, EVs, stats, exp, PID, shiny/egg).
// Names come from the PK6 itself; species / item / move ids stay numeric until
// the cart's RomFS text reader lands (Gen6Text's GARC a/0/7/<n>).
#ifndef PD_UI_GEN6_H_
#define PD_UI_GEN6_H_

#include "pd_canvas.h"
#include "pd_gen6.h"

#define PD_UI_GEN6_W 400
#define PD_UI_GEN6_H 240

// Draws the party screen; [selected] is the highlighted slot (0..partyCount-1, -1 = none).
void pd_ui_gen6_party(struct pd_canvas* c, const struct pd_gen6_snapshot* s, int selected);

#endif // PD_UI_GEN6_H_
