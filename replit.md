# IconFlow — Adobe After Effects Extension Suite

Two deliverables in this project:

---

## 1. IconFlow Recolor (CEP Panel) — root folder

An Adobe After Effects CEP (Common Extensibility Platform) panel (v1.5.0) for recoloring animated icons. Written in HTML/CSS/JS + ExtendScript.

- **UI**: `index.html`, `css/style.css`, `js/main.js`
- **AE bridge**: `js/CSInterface.js`
- **ExtendScript**: `jsx/hostscript.jsx`
- **Manifest**: `CSXS/manifest.xml`

**Install (Windows):** Copy entire root folder to `%APPDATA%\Adobe\CEP\extensions\IconFlow-Recolor\`, enable CEP debug mode, open AE → `Window > Extensions (Legacy) > IconFlow Recolor`.

---

## 2. IconFlow Native Picker (C++ .aex) — `IconFlowNative/` folder

A production-ready Windows x64 native Adobe After Effects SDK effect plugin. Written in C++17, targets AE 2024 (24.x), MSVC v143.

**Core features:**
- Background keying (RGB/Hue/Chroma distance modes) with edge choke/expand
- 8 independent color-replacement slots with tolerance/softness falloff
- Optional global tint with luminance preservation
- Built-in glow (no Deep Glow dependency)
- Real Composition-panel eyedropper via Custom Comp UI (`PF_OutFlag_CUSTOM_UI`)
  - Crosshair cursor while picking (`PF_Event_ADJUST_CURSOR`)
  - Click → samples pixel from comp, writes to color param (`PF_Event_DO_CLICK`)
  - Ctrl+click → 5×5 averaged sample
  - Escape → cancel pick
- 8-bit, 16-bit, and 32-bit float rendering
- Non-destructive, premultiplied-alpha-correct pipeline

### File structure

```
IconFlowNative/
├── src/
│   ├── IconFlowNative.h        # Param indices, sequence data, entry point
│   ├── IconFlowNative.cpp      # EffectMain, Custom Comp UI, SmartFX
│   ├── ColorMath.h             # Pure color math (no AE SDK, header-only)
│   ├── Renderer.h / .cpp       # 8/16/32-bit rendering pipeline
│   └── IconFlowNative.rc       # Windows resources (PiPL + version)
├── pipl/
│   └── IconFlowNative.r        # PiPL Rez source (→ .rr via PiPLtool)
├── tests/
│   ├── ColorMathTests.cpp      # Standalone unit tests (no AE SDK)
│   ├── ColorMathTests.vcxproj
│   └── CMakeLists.txt
├── cep-bridge/                 # OPTIONAL CEP launcher companion
│   ├── index.html
│   ├── js/bridge.js
│   └── CSXS/manifest.xml
├── IconFlowNative.vcxproj      # VS2022 project
├── IconFlowNative.sln          # VS2022 solution
├── CMakeLists.txt              # CMake alternative
├── configure_windows.ps1       # First-time setup
├── build_release.ps1           # Build + optional test + install
└── README_AR.md                # Full Arabic build guide
```

### Build (Windows only)

**Prerequisites:** VS2022 with "Desktop development with C++", AE SDK, `AE_SDK_ROOT` env var.

```powershell
# 1. Set SDK path (once)
$env:AE_SDK_ROOT = "C:\Adobe\AfterEffectsSDK\2024"

# 2. Configure (generates PiPL .rr, validates tools)
.\IconFlowNative\configure_windows.ps1

# 3. Build
.\IconFlowNative\build_release.ps1

# 4. Build + test + install to AE
.\IconFlowNative\build_release.ps1 -RunTests -InstallToAE
```

Output: `IconFlowNative\build\Release\IconFlowNative.aex`

Install to: `%PROGRAMFILES%\Adobe\Adobe After Effects 2024\Support Files\Plug-ins\IconFlow\`

### SDK API uncertainties (documented in source)

All uncertain API names are called out with `// SDK uncertainty:` comments in `src/IconFlowNative.cpp`. Key ones:
- `PF_EventCallbacks.comp_to_layer` signature — compare against your SDK's `AE_EffectUI.h` and the CCU sample at `SDK_ROOT/Examples/Effect/CCU/`.
- `PF_Cursor_CROSS_HAIR` constant name — grep for "Cursor" in `AE_EffectUI.h`.
- `PF_WorldFlag_FLOAT` for 32-bit detection — grep for "WorldFlag" in `AE_Effect.h`.

## User preferences

- Deliver complete, compilable source with no placeholder code.
- Report SDK API uncertainties explicitly rather than inventing symbols.
