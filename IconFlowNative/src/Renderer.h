/**
 * Renderer.h
 * Declares the multi-bit-depth rendering pipeline for IconFlow Native Picker.
 *
 * Process order (non-destructive, per specification):
 *   1. Background key / matte
 *   2. Edge cleanup / choke / de-spill
 *   3. Color mappings (up to NUM_COLOR_SLOTS)
 *   4. Optional global tint
 *   5. Glow
 */
#pragma once
#include "AE_Effect.h"
#include "ColorMath.h"
#include "IconFlowNative.h"

// ── Rendering parameters (extracted from PF_ParamDef[] before render) ────────
struct ColorSlotParams {
    bool  enabled;
    float srcR, srcG, srcB;
    float dstR, dstG, dstB;
    float tolerance;   // 0..1
    float softness;    // 0..1
};

struct RenderParams {
    // Background
    bool    bgEnabled;
    float   bgR, bgG, bgB;
    int     bgDistMode;     // DistMode enum
    float   bgTolerance;    // 0..1
    float   bgSoftness;     // 0..1
    float   bgEdgeChoke;    // pixels, negative=expand, positive=choke
    bool    bgEdgeCleanup;
    bool    bgDespill;

    // Color replacement
    ColorSlotParams slots[NUM_COLOR_SLOTS];
    bool    preserveLum;
    bool    globalTintEnabled;
    float   tintR, tintG, tintB;

    // Glow
    bool    glowEnabled;
    float   glowThreshold;  // 0..1
    float   glowRadius;     // pixels
    float   glowIntensity;  // 0..3
    float   glowTintR, glowTintG, glowTintB;
};

// ── Extract render params from AE param array ─────────────────────────────────
RenderParams extractRenderParams(PF_InData *in_data, PF_ParamDef *params[]);

// ── Entry points for each bit depth ──────────────────────────────────────────
// 8-bit (PF_Pixel: A_u_char a,r,g,b each 0-255)
PF_Err Render8(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp);

// 16-bit (PF_Pixel16: A_u_short16 a,r,g,b each 0-32768)
PF_Err Render16(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp);

// 32-bit float (PF_PixelFloat: PF_FpShort a,r,g,b each 0.0-1.0+)
PF_Err Render32(
    PF_InData     *in_data,
    PF_EffectWorld *input,
    PF_EffectWorld *output,
    const RenderParams &rp);

// ── Internal helper: run full pipeline on one float pixel ────────────────────
void ProcessPixelF(
    float &r, float &g, float &b, float &a,
    const RenderParams &rp);

// ── Glow pass: separable Gaussian blur applied to bright regions ─────────────
//
// Works on a temporary RGBA float buffer allocated on the heap.
// Written back to output after the main pixel loop.
struct FloatBuf {
    float  *data;  // r,g,b,a interleaved, row-major
    int     width, height;
    ~FloatBuf() { delete[] data; }
};

FloatBuf allocFloatBuf(int w, int h);
void glowPass(FloatBuf &buf, const RenderParams &rp);
