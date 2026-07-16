# IconFlow Recolor

An Adobe After Effects CEP (Common Extensibility Platform) extension panel (v1.5.0) for recoloring animated icons.

## What it does

- Isolates backgrounds (black, green, white, or custom color) using Linear Color Key
- Replaces individual icon colors while preserving animation timing
- Applies glow effects (built-in AE Glow or optional Deep Glow plugin)
- Non-destructive: creates a working copy, hides the original as a backup

## Stack

- **UI**: HTML/CSS/JS panel (`index.html`, `css/style.css`, `js/main.js`)
- **AE bridge**: `js/CSInterface.js` (CEP bridge)
- **ExtendScript**: `jsx/hostscript.jsx` (runs inside After Effects)
- **Manifest**: `CSXS/manifest.xml` (CEP extension definition)

## How to install (Windows)

1. Close After Effects.
2. Copy the entire project folder to:
   `%APPDATA%\Adobe\CEP\extensions\IconFlow-Recolor`
   The manifest must end up at:
   `%APPDATA%\Adobe\CEP\extensions\IconFlow-Recolor\CSXS\manifest.xml`
3. Enable CEP debug mode (run once in PowerShell as a normal user):
   ```powershell
   9..12 | ForEach-Object {
     $key = "HKCU:\Software\Adobe\CSXS.$_"
     New-Item -Path $key -Force | Out-Null
     New-ItemProperty -Path $key -Name PlayerDebugMode -Value 1 -PropertyType String -Force | Out-Null
   }
   ```
4. Open After Effects → `Window > Extensions (Legacy) > IconFlow Recolor`.

## Requirements

- After Effects 2020 or newer (CEP 9+)
- Windows
- Deep Glow plugin (optional — falls back to built-in AE Glow if absent)

## Notes

This extension cannot be "run" on Replit directly — it requires Adobe After Effects to function. Replit is used here for editing the source files.
