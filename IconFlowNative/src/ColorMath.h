/**
 * ColorMath.h
 * Pure color-math functions — no AE SDK dependencies.
 * All functions operate in linear float (0.0–1.0) space.
 * These are independently unit-tested (see tests/ColorMathTests.cpp).
 */
#pragma once
#include <cmath>
#include <algorithm>

// Distance modes are part of the SDK-independent color-math API so that
// both the native effect and its standalone unit tests share one definition.
enum DistMode {
    DIST_RGB    = 1,
    DIST_HUE    = 2,
    DIST_CHROMA = 3
};

// ── Float color ───────────────────────────────────────────────────────────────
struct ColorF {
    float r, g, b, a;
};

// ── Bit-depth conversion helpers ─────────────────────────────────────────────
inline float u8ToF(unsigned char  v) { return v  / 255.0f; }
inline float u16ToF(unsigned short v) { return v / 32768.0f; }  // AE 16-bit max = 32768
inline unsigned char  fToU8 (float v) {
    int i = static_cast<int>(v * 255.0f + 0.5f);
    return static_cast<unsigned char>(i < 0 ? 0 : i > 255 ? 255 : i);
}
inline unsigned short fToU16(float v) {
    int i = static_cast<int>(v * 32768.0f + 0.5f);
    return static_cast<unsigned short>(i < 0 ? 0 : i > 32768 ? 32768 : i);
}
inline float clampF(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

// ── RGB ↔ HSL ────────────────────────────────────────────────────────────────
struct HSL { float h, s, l; };

inline HSL rgbToHsl(float r, float g, float b) {
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    float l  = (mx + mn) * 0.5f;
    if (mx == mn) return {0.0f, 0.0f, l};

    float d  = mx - mn;
    float s  = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
    float h  = 0.0f;
    if      (mx == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else              h = (r - g) / d + 4.0f;
    h /= 6.0f;
    return {h, s, l};
}

// ── Color distance ────────────────────────────────────────────────────────────
//
// Returns a value in [0, 1] representing how different two colors are.
// mode: 1=RGB Euclidean, 2=Hue only, 3=Chroma (saturation × hue).

inline float colorDistance(
    float r1, float g1, float b1,
    float r2, float g2, float b2,
    int   mode)
{
    if (mode == 1 /*RGB*/) {
        float dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
        return std::sqrt(dr*dr + dg*dg + db*db) / 1.732050808f; // normalise by sqrt(3)
    }
    if (mode == 2 /*Hue*/) {
        HSL a = rgbToHsl(r1, g1, b1);
        HSL b = rgbToHsl(r2, g2, b2);
        float dh = std::abs(a.h - b.h);
        if (dh > 0.5f) dh = 1.0f - dh;
        return dh * 2.0f; // 0..1
    }
    // mode == 3 — Chroma: weight hue by saturation of reference
    {
        HSL a = rgbToHsl(r1, g1, b1);
        HSL b = rgbToHsl(r2, g2, b2);
        float dh = std::abs(a.h - b.h);
        if (dh > 0.5f) dh = 1.0f - dh;
        float ds = std::abs(a.s - b.s);
        return (dh * b.s * 1.5f + ds * 0.5f);
    }
}

// ── Smooth step (s-curve falloff) ─────────────────────────────────────────────
inline float smoothStep(float edge0, float edge1, float x) {
    if (edge1 <= edge0) return x < edge0 ? 1.0f : 0.0f;
    float t = clampF((x - edge0) / (edge1 - edge0));
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

// ── Background matte factor ───────────────────────────────────────────────────
//
// Returns alpha multiplier for the pixel:
//   1.0 = keep pixel (foreground)
//   0.0 = discard pixel (matched background)
inline float bgMatteFactor(
    float pr, float pg, float pb,
    float br, float bg, float bb,
    int   distMode,
    float tolerance,   // 0..1
    float softness)    // 0..1
{
    float dist = colorDistance(pr, pg, pb, br, bg, bb, distMode);
    // A close match is keyed out (0); a distant pixel is kept (1).
    if (dist <= tolerance) return 0.0f;

    float transition = std::max(softness, 0.0f);
    if (transition <= 0.0f || dist >= tolerance + transition) {
        return 1.0f;
    }

    float t = (dist - tolerance) / transition;
    return t * t * (3.0f - 2.0f * t);
}

// ── Green spill suppressor ────────────────────────────────────────────────────
//
// Reduces green channel to max(red, blue) — standard de-spill formula.
inline void greenDespill(float &r, float &g, float &b, float alpha) {
    float maxRB = std::max(r, b);
    if (g > maxRB) {
        float excess = g - maxRB;
        g -= excess * alpha;
    }
}

// ── Edge choke / expand ───────────────────────────────────────────────────────
//
// Applied as a post-process after alpha keying in the renderer.
// Simple alpha erosion (choke > 0) or dilation (choke < 0) via radius scaling.
// Full dilation/erosion requires a separate pass — the Renderer does this.
// chokeRadius in [−5, +5] pixels.

// ── Luminance ─────────────────────────────────────────────────────────────────
inline float luminance(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// ── Color replacement factor ──────────────────────────────────────────────────
//
// Returns blend weight (0 = no change, 1 = full replace) for this pixel
// given a source color and tolerance/softness.
inline float colorReplaceFactor(
    float pr, float pg, float pb,
    float sr, float sg, float sb,
    float tolerance,  // 0..1
    float softness)   // 0..1
{
    float dist = colorDistance(pr, pg, pb, sr, sg, sb, 1 /*RGB*/);
    // An exact/near source match is fully replaced (1); a distant pixel is
    // left unchanged (0). Handle zero softness explicitly so an exact match
    // remains replaceable instead of falling through a degenerate step.
    if (dist <= tolerance) return 1.0f;

    float transition = std::max(softness, 0.0f);
    if (transition <= 0.0f || dist >= tolerance + transition) {
        return 0.0f;
    }

    float t = (dist - tolerance) / transition;
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

// ── Apply color replacement ───────────────────────────────────────────────────
//
// Blends pixel toward dstColor by factor w, optionally preserving luminance.
inline void applyColorReplace(
    float &r, float &g, float &b,
    float dr, float dg, float db,
    float w,
    bool  preserveLum)
{
    if (w <= 0.0f) return;
    float origLum = luminance(r, g, b);
    r = r * (1.0f - w) + dr * w;
    g = g * (1.0f - w) + dg * w;
    b = b * (1.0f - w) + db * w;
    if (preserveLum) {
        float newLum = luminance(r, g, b);
        if (newLum > 1e-4f) {
            float scale = origLum / newLum;
            r = clampF(r * scale);
            g = clampF(g * scale);
            b = clampF(b * scale);
        }
    }
}

// ── Global tint ───────────────────────────────────────────────────────────────
//
// Tints the pixel toward tintColor while preserving luminance.
inline void applyGlobalTint(
    float &r, float &g, float &b,
    float tr, float tg, float tb)
{
    float lum = luminance(r, g, b);
    r = clampF(tr * lum);
    g = clampF(tg * lum);
    b = clampF(tb * lum);
}

// ── 5×5 average sample helper ────────────────────────────────────────────────
//
// Called by the picker code — averages a 5×5 block centered at (cx, cy).
// Data must be a contiguous ARGB8 (PF_Pixel) raster.
// Returns averaged (r, g, b) in float [0..1].
struct SampleResult { float r, g, b; bool valid; };

inline SampleResult sample5x5ARGB8(
    const void* data, int rowbytes,
    int width, int height,
    int cx, int cy)
{
    double sumR = 0, sumG = 0, sumB = 0;
    int    count = 0;
    for (int dy = -2; dy <= 2; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= height) continue;
        for (int dx = -2; dx <= 2; dx++) {
            int x = cx + dx;
            if (x < 0 || x >= width) continue;
            // AE PF_Pixel layout: alpha, red, green, blue (ARGB in memory)
            const unsigned char* row =
                reinterpret_cast<const unsigned char*>(data) + y * rowbytes;
            const unsigned char* px = row + x * 4;
            sumR += px[1]; sumG += px[2]; sumB += px[3];
            count++;
        }
    }
    if (count == 0) return {0, 0, 0, false};
    return {
        static_cast<float>(sumR / count / 255.0),
        static_cast<float>(sumG / count / 255.0),
        static_cast<float>(sumB / count / 255.0),
        true
    };
}
