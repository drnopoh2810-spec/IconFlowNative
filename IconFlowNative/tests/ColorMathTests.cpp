/**
 * ColorMathTests.cpp
 * Unit tests for pure color-math functions (no AE SDK dependency).
 *
 * Build standalone with CMake (see tests/CMakeLists.txt) or via:
 *   cl /EHsc /I..\src ColorMathTests.cpp /Fe:ColorMathTests.exe
 *   .\ColorMathTests.exe
 *
 * All functions under test live in src/ColorMath.h (header-only).
 */

#include "../src/ColorMath.h"

#include <cstdio>
#include <cmath>
#include <cassert>
#include <cstring>

// ── Minimal test harness ──────────────────────────────────────────────────────
static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr) \
    do { \
        if (expr) { \
            g_passed++; \
        } else { \
            g_failed++; \
            printf("FAIL  line %d: %s\n", __LINE__, #expr); \
        } \
    } while(0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) < (eps))

// ── Helpers ───────────────────────────────────────────────────────────────────
static bool colorNear(float r1, float g1, float b1,
                      float r2, float g2, float b2,
                      float eps = 0.01f)
{
    return std::fabs(r1-r2) < eps &&
           std::fabs(g1-g2) < eps &&
           std::fabs(b1-b2) < eps;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: bit-depth conversion
// ─────────────────────────────────────────────────────────────────────────────
static void test_bitDepthConversion()
{
    printf("── bitDepthConversion\n");

    // u8ToF
    CHECK_NEAR(u8ToF(0),   0.0f, 1e-6f);
    CHECK_NEAR(u8ToF(255), 1.0f, 0.005f);
    CHECK_NEAR(u8ToF(128), 0.502f, 0.005f);

    // u16ToF  (AE 16-bit max = 32768)
    CHECK_NEAR(u16ToF(0),     0.0f, 1e-6f);
    CHECK_NEAR(u16ToF(32768), 1.0f, 1e-5f);
    CHECK_NEAR(u16ToF(16384), 0.5f, 0.001f);

    // fToU8 round-trip
    CHECK(fToU8(0.0f)  == 0);
    CHECK(fToU8(1.0f)  == 255);
    CHECK(fToU8(-0.5f) == 0);   // clamp low
    CHECK(fToU8(1.5f)  == 255); // clamp high
    CHECK(fToU8(0.502f) >= 127 && fToU8(0.502f) <= 129);

    // fToU16 round-trip
    CHECK(fToU16(0.0f)  == 0);
    CHECK(fToU16(1.0f)  == 32768);
    CHECK(fToU16(-1.0f) == 0);
    CHECK(fToU16(2.0f)  == 32768);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: RGB ↔ HSL
// ─────────────────────────────────────────────────────────────────────────────
static void test_rgbToHsl()
{
    printf("── rgbToHsl\n");

    // Black
    HSL h = rgbToHsl(0, 0, 0);
    CHECK_NEAR(h.l, 0.0f, 1e-4f);
    CHECK_NEAR(h.s, 0.0f, 1e-4f);

    // White
    h = rgbToHsl(1, 1, 1);
    CHECK_NEAR(h.l, 1.0f, 1e-4f);
    CHECK_NEAR(h.s, 0.0f, 1e-4f);

    // Pure red
    h = rgbToHsl(1, 0, 0);
    CHECK_NEAR(h.h, 0.0f, 1e-4f);
    CHECK_NEAR(h.s, 1.0f, 1e-4f);
    CHECK_NEAR(h.l, 0.5f, 1e-4f);

    // Pure green
    h = rgbToHsl(0, 1, 0);
    CHECK_NEAR(h.h, 1.0f/3.0f, 0.002f);
    CHECK_NEAR(h.s, 1.0f, 1e-4f);

    // Pure blue
    h = rgbToHsl(0, 0, 1);
    CHECK_NEAR(h.h, 2.0f/3.0f, 0.002f);
    CHECK_NEAR(h.s, 1.0f, 1e-4f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: colorDistance
// ─────────────────────────────────────────────────────────────────────────────
static void test_colorDistance()
{
    printf("── colorDistance\n");

    // Identical colors → distance = 0 (all modes)
    for (int m = 1; m <= 3; m++) {
        CHECK_NEAR(colorDistance(0.5f, 0.5f, 0.5f,
                                 0.5f, 0.5f, 0.5f, m), 0.0f, 1e-5f);
        CHECK_NEAR(colorDistance(1, 0, 0, 1, 0, 0, m), 0.0f, 1e-5f);
    }

    // RGB mode: black vs white = max distance (1.0)
    float d = colorDistance(0, 0, 0, 1, 1, 1, DIST_RGB);
    CHECK_NEAR(d, 1.0f, 0.001f);

    // RGB mode: symmetry
    float d1 = colorDistance(1, 0, 0, 0, 1, 0, DIST_RGB);
    float d2 = colorDistance(0, 1, 0, 1, 0, 0, DIST_RGB);
    CHECK_NEAR(d1, d2, 1e-5f);

    // Hue mode: complementary hues (red vs cyan) are maximally far
    float dHue = colorDistance(1, 0, 0, 0, 1, 1, DIST_HUE);
    CHECK(dHue > 0.4f && dHue <= 1.0f);

    // Hue mode: same hue different saturation → small hue distance
    float dSame = colorDistance(1, 0, 0, 0.5f, 0, 0, DIST_HUE);
    CHECK(dSame < 0.05f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: smoothStep
// ─────────────────────────────────────────────────────────────────────────────
static void test_smoothStep()
{
    printf("── smoothStep\n");

    // Below edge0 → 1 (keep)
    CHECK_NEAR(smoothStep(0.2f, 0.4f, 0.0f), 1.0f, 1e-5f);
    // Above edge1 → 0 (discard)
    CHECK_NEAR(smoothStep(0.2f, 0.4f, 1.0f), 0.0f, 1e-5f);
    // Midpoint → 0.5
    CHECK_NEAR(smoothStep(0.2f, 0.4f, 0.3f), 0.5f, 0.01f);
    // Degenerate equal edges
    CHECK(smoothStep(0.5f, 0.5f, 0.3f) == 1.0f);
    CHECK(smoothStep(0.5f, 0.5f, 0.7f) == 0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: bgMatteFactor
// ─────────────────────────────────────────────────────────────────────────────
static void test_bgMatteFactor()
{
    printf("── bgMatteFactor\n");

    // Pixel exactly equals background → 0 (fully keyed out)
    float f = bgMatteFactor(0, 1, 0,  // pixel = pure green
                            0, 1, 0,  // bg   = pure green
                            DIST_RGB, 0.05f, 0.05f);
    CHECK_NEAR(f, 0.0f, 0.05f);

    // Pixel far from background → 1 (fully kept)
    f = bgMatteFactor(1, 0, 0,  // pixel = red
                      0, 1, 0,  // bg   = green
                      DIST_RGB, 0.1f, 0.1f);
    CHECK_NEAR(f, 1.0f, 0.05f);

    // Tolerance=0, softness=0 → hard threshold
    f = bgMatteFactor(0, 1, 0, 0, 1, 0, DIST_RGB, 0.0f, 0.0f);
    CHECK_NEAR(f, 0.0f, 0.05f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: greenDespill
// ─────────────────────────────────────────────────────────────────────────────
static void test_greenDespill()
{
    printf("── greenDespill\n");

    // Green > max(red, blue): should reduce green
    float r = 0.2f, g = 0.8f, b = 0.2f;
    greenDespill(r, g, b, 1.0f);
    CHECK(g <= 0.21f);  // clamped to max(r,b)
    CHECK_NEAR(r, 0.2f, 1e-5f);  // red unchanged
    CHECK_NEAR(b, 0.2f, 1e-5f);  // blue unchanged

    // Green <= max(red, blue): unchanged
    r = 0.8f; g = 0.5f; b = 0.1f;
    float origG = g;
    greenDespill(r, g, b, 1.0f);
    CHECK_NEAR(g, origG, 1e-5f);

    // Partial despill (alpha=0.5)
    r = 0.2f; g = 0.8f; b = 0.2f;
    greenDespill(r, g, b, 0.5f);
    CHECK(g > 0.2f && g < 0.8f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: colorReplaceFactor
// ─────────────────────────────────────────────────────────────────────────────
static void test_colorReplaceFactor()
{
    printf("── colorReplaceFactor\n");

    // Pixel = source color → fully replace (factor ~1)
    float w = colorReplaceFactor(1, 0, 0, 1, 0, 0, 0.1f, 0.1f);
    CHECK(w > 0.9f);

    // Pixel far from source → no replace (factor ~0)
    w = colorReplaceFactor(0, 0, 1, 1, 0, 0, 0.1f, 0.1f);
    CHECK(w < 0.1f);

    // Tolerance zero, exact match
    w = colorReplaceFactor(1, 0, 0, 1, 0, 0, 0.0f, 0.0f);
    CHECK(w > 0.9f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: applyColorReplace
// ─────────────────────────────────────────────────────────────────────────────
static void test_applyColorReplace()
{
    printf("── applyColorReplace\n");

    // Full replacement: pixel should become dst color
    float r = 1, g = 0, b = 0;
    applyColorReplace(r, g, b, 0, 0, 1, 1.0f, false);
    CHECK(colorNear(r, g, b, 0, 0, 1));

    // No replacement: pixel unchanged
    r = 1; g = 0; b = 0;
    applyColorReplace(r, g, b, 0, 0, 1, 0.0f, false);
    CHECK(colorNear(r, g, b, 1, 0, 0));

    // Preserve luminance for an in-gamut target. A saturated purple cannot
    // retain white's luminance in RGB [0, 1] without clipping, so use a
    // mid-gray source and a lighter purple that can be normalized exactly.
    r = 0.5f; g = 0.5f; b = 0.5f;
    float origLum = luminance(r, g, b);
    applyColorReplace(r, g, b, 0.5f, 0.4f, 0.5f, 1.0f, true);
    float newLum = luminance(r, g, b);
    CHECK_NEAR(newLum, origLum, 0.05f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: applyGlobalTint
// ─────────────────────────────────────────────────────────────────────────────
static void test_applyGlobalTint()
{
    printf("── applyGlobalTint\n");

    // White pixel tinted red: should be red with full luminance
    float r = 1, g = 1, b = 1;
    applyGlobalTint(r, g, b, 1, 0, 0);
    // Tinted red: g and b reduced
    CHECK(g < 0.1f);
    CHECK(b < 0.1f);
    CHECK(r > 0.9f);

    // Black pixel → stays black (luminance=0)
    r = 0; g = 0; b = 0;
    applyGlobalTint(r, g, b, 1, 0, 0);
    CHECK_NEAR(r, 0.0f, 1e-5f);
    CHECK_NEAR(g, 0.0f, 1e-5f);
    CHECK_NEAR(b, 0.0f, 1e-5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: luminance
// ─────────────────────────────────────────────────────────────────────────────
static void test_luminance()
{
    printf("── luminance\n");

    CHECK_NEAR(luminance(0, 0, 0), 0.0f, 1e-6f);
    CHECK_NEAR(luminance(1, 1, 1), 1.0f, 1e-4f);
    // Green contributes most
    CHECK(luminance(0, 1, 0) > luminance(1, 0, 0));
    CHECK(luminance(0, 1, 0) > luminance(0, 0, 1));
    // Rec.709 coefficients
    CHECK_NEAR(luminance(1, 0, 0), 0.2126f, 0.001f);
    CHECK_NEAR(luminance(0, 1, 0), 0.7152f, 0.001f);
    CHECK_NEAR(luminance(0, 0, 1), 0.0722f, 0.001f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: sample5x5ARGB8
// ─────────────────────────────────────────────────────────────────────────────
static void test_sample5x5()
{
    printf("── sample5x5ARGB8\n");

    // Create an 8×8 ARGB8 image, all red pixels.
    const int W = 8, H = 8;
    unsigned char img[H][W][4];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            img[y][x][0] = 255; // alpha
            img[y][x][1] = 200; // red
            img[y][x][2] = 50;  // green
            img[y][x][3] = 10;  // blue
        }

    SampleResult res = sample5x5ARGB8(img, W * 4, W, H, 4, 4);
    CHECK(res.valid);
    CHECK_NEAR(res.r, 200.0f / 255.0f, 0.01f);
    CHECK_NEAR(res.g,  50.0f / 255.0f, 0.01f);
    CHECK_NEAR(res.b,  10.0f / 255.0f, 0.01f);

    // Edge sample (near border) — should clamp, not crash.
    res = sample5x5ARGB8(img, W * 4, W, H, 0, 0);
    CHECK(res.valid);

    // Invalid (empty image)
    res = sample5x5ARGB8(img, W * 4, 0, 0, 0, 0);
    CHECK(!res.valid);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests: clampF
// ─────────────────────────────────────────────────────────────────────────────
static void test_clampF()
{
    printf("── clampF\n");
    CHECK_NEAR(clampF(-1.0f), 0.0f, 1e-6f);
    CHECK_NEAR(clampF(2.0f),  1.0f, 1e-6f);
    CHECK_NEAR(clampF(0.5f),  0.5f, 1e-6f);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    printf("IconFlow Native Picker — Color Math Unit Tests\n");
    printf("================================================\n");

    test_bitDepthConversion();
    test_rgbToHsl();
    test_colorDistance();
    test_smoothStep();
    test_bgMatteFactor();
    test_greenDespill();
    test_colorReplaceFactor();
    test_applyColorReplace();
    test_applyGlobalTint();
    test_luminance();
    test_sample5x5();
    test_clampF();

    printf("================================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);

    return g_failed > 0 ? 1 : 0;
}
