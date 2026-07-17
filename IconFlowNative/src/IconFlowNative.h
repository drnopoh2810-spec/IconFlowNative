/**
 * IconFlowNative.h
 * Main header for IconFlow Native Picker — Adobe After Effects native effect plugin.
 * Target: Windows x64, AE 2024 (24.x), MSVC v143, CEP/CSXS 9+
 *
 * Build requires AE_SDK_ROOT environment variable pointing to the AE SDK root.
 */
#pragma once

// ── Standard SDK includes ────────────────────────────────────────────────────
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "Smart_Utils.h"
#include "Param_Utils.h"
#include "String_Utils.h"

// ── Plugin identity ──────────────────────────────────────────────────────────
#define PLUGIN_NAME        "IconFlow Native Picker"
#define PLUGIN_MATCH_NAME  "IconFlow Native Picker"
#define PLUGIN_CATEGORY    "IconFlow"
#define PLUGIN_MAJOR_VER   1
#define PLUGIN_MINOR_VER   5
#define PLUGIN_BUG_VER     0
#define PLUGIN_STAGE_VER   PF_Stage_RELEASE
#define PLUGIN_BUILD_VER   0

// ── Number of color-replacement slots ────────────────────────────────────────
#define NUM_COLOR_SLOTS    8
#define PARAMS_PER_SLOT    6   // enable, pick, src, dst, tolerance, softness

// ── Parameter indices ────────────────────────────────────────────────────────
enum ParamIndex {
    PARAM_INPUT = 0,

    // Background keying
    PARAM_BG_ENABLE,        //  1
    PARAM_BG_PICK,          //  2  button → triggers eyedropper for BG color
    PARAM_BG_COLOR,         //  3
    PARAM_BG_DIST_MODE,     //  4  popup: 1=RGB, 2=Hue, 3=Chroma
    PARAM_BG_TOLERANCE,     //  5
    PARAM_BG_SOFTNESS,      //  6
    PARAM_BG_EDGE_CHOKE,    //  7  negative=expand, positive=choke, px
    PARAM_BG_EDGE_CLEANUP,  //  8  checkbox
    PARAM_BG_DESPILL,       //  9  checkbox (green spill suppressor)

    // Color replacement — 8 slots of 6 params each (indices 10–57)
    PARAM_CR0_ENABLE,       // 10
    PARAM_CR0_PICK,         // 11  button
    PARAM_CR0_SRC,          // 12
    PARAM_CR0_DST,          // 13
    PARAM_CR0_TOL,          // 14
    PARAM_CR0_SOFT,         // 15

    PARAM_CR1_ENABLE,       // 16
    PARAM_CR1_PICK,         // 17
    PARAM_CR1_SRC,          // 18
    PARAM_CR1_DST,          // 19
    PARAM_CR1_TOL,          // 20
    PARAM_CR1_SOFT,         // 21

    PARAM_CR2_ENABLE,       // 22
    PARAM_CR2_PICK,         // 23
    PARAM_CR2_SRC,          // 24
    PARAM_CR2_DST,          // 25
    PARAM_CR2_TOL,          // 26
    PARAM_CR2_SOFT,         // 27

    PARAM_CR3_ENABLE,       // 28
    PARAM_CR3_PICK,         // 29
    PARAM_CR3_SRC,          // 30
    PARAM_CR3_DST,          // 31
    PARAM_CR3_TOL,          // 32
    PARAM_CR3_SOFT,         // 33

    PARAM_CR4_ENABLE,       // 34
    PARAM_CR4_PICK,         // 35
    PARAM_CR4_SRC,          // 36
    PARAM_CR4_DST,          // 37
    PARAM_CR4_TOL,          // 38
    PARAM_CR4_SOFT,         // 39

    PARAM_CR5_ENABLE,       // 40
    PARAM_CR5_PICK,         // 41
    PARAM_CR5_SRC,          // 42
    PARAM_CR5_DST,          // 43
    PARAM_CR5_TOL,          // 44
    PARAM_CR5_SOFT,         // 45

    PARAM_CR6_ENABLE,       // 46
    PARAM_CR6_PICK,         // 47
    PARAM_CR6_SRC,          // 48
    PARAM_CR6_DST,          // 49
    PARAM_CR6_TOL,          // 50
    PARAM_CR6_SOFT,         // 51

    PARAM_CR7_ENABLE,       // 52
    PARAM_CR7_PICK,         // 53
    PARAM_CR7_SRC,          // 54
    PARAM_CR7_DST,          // 55
    PARAM_CR7_TOL,          // 56
    PARAM_CR7_SOFT,         // 57

    // Global options
    PARAM_PRESERVE_LUM,     // 58  checkbox
    PARAM_GLOBAL_TINT_EN,   // 59  checkbox
    PARAM_GLOBAL_TINT_COLOR,// 60

    // Glow
    PARAM_GLOW_ENABLE,      // 61
    PARAM_GLOW_THRESHOLD,   // 62
    PARAM_GLOW_RADIUS,      // 63
    PARAM_GLOW_INTENSITY,   // 64
    PARAM_GLOW_TINT_COLOR,  // 65

    PARAM_NUM_PARAMS        // 66
};

// ── Slot accessor macros ─────────────────────────────────────────────────────
#define SLOT_BASE(i)    (PARAM_CR0_ENABLE + (i) * PARAMS_PER_SLOT)
#define SLOT_ENABLE(i)  (SLOT_BASE(i) + 0)
#define SLOT_PICK(i)    (SLOT_BASE(i) + 1)
#define SLOT_SRC(i)     (SLOT_BASE(i) + 2)
#define SLOT_DST(i)     (SLOT_BASE(i) + 3)
#define SLOT_TOL(i)     (SLOT_BASE(i) + 4)
#define SLOT_SOFT(i)    (SLOT_BASE(i) + 5)

// ── Sequence data (per-instance mutable state) ───────────────────────────────
//
// picker_active: PARAM index of the parameter awaiting a Comp-panel click,
//                or PICKER_NONE (-1) when no pick is in progress.
// ctrl_down:     1 if Ctrl was held when the pick button was pressed
//                (triggers 5×5 averaged sample).
#define PICKER_NONE  (-1)

struct SequenceData {
    A_long picker_active;  // PARAM index or PICKER_NONE
    A_long ctrl_sample;    // 1 → 5×5 averaged sample on next click
    A_u_char _pad[8];
};

// ── Plugin entry point ───────────────────────────────────────────────────────
extern "C" {
DllExport PF_Err EffectMain(
    PF_Cmd        cmd,
    PF_InData    *in_data,
    PF_OutData   *out_data,
    PF_ParamDef  *params[],
    PF_LayerDef  *output,
    void         *extra);
}
