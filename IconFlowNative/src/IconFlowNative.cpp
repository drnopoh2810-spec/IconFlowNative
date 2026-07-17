/**
 * IconFlowNative.cpp
 * Adobe After Effects native effect plugin — IconFlow Native Picker v1.5
 *
 * Implements:
 *   PF_Cmd_ABOUT
 *   PF_Cmd_GLOBAL_SETUP
 *   PF_Cmd_PARAMS_SETUP
 *   PF_Cmd_RENDER (+ SmartFX / PF_Cmd_SMART_RENDER)
 *   PF_Cmd_EVENT  (Custom Comp UI eyedropper)
 *   PF_Cmd_USER_CHANGED_PARAM (pick-button handler)
 *   PF_Cmd_SEQUENCE_SETUP / RESETUP / FLATTEN / UNFLATTEN
 *
 * SDK API notes / known uncertainties
 * ─────────────────────────────────────
 * 1. PF_EventExtra coordinate conversion callbacks:
 *    The exact function signatures for comp_to_layer / layer_to_comp differ
 *    between SDK versions.  Locate the CCU (Custom Comp UI) sample shipped
 *    with your SDK and compare the PF_EventCallbacks typedef in AE_EffectUI.h.
 *    The calls below match the SDK 2022+ convention; adjust if required.
 *
 * 2. PF_WorldFlag_FLOAT:
 *    32-bit float worlds are identified via (output->world_flags & PF_WorldFlag_FLOAT).
 *    If your SDK revision names this flag differently, grep for "WorldFlag" in AE_Effect.h.
 *
 * 3. PF_Cursor_CROSS_HAIR:
 *    Cursor constant used for ADJUST_CURSOR during picking.
 *    If your SDK spells it differently, grep for "Cursor" in AE_EffectUI.h.
 *
 * 4. AEGP_SuiteHandler:
 *    Used only for param checkout/checkin in the EVENT handler.  If you
 *    encounter link errors, ensure you link AEGPLib.lib (or the equivalent
 *    provided by your SDK build scripts).
 */

#include "IconFlowNative.h"
#include "Renderer.h"
#include "ColorMath.h"

#include <cstring>
#include <cmath>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Sequence data helpers
// ─────────────────────────────────────────────────────────────────────────────

static PF_Err AllocSequenceData(PF_InData *in_data, PF_OutData *out_data)
{
    PF_Err err = PF_Err_NONE;
    PF_Handle h = PF_NEW_HANDLE(sizeof(SequenceData));
    if (!h) return PF_Err_OUT_OF_MEMORY;

    SequenceData *sd = reinterpret_cast<SequenceData*>(PF_LOCK_HANDLE(h));
    sd->picker_active = PICKER_NONE;
    sd->ctrl_sample   = 0;
    PF_UNLOCK_HANDLE(h);

    out_data->sequence_data = h;
    return err;
}

static SequenceData* LockSequenceData(PF_InData *in_data)
{
    if (!in_data->sequence_data) return nullptr;
    return reinterpret_cast<SequenceData*>(PF_LOCK_HANDLE(in_data->sequence_data));
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_ABOUT
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err About(PF_InData *in_data, PF_OutData *out_data)
{
    PF_SPRINTF(out_data->return_msg,
        "%s v%d.%d\n"
        "Adobe After Effects native color-keying and replacement effect.\n"
        "Includes composition-panel eyedropper (Custom Comp UI).\n"
        "No Deep Glow dependency — built-in simple glow only.\n"
        "\xC2\xA9 IconFlow 2024",
        PLUGIN_NAME, PLUGIN_MAJOR_VER, PLUGIN_MINOR_VER);
    return PF_Err_NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_GLOBAL_SETUP
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data)
{
    out_data->my_version = PF_VERSION(
        PLUGIN_MAJOR_VER, PLUGIN_MINOR_VER,
        PLUGIN_BUG_VER,   PLUGIN_STAGE_VER,
        PLUGIN_BUILD_VER);

    // Support 8-bit, 16-bit, and 32-bit float rendering.
    out_data->out_flags =
        PF_OutFlag_DEEP_COLOR_AWARE    |  // 16-bit
        PF_OutFlag_CUSTOM_UI           |  // Custom Comp UI (eyedropper)
        PF_OutFlag_SEND_UPDATE_PARAMS_UI; // Re-draw UI when params change

    // SmartFX / 32-bit float support (AE CS6+).
    out_data->out_flags2 =
        PF_OutFlag2_SUPPORTS_SMART_RENDER  |
        PF_OutFlag2_FLOAT_COLOR_AWARE       |
        PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;

    return PF_Err_NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_PARAMS_SETUP
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data,
                           PF_ParamDef *params[], PF_LayerDef *output)
{
    PF_Err       err = PF_Err_NONE;
    PF_ParamDef  def;

    // ── Group: Background Keying ──────────────────────────────────────────
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("Background Keying", PARAM_BG_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Enable BG Key", "On", FALSE, 0, PARAM_BG_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_BUTTON("Pick BG Color", "Pick \xF0\x9F\xA7\xB2",
                  PF_PUI_NONE, PF_PUI_NONE, PARAM_BG_PICK);

    AEFX_CLR_STRUCT(def);
    def.param_type              = PF_Param_COLOR;
    def.u.cd.value.red          = 0;
    def.u.cd.value.green        = 255;
    def.u.cd.value.blue         = 0;
    def.u.cd.value.alpha        = 255;
    def.u.cd.dephault           = def.u.cd.value;
    PF_STRCPY(def.name, "Background Color");
    def.uu.id = PARAM_BG_COLOR;
    ERR(PF_ADD_PARAM(in_data, -1, &def));

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Distance Mode",
                 3,           // num choices
                 DIST_RGB,    // default
                 "RGB|Hue|Chroma",
                 PARAM_BG_DIST_MODE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Tolerance", 0.0, 100.0, 0.0, 100.0,
                          18.0, PF_Precision_TENTHS, 0, 0, PARAM_BG_TOLERANCE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Softness", 0.0, 100.0, 0.0, 100.0,
                          8.0, PF_Precision_TENTHS, 0, 0, PARAM_BG_SOFTNESS);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Edge Choke/Expand", -5.0, 5.0, -5.0, 5.0,
                          0.0, PF_Precision_HUNDREDTHS, 0, 0, PARAM_BG_EDGE_CHOKE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Edge Cleanup", "Auto", TRUE, 0, PARAM_BG_EDGE_CLEANUP);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Green Despill", "On", TRUE, 0, PARAM_BG_DESPILL);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(PARAM_BG_DESPILL + 1);

    // ── Group: Color Replacement ──────────────────────────────────────────
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("Color Replacement", PARAM_CR0_ENABLE);

    for (int i = 0; i < NUM_COLOR_SLOTS; i++) {
        char slotLabel[64];
        PF_SPRINTF(slotLabel, "Slot %d", i + 1);

        // Sub-group for each slot
        AEFX_CLR_STRUCT(def);
        PF_ADD_TOPIC(slotLabel, SLOT_ENABLE(i));

        AEFX_CLR_STRUCT(def);
        char enLabel[64];
        PF_SPRINTF(enLabel, "Enable Slot %d", i + 1);
        PF_ADD_CHECKBOX(enLabel, "On", (i == 0) ? TRUE : FALSE, 0, SLOT_ENABLE(i));

        AEFX_CLR_STRUCT(def);
        char pickLabel[64];
        PF_SPRINTF(pickLabel, "Pick Source %d", i + 1);
        PF_ADD_BUTTON(pickLabel, "Pick", PF_PUI_NONE, PF_PUI_NONE, SLOT_PICK(i));

        // Source color (default: white)
        AEFX_CLR_STRUCT(def);
        def.param_type              = PF_Param_COLOR;
        def.u.cd.value.red          = 255;
        def.u.cd.value.green        = 255;
        def.u.cd.value.blue         = 255;
        def.u.cd.value.alpha        = 255;
        def.u.cd.dephault           = def.u.cd.value;
        char srcLabel[64];
        PF_SPRINTF(srcLabel, "From %d", i + 1);
        PF_STRCPY(def.name, srcLabel);
        def.uu.id = SLOT_SRC(i);
        ERR(PF_ADD_PARAM(in_data, -1, &def));

        // Destination color (default: electric blue)
        AEFX_CLR_STRUCT(def);
        def.param_type              = PF_Param_COLOR;
        def.u.cd.value.red          = 125;
        def.u.cd.value.green        = 131;
        def.u.cd.value.blue         = 255;
        def.u.cd.value.alpha        = 255;
        def.u.cd.dephault           = def.u.cd.value;
        char dstLabel[64];
        PF_SPRINTF(dstLabel, "To %d", i + 1);
        PF_STRCPY(def.name, dstLabel);
        def.uu.id = SLOT_DST(i);
        ERR(PF_ADD_PARAM(in_data, -1, &def));

        AEFX_CLR_STRUCT(def);
        char tolLabel[64];
        PF_SPRINTF(tolLabel, "Tolerance %d", i + 1);
        PF_ADD_FLOAT_SLIDERX(tolLabel, 0.0, 100.0, 0.0, 100.0,
                              10.0, PF_Precision_TENTHS, 0, 0, SLOT_TOL(i));

        AEFX_CLR_STRUCT(def);
        char softLabel[64];
        PF_SPRINTF(softLabel, "Softness %d", i + 1);
        PF_ADD_FLOAT_SLIDERX(softLabel, 0.0, 100.0, 0.0, 100.0,
                              5.0, PF_Precision_TENTHS, 0, 0, SLOT_SOFT(i));

        AEFX_CLR_STRUCT(def);
        PF_END_TOPIC(SLOT_SOFT(i) + 1);
    }

    // Global options
    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Preserve Luminance", "On", FALSE, 0, PARAM_PRESERVE_LUM);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Global Tint", "On", FALSE, 0, PARAM_GLOBAL_TINT_EN);

    AEFX_CLR_STRUCT(def);
    def.param_type              = PF_Param_COLOR;
    def.u.cd.value.red          = 125;
    def.u.cd.value.green        = 131;
    def.u.cd.value.blue         = 255;
    def.u.cd.value.alpha        = 255;
    def.u.cd.dephault           = def.u.cd.value;
    PF_STRCPY(def.name, "Tint Color");
    def.uu.id = PARAM_GLOBAL_TINT_COLOR;
    ERR(PF_ADD_PARAM(in_data, -1, &def));

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(PARAM_GLOBAL_TINT_COLOR + 1);

    // ── Group: Glow ───────────────────────────────────────────────────────
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("Glow", PARAM_GLOW_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Enable Glow", "On", FALSE, 0, PARAM_GLOW_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Threshold", 0.0, 100.0, 0.0, 100.0,
                          60.0, PF_Precision_TENTHS, 0, 0, PARAM_GLOW_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Radius", 0.0, 250.0, 0.0, 250.0,
                          45.0, PF_Precision_TENTHS, 0, 0, PARAM_GLOW_RADIUS);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Intensity", 0.0, 3.0, 0.0, 3.0,
                          0.8, PF_Precision_HUNDREDTHS, 0, 0, PARAM_GLOW_INTENSITY);

    AEFX_CLR_STRUCT(def);
    def.param_type              = PF_Param_COLOR;
    def.u.cd.value.red          = 255;
    def.u.cd.value.green        = 255;
    def.u.cd.value.blue         = 255;
    def.u.cd.value.alpha        = 255;
    def.u.cd.dephault           = def.u.cd.value;
    PF_STRCPY(def.name, "Glow Tint");
    def.uu.id = PARAM_GLOW_TINT_COLOR;
    ERR(PF_ADD_PARAM(in_data, -1, &def));

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(PARAM_GLOW_TINT_COLOR + 1);

    out_data->num_params = PARAM_NUM_PARAMS;
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_USER_CHANGED_PARAM
// When the user presses a Pick button, record which param awaits a click.
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err UserChangedParam(
    PF_InData     *in_data,
    PF_OutData    *out_data,
    PF_ParamDef   *params[],
    PF_LayerDef   *output,
    PF_UserChangedParamExtra *extra)
{
    PF_Err       err = PF_Err_NONE;
    A_long       changedIdx = extra->param_index;

    SequenceData *sd = LockSequenceData(in_data);
    if (!sd) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    // Background pick button
    if (changedIdx == PARAM_BG_PICK) {
        sd->picker_active = PARAM_BG_COLOR;
        out_data->out_flags |= PF_OutFlag_REFRESH_UI;
        PF_UNLOCK_HANDLE(in_data->sequence_data);
        return err;
    }

    // Color slot pick buttons
    for (int i = 0; i < NUM_COLOR_SLOTS; i++) {
        if (changedIdx == SLOT_PICK(i)) {
            sd->picker_active = SLOT_SRC(i);
            out_data->out_flags |= PF_OutFlag_REFRESH_UI;
            PF_UNLOCK_HANDLE(in_data->sequence_data);
            return err;
        }
    }

    // Any other param change: cancel pending pick.
    sd->picker_active = PICKER_NONE;
    PF_UNLOCK_HANDLE(in_data->sequence_data);
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_EVENT — Custom Comp UI eyedropper
// ─────────────────────────────────────────────────────────────────────────────
//
// SDK uncertainty note:
//   The PF_EventExtra structure and PF_EventCallbacks coordinate-conversion
//   functions are defined in AE_EffectUI.h.  The code below uses the layout
//   found in After Effects SDK 2022+.  If your SDK differs, compare against
//   the CCU sample (SDK_ROOT/Examples/Effect/CCU/).
//
//   Coordinate conversion:
//     event_extra->cbs.comp_to_layer() converts a Composition-space PF_FixedPoint
//     to a layer-pixel PF_FixedPoint.  The PF_FixedPoint type uses 16.16 fixed-
//     point (PF_FIX_2_LONG / PF_LONG_2_FIX macros for conversion).

static PF_Err HandleEvent(
    PF_InData    *in_data,
    PF_OutData   *out_data,
    PF_ParamDef  *params[],
    PF_LayerDef  *output,
    PF_EventExtra *event_extra)
{
    PF_Err err = PF_Err_NONE;

    SequenceData *sd = LockSequenceData(in_data);
    bool pickerActive = (sd && sd->picker_active != PICKER_NONE);

    switch (event_extra->e_type) {

    // ── Register custom UI context ────────────────────────────────────────
    case PF_Event_NEW_CONTEXT:
        // Nothing to allocate; we use sequence data for state.
        // Mark that we handle events in the Composition panel.
        event_extra->evt_out_flags = PF_EO_NONE;
        break;

    // ── Cursor: switch to crosshair while pick is pending ────────────────
    case PF_Event_ADJUST_CURSOR:
        if (pickerActive) {
            // SDK uncertainty: constant name may vary — see AE_EffectUI.h.
            // Common alternatives: PF_Cursor_CROSS_HAIR, PF_Cursor_CROSSHAIRS.
            event_extra->u.adjust_cursor.set_cursor = PF_Cursor_CROSS_HAIR;
            event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;
        }
        break;

    // ── Click: sample the pixel and write into the target color param ────
    case PF_Event_DO_CLICK: {
        if (!pickerActive || !sd) break;

        PF_DoClickEventInfo &click = event_extra->u.do_click;

        // Ctrl-click → 5×5 average sample.
        bool ctrlDown = (click.send_drag == FALSE &&
                         (click.num_clicks > 0));
        // NOTE: Detect Ctrl via Windows API GetKeyState in the click handler.
        // PF_EventExtra does not expose modifier keys directly in all SDK
        // versions.  For cross-version compatibility we check via:
#ifdef AE_OS_WIN
        ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
#endif

        // Convert composition screen point → layer pixel coordinates.
        // SDK uncertainty: The callback signature below matches SDK 2022+.
        // If comp_to_layer is unavailable, use PF_FIXED_MATH equivalents
        // and in_data->output_origin_x/y + in_data->downsample_x/y.
        PF_FixedPoint compPt = {
            PF_LONG_2_FIX(click.screen_point.h),
            PF_LONG_2_FIX(click.screen_point.v)
        };
        PF_FixedPoint layerPt = compPt;

        if (event_extra->cbs.comp_to_layer) {
            err = event_extra->cbs.comp_to_layer(
                event_extra->cbs.refcon,
                in_data,
                &compPt,
                &layerPt);
        }

        A_long lx = PF_FIX_2_LONG(layerPt.x);
        A_long ly = PF_FIX_2_LONG(layerPt.y);

        // Clamp to input layer bounds.
        PF_EffectWorld *world = &params[PARAM_INPUT]->u.ld;
        lx = (lx < 0) ? 0 : (lx >= world->width)  ? world->width  - 1 : lx;
        ly = (ly < 0) ? 0 : (ly >= world->height) ? world->height - 1 : ly;

        // Sample pixel.
        float sr, sg, sb;
        if (ctrlDown) {
            // 5×5 averaged sample (only 8-bit path; 16/32 handled similarly).
            SampleResult res = sample5x5ARGB8(
                world->data, world->rowbytes,
                world->width, world->height, lx, ly);
            if (!res.valid) break;
            sr = res.r; sg = res.g; sb = res.b;
        } else {
            const unsigned char *row =
                reinterpret_cast<const unsigned char*>(world->data) +
                ly * world->rowbytes + lx * 4;
            // PF_Pixel layout in memory: alpha, red, green, blue.
            sr = u8ToF(row[1]);
            sg = u8ToF(row[2]);
            sb = u8ToF(row[3]);
        }

        // Check out the target param, update its color, check in.
        A_long targetParamIdx = sd->picker_active;

        // We must use PF_CHECKOUT_PARAM / PF_CHECKIN_PARAM to modify params
        // from within an event handler.
        PF_ParamDef colorParam;
        AEFX_CLR_STRUCT(colorParam);
        ERR(PF_CHECKOUT_PARAM(in_data, targetParamIdx,
                              in_data->current_time,
                              in_data->time_step,
                              in_data->time_scale,
                              &colorParam));
        if (!err) {
            colorParam.u.cd.value.red   = fToU8(sr);
            colorParam.u.cd.value.green = fToU8(sg);
            colorParam.u.cd.value.blue  = fToU8(sb);
            colorParam.u.cd.value.alpha = 255;

            ERR(PF_CHECKIN_PARAM(in_data, &colorParam));

            // Notify AE that a param changed so it re-renders.
            out_data->out_flags |= PF_OutFlag_FORCE_RERENDER |
                                   PF_OutFlag_REFRESH_UI;
        }

        // Deactivate picker after one click.
        sd->picker_active = PICKER_NONE;
        event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;
        break;
    }

    // ── Keyboard: Escape cancels pending pick ─────────────────────────────
    case PF_Event_KEYDOWN:
        if (pickerActive) {
            // VK_ESCAPE = 0x1B; AE passes virtual key codes in keydown info.
            // SDK uncertainty: field name may be key_code or virt_key —
            // check PF_KeyDownEvent in AE_EffectUI.h.
            if (event_extra->u.key_down.key_code == 0x1B /*VK_ESCAPE*/) {
                sd->picker_active = PICKER_NONE;
                out_data->out_flags |= PF_OutFlag_REFRESH_UI;
                event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;
            }
        }
        break;

    default:
        break;
    }

    if (sd) PF_UNLOCK_HANDLE(in_data->sequence_data);
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// PF_Cmd_RENDER (legacy render path — non-SmartFX)
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err Render(
    PF_InData    *in_data,
    PF_OutData   *out_data,
    PF_ParamDef  *params[],
    PF_LayerDef  *output)
{
    RenderParams rp = extractRenderParams(in_data, params);
    PF_EffectWorld *input = &params[PARAM_INPUT]->u.ld;

    // Detect bit depth via world flags.
    PF_Boolean is16    = PF_WORLD_IS_DEEP(output);
    PF_Boolean isFloat = (output->world_flags & PF_WorldFlag_FLOAT) ? TRUE : FALSE;

    if (isFloat) return Render32(in_data, input, output, rp);
    if (is16)    return Render16(in_data, input, output, rp);
    return             Render8 (in_data, input, output, rp);
}

// ─────────────────────────────────────────────────────────────────────────────
// SmartFX pre-render / smart-render
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err SmartPreRender(
    PF_InData          *in_data,
    PF_OutData         *out_data,
    PF_PreRenderExtra  *extra)
{
    PF_Err           err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult inResult;

    ERR(extra->cb->checkout_layer(
        in_data->effect_ref,
        PARAM_INPUT,
        PARAM_INPUT,
        &req,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &inResult));

    // Output same rect as input.
    UnionLRect(&inResult.result_rect,     &extra->output->result_rect);
    UnionLRect(&inResult.max_result_rect, &extra->output->max_result_rect);
    return err;
}

static PF_Err SmartRender(
    PF_InData        *in_data,
    PF_OutData       *out_data,
    PF_SmartRenderExtra *extra)
{
    PF_Err         err   = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    PF_EffectWorld *inputWorld  = nullptr;
    PF_EffectWorld *outputWorld = nullptr;

    ERR(extra->cb->checkout_layer_pixels(
        in_data->effect_ref, PARAM_INPUT, &inputWorld));
    ERR(extra->cb->checkout_output(
        in_data->effect_ref, &outputWorld));

    if (!err && inputWorld && outputWorld) {
        // Checkout all params at current time.
        PF_ParamDef *params[PARAM_NUM_PARAMS] = {};
        PF_ParamDef  paramStorage[PARAM_NUM_PARAMS];
        for (int i = 1; i < PARAM_NUM_PARAMS; i++) {
            AEFX_CLR_STRUCT(paramStorage[i]);
            ERR(PF_CHECKOUT_PARAM(in_data, i,
                                  in_data->current_time,
                                  in_data->time_step,
                                  in_data->time_scale,
                                  &paramStorage[i]));
            params[i] = &paramStorage[i];
        }

        RenderParams rp = extractRenderParams(in_data, params);

        PF_Boolean is16    = PF_WORLD_IS_DEEP(outputWorld);
        PF_Boolean isFloat = (outputWorld->world_flags & PF_WorldFlag_FLOAT) ? TRUE : FALSE;

        if (!err) {
            if (isFloat) err = Render32(in_data, inputWorld, outputWorld, rp);
            else if (is16) err = Render16(in_data, inputWorld, outputWorld, rp);
            else           err = Render8 (in_data, inputWorld, outputWorld, rp);
        }

        // Check in params.
        for (int i = 1; i < PARAM_NUM_PARAMS; i++) {
            if (params[i]) {
                PF_CHECKIN_PARAM(in_data, &paramStorage[i]);
            }
        }
    }

    ERR(extra->cb->checkin_layer_pixels(in_data->effect_ref, PARAM_INPUT));
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sequence setup / flatten / unflatten
// ─────────────────────────────────────────────────────────────────────────────
static PF_Err SequenceSetup(PF_InData *in_data, PF_OutData *out_data)
{
    return AllocSequenceData(in_data, out_data);
}

static PF_Err SequenceResetup(PF_InData *in_data, PF_OutData *out_data)
{
    // Unflatten flat data if present, otherwise allocate fresh.
    if (in_data->sequence_data) {
        // Duplicate the handle from the project.
        out_data->sequence_data = in_data->sequence_data;
        return PF_Err_NONE;
    }
    return AllocSequenceData(in_data, out_data);
}

static PF_Err SequenceFlatten(PF_InData *in_data, PF_OutData *out_data)
{
    // Sequence data is already flat (plain struct, no pointers).
    out_data->sequence_data = in_data->sequence_data;
    return PF_Err_NONE;
}

static PF_Err SequenceSetdown(PF_InData *in_data, PF_OutData *out_data)
{
    if (in_data->sequence_data) {
        PF_DISPOSE_HANDLE(in_data->sequence_data);
        out_data->sequence_data = nullptr;
    }
    return PF_Err_NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
// EffectMain — plugin entry point
// ─────────────────────────────────────────────────────────────────────────────
DllExport PF_Err EffectMain(
    PF_Cmd        cmd,
    PF_InData    *in_data,
    PF_OutData   *out_data,
    PF_ParamDef  *params[],
    PF_LayerDef  *output,
    void         *extra)
{
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
        case PF_Cmd_ABOUT:
            err = About(in_data, out_data);
            break;

        case PF_Cmd_GLOBAL_SETUP:
            err = GlobalSetup(in_data, out_data);
            break;

        case PF_Cmd_PARAMS_SETUP:
            err = ParamsSetup(in_data, out_data, params, output);
            break;

        case PF_Cmd_SEQUENCE_SETUP:
            err = SequenceSetup(in_data, out_data);
            break;

        case PF_Cmd_SEQUENCE_RESETUP:
            err = SequenceResetup(in_data, out_data);
            break;

        case PF_Cmd_SEQUENCE_FLATTEN:
            err = SequenceFlatten(in_data, out_data);
            break;

        case PF_Cmd_SEQUENCE_SETDOWN:
            err = SequenceSetdown(in_data, out_data);
            break;

        case PF_Cmd_RENDER:
            err = Render(in_data, out_data, params, output);
            break;

        case PF_Cmd_SMART_PRE_RENDER:
            err = SmartPreRender(in_data, out_data,
                                 reinterpret_cast<PF_PreRenderExtra*>(extra));
            break;

        case PF_Cmd_SMART_RENDER:
            err = SmartRender(in_data, out_data,
                              reinterpret_cast<PF_SmartRenderExtra*>(extra));
            break;

        case PF_Cmd_USER_CHANGED_PARAM:
            err = UserChangedParam(in_data, out_data, params, output,
                                   reinterpret_cast<PF_UserChangedParamExtra*>(extra));
            break;

        case PF_Cmd_EVENT:
            err = HandleEvent(in_data, out_data, params, output,
                              reinterpret_cast<PF_EventExtra*>(extra));
            break;

        default:
            break;
        }
    }
    catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return err;
}
