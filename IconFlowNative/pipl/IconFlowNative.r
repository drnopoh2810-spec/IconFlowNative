/*
 * IconFlowNative.r
 * PiPL resource descriptor for IconFlow Native Picker (.aex)
 *
 * Processed by the AE SDK PiPL tool (pipl.rsp / cnvtpipl):
 *   Windows:  $(AE_SDK_ROOT)\Resources\PiPLtool.exe
 *   Command:  PiPLtool IconFlowNative.r IconFlowNative.rr
 *
 * The generated .rr file is #included by IconFlowNative.rc.
 *
 * SDK includes required by Rez/rc preprocessing:
 *   AEConfig.h, AE_EffectVers.h, PIGeneral.r, PIDefines.h
 *
 * Host: After Effects 2020 (version 17) and later, x64 Windows.
 */

#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AEConfig_h
    #error AEConfig.h not found. Ensure AE_SDK_ROOT is set correctly.
#endif

resource 'PiPL' (16000) {
    {
        /* ── Plugin kind ──────────────────────────────────────── */
        Kind { AEEffect },

        /* ── Display name (appears in Effect menu) ───────────── */
        Name { "IconFlow Native Picker" },

        /* ── Effect category ─────────────────────────────────── */
        Category { "IconFlow" },

        /* ── Unique match name (must never change after release) */
        AEEffectMatchName { "ADBE IconFlow Native Picker" },

        /* ── Plugin version ──────────────────────────────────── */
        Version { 0x00010500 },      /* 1.5.0 */

        /* ── Host compatibility range ────────────────────────── */
        /* AE 17.0 (2020) – 99.9 */
        AEReservedInfo { 8 },

        /* ── Entry point (exported function name) ────────────── */
        CodeWin64X86 { "EffectMain" },

        /* ── Supported rendering features ───────────────────── */
        /* DEEP_COLOR_AWARE: 16-bit support                       */
        /* CUSTOM_UI: Custom Comp UI for eyedropper               */
        /* SMART_RENDER: pre-render / smart-render path           */
        SupportedModes
        {
            noBitmap, noGrayScale,
            noIndexedColor, noRGBColor,
            noCMYKColor, noHSLColor,
            noHSBColor, noMultichannel,
            noDuotone, noLABColor
        },

        /* ── PiPL properties required by AE SDK ─────────────── */
        /* Number of parameters is determined at runtime via       */
        /* PF_Cmd_PARAMS_SETUP; PiPL does not declare them.        */
    }
};
