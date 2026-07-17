/**
 * bridge.js — OPTIONAL CEP companion for IconFlow Native Picker.
 *
 * PURPOSE
 * ───────
 * This CEP panel can apply the native IconFlowNative.aex effect to the
 * currently selected layers in After Effects.  It does NOT implement the
 * Composition-panel eyedropper — that capability lives exclusively in the
 * native .aex effect (Custom Comp UI).  This panel is a convenience launcher
 * only.
 *
 * WHAT THIS PANEL CANNOT DO
 * ─────────────────────────
 * - It cannot capture clicks inside the Composition panel.
 * - It cannot sample pixel colors from the Composition via CEP/browser APIs.
 * - All color picking must be done through the native effect's "Pick" buttons.
 *
 * REQUIREMENTS
 * ────────────
 * - IconFlowNative.aex must be installed in After Effects.
 * - CEP 9+ (After Effects 2020 or later).
 */

'use strict';

const cs = new CSInterface();

// ── Apply native effect to selected layers ────────────────────────────────────
function applyNativeEffect() {
    const script = `
(function applyIconFlowNative() {
    var comp = app.project.activeItem;
    if (!comp || !(comp instanceof CompItem)) {
        return "ERROR: No active composition.";
    }
    var layers = comp.selectedLayers;
    if (!layers || layers.length === 0) {
        return "ERROR: No layers selected.";
    }
    var applied = 0;
    var errors  = [];
    app.beginUndoGroup("Apply IconFlow Native Picker");
    for (var i = 0; i < layers.length; i++) {
        var layer = layers[i];
        if (!(layer instanceof AVLayer)) {
            errors.push("Layer \\"" + layer.name + "\\" is not a video layer — skipped.");
            continue;
        }
        try {
            // Match name must match PLUGIN_MATCH_NAME in IconFlowNative.h
            var fx = layer.Effects.addProperty("ADBE IconFlow Native Picker");
            if (!fx) {
                errors.push("Could not apply effect to \\"" + layer.name +
                    "\\". Is IconFlowNative.aex installed?");
            } else {
                applied++;
            }
        } catch(e) {
            errors.push("Error on \\"" + layer.name + "\\": " + e.message);
        }
    }
    app.endUndoGroup();
    var msg = "Applied to " + applied + " layer(s).";
    if (errors.length > 0) msg += "\\nWarnings:\\n" + errors.join("\\n");
    return msg;
})();
`;
    cs.evalScript(script, function(result) {
        showStatus(result || 'Done.');
    });
}

// ── Remove IconFlow effects from selected layers ───────────────────────────────
function clearNativeEffect() {
    const script = `
(function clearIconFlowNative() {
    var comp = app.project.activeItem;
    if (!comp || !(comp instanceof CompItem)) return "ERROR: No active composition.";
    var layers = comp.selectedLayers;
    if (!layers || layers.length === 0) return "ERROR: No layers selected.";
    var removed = 0;
    app.beginUndoGroup("Remove IconFlow Native Picker");
    for (var i = 0; i < layers.length; i++) {
        var fx = layers[i].Effects;
        for (var j = fx.numProperties; j >= 1; j--) {
            var prop = fx.property(j);
            if (prop && prop.matchName === "ADBE IconFlow Native Picker") {
                prop.remove();
                removed++;
            }
        }
    }
    app.endUndoGroup();
    return "Removed " + removed + " effect instance(s).";
})();
`;
    cs.evalScript(script, function(result) {
        showStatus(result || 'Done.');
    });
}

// ── UI helpers ────────────────────────────────────────────────────────────────
function showStatus(msg) {
    const el = document.getElementById('status');
    if (el) el.textContent = msg;
}

// ── Wire up buttons on DOM ready ──────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', function() {
    const applyBtn = document.getElementById('btn-apply');
    const clearBtn = document.getElementById('btn-clear');

    if (applyBtn) applyBtn.addEventListener('click', applyNativeEffect);
    if (clearBtn) clearBtn.addEventListener('click', clearNativeEffect);

    showStatus('Select layers and press Apply.');
});
