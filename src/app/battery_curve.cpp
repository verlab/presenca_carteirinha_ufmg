#include "app/battery_curve.h"

// mV -> %, high to low. Linear across the pack's usable range
// 3.90 V (100%) .. 3.30 V (0%), with an anchor every 5% (30 mV) so the mapping
// is easy to tweak per-anchor later; battery_mv_to_pct() interpolates linearly
// between adjacent anchors.
//
// BOTH endpoints are what this board can use, not what the cell can hold. The
// datasheet range is 4.20 V .. 2.70 V; the device is treated as full at 3.90 V
// and flat at 3.30 V, so 0% means "about to stop" and 100% means "as charged as
// this reading gets". Anything at or above 3.90 V clamps to 100%, which is what
// makes a resting pack read full instead of stalling in the 90s.
//
// The 600 mV span is narrow, so the reading moves fast: 30 mV is a whole 5%
// step, and 30 mV is not far above the noise and calibration error on this
// divider. Expect the percentage to be jumpier than the old 1500 mV curve.
//
// These are post-calibration volts, i.e. what battery_service reports after
// BAT_CAL_SCALE — not raw cell volts. Endpoints under load are still unverified;
// see BACKLOG.md §Q4 / BATTERY_DRAIN_LOG in platformio.ini.
struct volt_pct_t {
    uint16_t mv;
    uint8_t pct;
};
static const volt_pct_t CURVE[] = {
    {3900, 100},
    {3870, 95},
    {3840, 90},
    {3810, 85},
    {3780, 80},
    {3750, 75},
    {3720, 70},
    {3690, 65},
    {3660, 60},
    {3630, 55},
    {3600, 50},
    {3570, 45},
    {3540, 40},
    {3510, 35},
    {3480, 30},
    {3450, 25},
    {3420, 20},
    {3390, 15},
    {3360, 10},
    {3330, 5},
    {3300, 0},
};

// No charge-status pin on this board, so "charging" is inferred from the rail
// sitting above anything a resting pack reaches: 4.20 V is the cell's own full
// voltage, and only USB actively topping it pushes the reading higher.
//
// This is deliberately NOT the curve's 100% anchor (3.90 V) — the two answer
// different questions. Tying charging to 3.90 V would light the charging icon on
// any healthy resting pack. The gap is the cost: between 3.90 V and 4.20 V the
// device reads 100% and "not charging", which is correct for a rested full pack
// and wrong for one still on USB. If the pack never reads above 4.20 V in
// practice, the charging icon never appears — see BACKLOG.md §Q4.
static constexpr uint16_t CHARGING_MV = 4200;

bool battery_is_charging(uint16_t mv) { return mv > CHARGING_MV; }

uint8_t battery_mv_to_pct(uint16_t mv) {
    const int n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv >= CURVE[0].mv) return 100;
    if (mv <= CURVE[n - 1].mv) return 0;
    for (int i = 1; i < n; i++) {
        if (mv >= CURVE[i].mv) {
            const volt_pct_t& hi = CURVE[i - 1];
            const volt_pct_t& lo = CURVE[i];
            float f = (float)(mv - lo.mv) / (float)(hi.mv - lo.mv);
            return (uint8_t)(lo.pct + f * (hi.pct - lo.pct) + 0.5f);
        }
    }
    return 0;
}
