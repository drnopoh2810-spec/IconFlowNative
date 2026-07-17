/**
 * Renderer.cpp
 * Multi-bit-depth rendering pipeline for IconFlow Native Picker.
 *
 * Process order per spec:
 *   1. BG key (alpha matte)
 *   2. Edge cleanup / choke / de-spill
 *   3. Color mappings
 *   4. Global tint
 *   5. Glow
 *
 * No AE SDK background-thread calls are made here; this runs on the
 * render thread that AE provides.
 */
#include "Renderer.h"
#include "ColorMath.h"
#include "IconFlowNative.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Helper: extract color from PF_ParamDef COLOR param
// ─────────────────────────────────────────────────────────────────────────────
static void extractColor(PF_ParamDef *p, float &r, float &g, float &b)
{
    // AE COLOR params store PF_Pixel values (0-255 each channel).
    r = u8ToF(p->u.cd.value.red);
    g = u8ToF(p->u.cd.value.green);
    b = u8ToF(p->u.cd.value.blue);
}

// ─────────────────────────────────────────────────────────────────────────────
// extractRenderParams
// ─────────────────────────────────────────────────────────────────────────────
RenderParams extractRenderParams(PF_InData *in_data, PF_ParamDef *params[])
{
    RenderParams rp = {};

    // Background
    rp.bgEnabled     = (params[PARAM_BG_ENABLE]->u.bd.value != 0);
    extractColor(params[PARAM_BG_COLOR], rp.bgR, rp.bgG, rp.bgB);
    rp.bgDistMode    = (int)params[PARAM_BG_DIST_MODE]->u.pd.value; // 1,2,3
    rp.bgTolerance   = (float)params[PARAM_BG_TOLERANCE]->u.fs_d.value / 100.0f;
    rp.bgSoftness    = (float)params[PARAM_BG_SOFTNESS]->u.fs_d.value  / 100.0f;
    rp.bgEdgeChoke   = (float)params[PARAM_BG_EDGE_CHOKE]->u.fs_d.value;
    rp.bgEdgeCleanup = (params[PARAM_BG_EDGE_CLEANUP]->u.bd.value != 0);
    rp.bgDespill     = (params[PARAM_BG_DESPILL]->u.bd.value != 0);

    // Color slots
    for (int i = 0; i < NUM_COLOR_SLOTS; i++) {
        ColorSlotParams &s = rp.slots[i];
        s.enabled   = (params[SLOT_ENABLE(i)]->u.bd.value != 0);
        extractColor(params[SLOT_SRC(i)], s.srcR, s.srcG, s.srcB);
        extractColor(params[SLOT_DST(i)], s.dstR, s.dstG, s.dstB);
        s.tolerance = (float)params[SLOT_TOL(i)]->u.fs_d.value  / 100.0f;
        s.softness  = (float)params[SLOT_SOFT(i)]->u.fs_d.value / 100.0f;
    }

    // Global options
    rp.preserveLum        = (params[PARAM_PRESERVE_LUM]->u.bd.value    != 0);
    rp.globalTintEnabled  = (params[PARAM_GLOBAL_TINT_EN]->u.bd.value  != 0);
    extractColor(params[PARAM_GLOBAL_TINT_COLOR], rp.tintR, rp.tintG, rp.tintB);

    // Glow
    rp.glowEnabled    = (params[PARAM_GLOW_ENABLE]->u.bd.value != 0);
    rp.glowThreshold  = (float)params[PARAM_GLOW_THRESHOLD]->u.fs_d.value / 100.0f;
    rp.glowRadius     = (float)params[PARAM_GLOW_RADIUS]->u.fs_d.value;
    rp.glowIntensity  = (float)params[PARAM_GLOW_INTENSITY]->u.fs_d.value;
    extractColor(params[PARAM_GLOW_TINT_COLOR], rp.glowTintR, rp.glowTintG, rp.glowTintB);

    return rp;
}

// ─────────────────────────────────────────────────────────────────────────────
// ProcessPixelF — full pipeline for one float pixel
// ─────────────────────────────────────────────────────────────────────────────
void ProcessPixelF(
    float &r, float &g, float &b, float &a,
    const RenderParams &rp)
{
    // Guard: fully transparent pixels pass through unchanged.
    if (a <= 0.0f) return;

    // ── Step 1: Background key ────────────────────────────────────────────
    if (rp.bgEnabled) {
        float matteFactor = bgMatteFactor(
            r, g, b,
            rp.bgR, rp.bgG, rp.bgB,
            rp.bgDistMode,
            rp.bgTolerance,
            rp.bgSoftness);
        a *= matteFactor;
        if (a <= 0.0f) { r = g = b = a = 0.0f; return; }
    }

    // ── Step 2a: De-spill (green spill suppressor) ────────────────────────
    if (rp.bgEnabled && rp.bgDespill) {
        // Only meaningful if user chose a green-ish background.
        // Apply de-spill proportional to how "green" the BG is.
        float greenness = rp.bgG - std::max(rp.bgR, rp.bgB);
        if (greenness > 0.1f) {
            greenDespill(r, g, b, std::min(greenness * 2.0f, 1.0f));
        }
    }

    // ── Step 2b: Edge choke/expand is a spatial operation done per-frame
    //    in the caller after the pixel loop (applied to the alpha channel).
    //    Individual pixel processing cannot implement a true morphological
    //    operation; see glowPass / choke pass in Render8/16/32.

    // ── Step 3: Color mappings ────────────────────────────────────────────
    for (int i = 0; i < NUM_COLOR_SLOTS; i++) {
        const ColorSlotParams &s = rp.slots[i];
        if (!s.enabled) continue;
        float w = colorReplaceFactor(
            r, g, b,
            s.srcR, s.srcG, s.srcB,
            s.tolerance, s.softness);
        if (w > 0.0f) {
            applyColorReplace(r, g, b, s.dstR, s.dstG, s.dstB, w, rp.preserveLum);
        }
    }

    // ── Step 4: Global tint ───────────────────────────────────────────────
    if (rp.globalTintEnabled) {
        applyGlobalTint(r, g, b, rp.tintR, rp.tintG, rp.tintB);
    }

    // Clamp (glow is additive and applied in a separate pass).
    r = clampF(r);
    g = clampF(g);
    b = clampF(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// FloatBuf helpers
// ─────────────────────────────────────────────────────────────────────────────
FloatBuf allocFloatBuf(int w, int h)
{
    FloatBuf buf;
    buf.width  = w;
    buf.height = h;
    buf.data   = new float[w * h * 4]();
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// glowPass — separable Gaussian blur of bright (above threshold) RGB,
//            added back (additive compositing) onto buf.
// ─────────────────────────────────────────────────────────────────────────────
static void gaussianBlur1D(
    const float *src, float *dst,
    int len, int stride,
    float sigma)
{
    // Build kernel (radius = ceil(2.5 * sigma))
    int r = static_cast<int>(std::ceil(2.5f * sigma));
    if (r < 1) r = 1;
    std::vector<float> kernel(2 * r + 1);
    float sum = 0.0f;
    for (int k = -r; k <= r; k++) {
        float v = std::exp(-0.5f * k * k / (sigma * sigma));
        kernel[k + r] = v;
        sum += v;
    }
    for (auto &v : kernel) v /= sum;

    for (int i = 0; i < len; i++) {
        float acc = 0.0f;
        for (int k = -r; k <= r; k++) {
            int j = i + k;
            if (j < 0) j = 0;
            if (j >= len) j = len - 1;
            acc += src[j * stride] * kernel[k + r];
        }
        dst[i * stride] = acc;
    }
}

void glowPass(FloatBuf &buf, const RenderParams &rp)
{
    if (!rp.glowEnabled || rp.glowRadius < 0.5f || rp.glowIntensity <= 0.0f)
        return;

    int W = buf.width, H = buf.height;
    float sigma = rp.glowRadius / 3.0f;

    // Extract "bright" RGB into a separate buffer (glow source).
    std::vector<float> glow(W * H * 4, 0.0f);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int idx = (y * W + x) * 4;
            float r = buf.data[idx + 0];
            float g = buf.data[idx + 1];
            float b = buf.data[idx + 2];
            float a = buf.data[idx + 3];
            float lum = luminance(r, g, b) * a;
            float mask = lum > rp.glowThreshold ? (lum - rp.glowThreshold) / (1.0f - rp.glowThreshold + 1e-5f) : 0.0f;
            glow[idx + 0] = r * mask * rp.glowTintR;
            glow[idx + 1] = g * mask * rp.glowTintG;
            glow[idx + 2] = b * mask * rp.glowTintB;
            glow[idx + 3] = mask;
        }
    }

    // Blur glow source: horizontal pass.
    std::vector<float> tmp(W * H * 4, 0.0f);
    for (int c = 0; c < 3; c++) {
        for (int y = 0; y < H; y++) {
            const float *srcRow = glow.data() + y * W * 4 + c;
            float       *dstRow = tmp.data()  + y * W * 4 + c;
            gaussianBlur1D(srcRow, dstRow, W, 4, sigma);
        }
    }
    // Vertical pass.
    std::vector<float> blurred(W * H * 4, 0.0f);
    for (int c = 0; c < 3; c++) {
        for (int x = 0; x < W; x++) {
            const float *srcCol = tmp.data()     + x * 4 + c;
            float       *dstCol = blurred.data() + x * 4 + c;
            gaussianBlur1D(srcCol, dstCol, H, W * 4, sigma);
        }
    }

    // Additive composite blurred glow back onto buf.
    for (int i = 0; i < W * H; i++) {
        int idx = i * 4;
        buf.data[idx + 0] = clampF(buf.data[idx + 0] + blurred[idx + 0] * rp.glowIntensity);
        buf.data[idx + 1] = clampF(buf.data[idx + 1] + blurred[idx + 1] * rp.glowIntensity);
        buf.data[idx + 2] = clampF(buf.data[idx + 2] + blurred[idx + 2] * rp.glowIntensity);
        // alpha unchanged
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Edge choke/expand spatial pass (morphological alpha erosion/dilation).
// Operates on the float buf alpha channel.
// chokeRadius > 0 → erode (choke); < 0 → dilate (expand).
// ─────────────────────────────────────────────────────────────────────────────
static void edgeChokePass(FloatBuf &buf, float chokeRadius)
{
    if (std::abs(chokeRadius) < 0.01f) return;

    int W = buf.width, H = buf.height;
    int r = static_cast<int>(std::ceil(std::abs(chokeRadius)));
    std::vector<float> alphaIn(W * H);
    std::vector<float> alphaOut(W * H);

    // Extract alpha.
    for (int i = 0; i < W * H; i++)
        alphaIn[i] = buf.data[i * 4 + 3];

    bool erode = (chokeRadius > 0.0f);

    // Simple min/max filter approximation of morphological op.
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float best = erode ? 1.0f : 0.0f;
            for (int dy = -r; dy <= r; dy++) {
                int ny = y + dy; if (ny < 0 || ny >= H) continue;
                for (int dx = -r; dx <= r; dx++) {
                    int nx = x + dx; if (nx < 0 || nx >= W) continue;
                    float d = std::sqrt((float)(dx*dx + dy*dy));
                    if (d > (float)r) continue;
                    float v = alphaIn[ny * W + nx];
                    if (erode)  best = std::min(best, v);
                    else        best = std::max(best, v);
                }
            }
            alphaOut[y * W + x] = best;
        }
    }

    // Write back blended alpha (soft-falloff with chokeRadius fraction).
    float fraction = std::abs(chokeRadius) - std::floor(std::abs(chokeRadius));
    for (int i = 0; i < W * H; i++) {
        float orig = alphaIn[i];
        float filt = alphaOut[i];
        buf.data[i * 4 + 3] = orig + (filt - orig) * (1.0f - fraction + fraction * 0.5f);
        // Also premultiply RGB by new alpha.
        if (orig > 1e-6f) {
            float scale = buf.data[i * 4 + 3] / orig;
            buf.data[i * 4 + 0] *= scale;
            buf.data[i * 4 + 1] *= scale;
            buf.data[i * 4 + 2] *= scale;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render8
// ─────────────────────────────────────────────────────────────────────────────
PF_Err Render8(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp)
{
    PF_Err err = PF_Err_NONE;
    int W = output->width, H = output->height;

    // Allocate float working buffer.
    FloatBuf buf = allocFloatBuf(W, H);
    if (!buf.data) return PF_Err_OUT_OF_MEMORY;

    // Convert input → float, run per-pixel pipeline.
    for (int y = 0; y < H && !err; y++) {
        const PF_Pixel *inRow  = reinterpret_cast<const PF_Pixel*>(
            reinterpret_cast<const char*>(input->data) + y * input->rowbytes);

        for (int x = 0; x < W; x++) {
            float r = u8ToF(inRow[x].red);
            float g = u8ToF(inRow[x].green);
            float b = u8ToF(inRow[x].blue);
            float a = u8ToF(inRow[x].alpha);

            // Un-premultiply.
            if (a > 1e-6f) { r /= a; g /= a; b /= a; }

            ProcessPixelF(r, g, b, a, rp);

            // Re-premultiply.
            int idx = (y * W + x) * 4;
            buf.data[idx + 0] = clampF(r * a);
            buf.data[idx + 1] = clampF(g * a);
            buf.data[idx + 2] = clampF(b * a);
            buf.data[idx + 3] = a;
        }
    }

    // Spatial passes.
    if (rp.bgEnabled) edgeChokePass(buf, rp.bgEdgeChoke);
    glowPass(buf, rp);

    // Write float buffer → AE 8-bit output world.
    for (int y = 0; y < H; y++) {
        PF_Pixel *outRow = reinterpret_cast<PF_Pixel*>(
            reinterpret_cast<char*>(output->data) + y * output->rowbytes);
        for (int x = 0; x < W; x++) {
            int idx = (y * W + x) * 4;
            outRow[x].red   = fToU8(buf.data[idx + 0]);
            outRow[x].green = fToU8(buf.data[idx + 1]);
            outRow[x].blue  = fToU8(buf.data[idx + 2]);
            outRow[x].alpha = fToU8(buf.data[idx + 3]);
        }
    }
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render16
// ─────────────────────────────────────────────────────────────────────────────
PF_Err Render16(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp)
{
    PF_Err err = PF_Err_NONE;
    int W = output->width, H = output->height;

    FloatBuf buf = allocFloatBuf(W, H);
    if (!buf.data) return PF_Err_OUT_OF_MEMORY;

    for (int y = 0; y < H && !err; y++) {
        const PF_Pixel16 *inRow = reinterpret_cast<const PF_Pixel16*>(
            reinterpret_cast<const char*>(input->data) + y * input->rowbytes);

        for (int x = 0; x < W; x++) {
            float r = u16ToF(inRow[x].red);
            float g = u16ToF(inRow[x].green);
            float b = u16ToF(inRow[x].blue);
            float a = u16ToF(inRow[x].alpha);

            if (a > 1e-6f) { r /= a; g /= a; b /= a; }

            ProcessPixelF(r, g, b, a, rp);

            int idx = (y * W + x) * 4;
            buf.data[idx + 0] = clampF(r * a);
            buf.data[idx + 1] = clampF(g * a);
            buf.data[idx + 2] = clampF(b * a);
            buf.data[idx + 3] = a;
        }
    }

    if (rp.bgEnabled) edgeChokePass(buf, rp.bgEdgeChoke);
    glowPass(buf, rp);

    for (int y = 0; y < H; y++) {
        PF_Pixel16 *outRow = reinterpret_cast<PF_Pixel16*>(
            reinterpret_cast<char*>(output->data) + y * output->rowbytes);
        for (int x = 0; x < W; x++) {
            int idx = (y * W + x) * 4;
            outRow[x].red   = fToU16(buf.data[idx + 0]);
            outRow[x].green = fToU16(buf.data[idx + 1]);
            outRow[x].blue  = fToU16(buf.data[idx + 2]);
            outRow[x].alpha = fToU16(buf.data[idx + 3]);
        }
    }
    return err;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render32  (32-bit float worlds)
// PF_PixelFloat layout in AE SDK: alpha, red, green, blue — each PF_FpShort.
// ─────────────────────────────────────────────────────────────────────────────
PF_Err Render32(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp)
{
    PF_Err err = PF_Err_NONE;
    int W = output->width, H = output->height;

    FloatBuf buf = allocFloatBuf(W, H);
    if (!buf.data) return PF_Err_OUT_OF_MEMORY;

    for (int y = 0; y < H && !err; y++) {
        const PF_PixelFloat *inRow = reinterpret_cast<const PF_PixelFloat*>(
            reinterpret_cast<const char*>(input->data) + y * input->rowbytes);

        for (int x = 0; x < W; x++) {
            float r = inRow[x].red;
            float g = inRow[x].green;
            float b = inRow[x].blue;
            float a = inRow[x].alpha;

            // Float worlds are premultiplied in AE.
            if (a > 1e-6f) { r /= a; g /= a; b /= a; }

            ProcessPixelF(r, g, b, a, rp);

            int idx = (y * W + x) * 4;
            buf.data[idx + 0] = r * a;  // premultiply; no clamp (HDR allowed)
            buf.data[idx + 1] = g * a;
            buf.data[idx + 2] = b * a;
            buf.data[idx + 3] = a;
        }
    }

    if (rp.bgEnabled) edgeChokePass(buf, rp.bgEdgeChoke);
    glowPass(buf, rp);

    for (int y = 0; y < H; y++) {
        PF_PixelFloat *outRow = reinterpret_cast<PF_PixelFloat*>(
            reinterpret_cast<char*>(output->data) + y * output->rowbytes);
        for (int x = 0; x < W; x++) {
            int idx = (y * W + x) * 4;
            outRow[x].red   = buf.data[idx + 0];
            outRow[x].green = buf.data[idx + 1];
            outRow[x].blue  = buf.data[idx + 2];
            outRow[x].alpha = buf.data[idx + 3];
        }
    }
    return err;
}
