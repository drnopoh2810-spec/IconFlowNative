/*
  IconFlow Recolor host script for Adobe After Effects.
  All effects are identified with the IF__ prefix so the panel can remove only
  the effects it created. The script intentionally uses internal match names
  for Adobe effects, so it is not tied to the interface language.
*/
#target aftereffects

(function () {
    var PREFIX = "IF__";
    var WORK_PREFIX = "IF | ";

    function parseConfig(raw) {
        /* The JSON is produced by the local CEP panel only. */
        return eval("(" + raw + ")");
    }

    function hexToRgb(value) {
        var hex = String(value || "#000000").replace("#", "");
        if (!/^[0-9a-fA-F]{6}$/.test(hex)) hex = "000000";
        return [
            parseInt(hex.substr(0, 2), 16) / 255,
            parseInt(hex.substr(2, 2), 16) / 255,
            parseInt(hex.substr(4, 2), 16) / 255
        ];
    }

    function safeSet(effect, index, value) {
        try {
            var property = effect.property(index);
            if (property) {
                property.setValue(value);
                return true;
            }
        } catch (ignore) {}
        return false;
    }

    function getEffects(layer) {
        return layer.property("ADBE Effect Parade");
    }

    function addEffect(layer, matchName, effectName) {
        var effects = getEffects(layer);
        if (!effects) throw "NO_EFFECTS_GROUP";
        var effect = effects.addProperty(matchName);
        if (effect && effectName) effect.name = effectName;
        return effect;
    }

    function clearIconFlowEffects(layer) {
        var effects = getEffects(layer);
        var removed = 0;
        if (!effects) return removed;
        for (var i = effects.numProperties; i >= 1; i--) {
            try {
                var effect = effects.property(i);
                if (effect && String(effect.name).indexOf(PREFIX) === 0) {
                    effect.remove();
                    removed++;
                }
            } catch (ignore) {}
        }
        return removed;
    }

    function isWorkingLayer(layer) {
        return String(layer.name).indexOf(WORK_PREFIX) === 0;
    }

    function makeWorkingLayer(source, config) {
        if (!config.makeBackup || isWorkingLayer(source)) return source;

        var duplicate = source.duplicate();
        duplicate.name = WORK_PREFIX + source.name;

        /* Keep an audio track only on the hidden source, avoiding doubled sound. */
        try { duplicate.audioEnabled = false; } catch (ignoreAudio) {}
        source.enabled = false;
        /* Keep the processed copy selected so a live update reuses it instead of duplicating again. */
        try { source.selected = false; } catch (ignoreSourceSelection) {}
        try { duplicate.selected = true; } catch (ignoreDuplicateSelection) {}
        return duplicate;
    }

    function matteColor(config) {
        if (config.background === "green") return "#00FF00";
        if (config.background === "white") return "#FFFFFF";
        if (config.background === "custom") return config.customBackground;
        return "#000000";
    }

    function setFirstColorProperty(effect, value) {
        var i, property;
        for (i = 1; i <= effect.numProperties; i++) {
            try {
                property = effect.property(i);
                if (property.propertyValueType === PropertyValueType.COLOR) {
                    property.setValue(value);
                    return true;
                }
            } catch (ignore) {}
        }
        return false;
    }

    function configureLinearColorKey(key, config) {
        var color = hexToRgb(matteColor(config));
        var tolerance = Math.max(0, Math.min(100, Number(config.matteTolerance)));
        var softness = Math.max(0, Math.min(100, Number(config.matteSoftness)));

        /* Names are preferred when AE exposes English property names; index fallbacks keep localized AE usable. */
        if (!setEffectPropertyByKeywords(key, ["key color", "key colour"], color)) {
            if (!setFirstColorProperty(key, color)) safeSet(key, 1, color);
        }
        if (!setEffectPropertyByKeywords(key, ["matching tolerance", "tolerance"], tolerance)) {
            safeSet(key, 2, tolerance);
        }
        if (!setEffectPropertyByKeywords(key, ["matching softness", "softness"], softness)) {
            safeSet(key, 3, softness);
        }
    }

    function addMatte(layer, config) {
        if (config.background === "none") return;

        var key = addEffect(layer, "ADBE Linear Color Key2", PREFIX + "MATTE | " + config.background);
        if (!key) throw "LINEAR_KEY_UNAVAILABLE";

        configureLinearColorKey(key, config);

        if (config.edgeCleaner) {
            try { addEffect(layer, "ADBE KeyCleaner", PREFIX + "EDGE | Key Cleaner"); } catch (ignoreKeyCleaner) {}
        }
        if (Number(config.edgeChoke) !== 0) {
            var choker = addEffect(layer, "ADBE Simple Choker", PREFIX + "EDGE | Simple Choker");
            /* Simple Choker's first property is Choke Matte in pixels. */
            safeSet(choker, 1, Number(config.edgeChoke));
        }

        if (config.background === "green" && config.despill) {
            /* Advanced Spill Suppressor uses useful defaults and follows the keyer. */
            addEffect(layer, "ADBE Spill2", PREFIX + "DESPILL | green");
        }
    }

    function addColorMaps(layer, config) {
        if (!config.colorMaps || !config.colorMaps.length) return;
        for (var i = 0; i < config.colorMaps.length; i++) {
            var map = config.colorMaps[i];
            var change = addEffect(layer, "ADBE Change To Color", PREFIX + "COLOR | " + map.from + " > " + map.to);
            if (!change) continue;

            /* Change to Color: From, To, Change, Change By, Tolerance, Softness. */
            safeSet(change, 1, hexToRgb(map.from));
            safeSet(change, 2, hexToRgb(map.to));
            safeSet(change, 5, Number(config.colorTolerance));
            safeSet(change, 6, 0);
        }
    }

    function addUnifiedTint(layer, config) {
        if (!config.tintAll || !config.tintAll.enabled) return;
        var tint = addEffect(layer, "ADBE Tint", PREFIX + "TINT | " + config.tintAll.color);
        if (!tint) return;

        /* Tint: Map Black To, Map White To, Amount To Tint. */
        safeSet(tint, 1, [0, 0, 0]);
        safeSet(tint, 2, hexToRgb(config.tintAll.color));
        safeSet(tint, 3, 100);
    }

    function deepGlowMatchName() {
        var i, item, display, match, haystack;
        try {
            if (app.effects && app.effects.length) {
                for (i = 0; i < app.effects.length; i++) {
                    item = app.effects[i];
                    display = String(item.displayName || "");
                    match = String(item.matchName || "");
                    haystack = (display + " " + match).toLowerCase();
                    if (haystack.indexOf("deep glow") !== -1 || haystack.indexOf("deepglow") !== -1) return match || display;
                }
            }
        } catch (ignore) {}

        /* Covers common display names if an older AE does not expose app.effects. */
        var candidates = ["Deep Glow", "Deep Glow 2", "Deep Glow v2"];
        for (i = 0; i < candidates.length; i++) {
            try {
                var testComp = app.project.activeItem;
                if (!testComp || !(testComp instanceof CompItem) || !testComp.selectedLayers.length) break;
                var testEffects = getEffects(testComp.selectedLayers[0]);
                var temporary = testEffects.addProperty(candidates[i]);
                if (temporary) {
                    temporary.remove();
                    return candidates[i];
                }
            } catch (ignoredCandidate) {}
        }
        return null;
    }

    function setEffectPropertyByKeywords(effect, keywords, value) {
        var i, property, name;
        for (i = 1; i <= effect.numProperties; i++) {
            try {
                property = effect.property(i);
                name = String(property.name || "").toLowerCase();
                for (var j = 0; j < keywords.length; j++) {
                    if (name.indexOf(keywords[j]) !== -1) {
                        property.setValue(value);
                        return true;
                    }
                }
            } catch (ignore) {}
        }
        return false;
    }

    function configureDeepGlow(effect, config) {
        /* Deep Glow property indices differ by plug-in version; semantic lookup avoids hard-coding them. */
        setEffectPropertyByKeywords(effect, ["radius"], Number(config.glowRadius));
        setEffectPropertyByKeywords(effect, ["exposure"], Number(config.deepExposure));
        setEffectPropertyByKeywords(effect, ["intensity", "strength", "gain"], Number(config.glowStrength));
    }

    function addGlow(layer, config) {
        if (!config.useGlow || config.glowEngine === "none") return "none";

        if (config.glowEngine === "deep") {
            var deepGlow = deepGlowMatchName();
            if (deepGlow) {
                try {
                    var deepEffect = addEffect(layer, deepGlow, PREFIX + "GLOW | Deep Glow");
                    configureDeepGlow(deepEffect, config);
                    return "deep";
                } catch (ignoreDeepGlow) {}
            }
        }

        try {
            var glow = addEffect(layer, "ADBE Glo2", PREFIX + "GLOW | Built-in Glow");
            if (!glow) return "none";
            /* Glow: threshold, radius, intensity. Defaults remain available in Effect Controls. */
            safeSet(glow, 2, 0);
            safeSet(glow, 3, Number(config.glowRadius));
            safeSet(glow, 4, Number(config.glowStrength));
            return "builtin";
        } catch (ignoreBuiltinGlow) {
            return "none";
        }
    }

    function selectedLayersOrError() {
        if (!app.project || !(app.project.activeItem instanceof CompItem)) throw "NO_ACTIVE_COMP";
        var selected = app.project.activeItem.selectedLayers;
        if (!selected || selected.length < 1) throw "NO_SELECTED_LAYER";
        var layers = [];
        for (var i = 0; i < selected.length; i++) layers.push(selected[i]);
        return layers;
    }

    function selectedLayerForEffectCheck() {
        try {
            if (!app.project || !(app.project.activeItem instanceof CompItem)) return null;
            var selected = app.project.activeItem.selectedLayers;
            if (!selected || !selected.length) return null;
            return selected[0];
        } catch (ignore) {
            return null;
        }
    }

    function canAddEffectToLayer(layer, matchName) {
        var effects, temporary;
        try {
            effects = getEffects(layer);
            if (!effects) return false;
            temporary = effects.addProperty(matchName);
            if (!temporary) return false;
            temporary.remove();
            return true;
        } catch (ignore) {
            try { if (temporary) temporary.remove(); } catch (ignoreRemove) {}
            return false;
        }
    }

    function glowStatus() {
        var layer = selectedLayerForEffectCheck();
        if (!layer) return "FX|PENDING";
        var builtin = canAddEffectToLayer(layer, "ADBE Glo2");
        var deep = deepGlowMatchName() ? true : false;
        return "FX|" + (builtin ? "1" : "0") + "|DEEP|" + (deep ? "1" : "0");
    }

    function process(configJson) {
        var config, layers, processed = 0, deepCount = 0, builtinCount = 0;
        try {
            config = parseConfig(configJson);
            layers = selectedLayersOrError();
        } catch (e) {
            return "ERR|" + String(e);
        }

        app.beginUndoGroup("IconFlow Recolor");
        try {
            for (var i = 0; i < layers.length; i++) {
                var target = makeWorkingLayer(layers[i], config);
                clearIconFlowEffects(target);
                addMatte(target, config);
                addColorMaps(target, config);
                addUnifiedTint(target, config);
                var glow = addGlow(target, config);
                if (glow === "deep") deepCount++;
                if (glow === "builtin") builtinCount++;
                processed++;
            }
        } catch (error) {
            app.endUndoGroup();
            return "ERR|" + String(error);
        }
        app.endUndoGroup();
        return "OK|" + processed + "|" + deepCount + "|" + builtinCount;
    }

    function clear() {
        var layers;
        try {
            layers = selectedLayersOrError();
        } catch (e) {
            return "ERR|" + String(e);
        }
        app.beginUndoGroup("Clear IconFlow Effects");
        var count = 0;
        for (var i = 0; i < layers.length; i++) {
            if (clearIconFlowEffects(layers[i]) > 0) count++;
        }
        app.endUndoGroup();
        return "CLEARED|" + count;
    }

    /* Exposed globally for the CEP panel. */
    $.global.iconFlowProcess = process;
    $.global.iconFlowClear = clear;
    $.global.iconFlowDeepGlowStatus = function () { return deepGlowMatchName() ? "DEEP|1" : "DEEP|0"; };
    $.global.iconFlowGlowStatus = glowStatus;
}());
