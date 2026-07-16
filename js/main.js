(function () {
  "use strict";

  var cs = new CSInterface();
  var mappings = document.getElementById("colorMappings");
  var statusBox = document.getElementById("status");
  var applyButton = document.getElementById("apply");
  var clearButton = document.getElementById("clear");
  var pickButton = document.getElementById("pickBackground");
  var maxMappings = 12;
  var autoTimer = null;
  var operationInFlight = false;
  var pendingLiveUpdate = false;
  var presets = {
    electricBlue: { color: "#38A6FF", intensity: 1.2, radius: 55, exposure: 0.8 },
    neonGreen: { color: "#48FF8A", intensity: 1.4, radius: 60, exposure: 0.7 },
    magenta: { color: "#FF4FCB", intensity: 1.25, radius: 50, exposure: 0.6 },
    gold: { color: "#FFD15A", intensity: 1.0, radius: 38, exposure: 0.4 },
    ice: { color: "#A5F5FF", intensity: 1.35, radius: 65, exposure: 0.9 }
  };

  function q(selector) { return document.querySelector(selector); }
  function qa(selector) { return Array.prototype.slice.call(document.querySelectorAll(selector)); }

  function validHex(value) {
    return /^#[0-9a-f]{6}$/i.test(String(value || "").trim());
  }

  function normalizeHex(value) {
    value = String(value || "").trim();
    if (value.charAt(0) !== "#") value = "#" + value;
    return value.toUpperCase();
  }

  function setStatus(message, state) {
    statusBox.textContent = message;
    statusBox.className = "status" + (state ? " " + state : "");
  }

  function escapeForExtendScript(value) {
    return '"' + value.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\r/g, "\\r").replace(/\n/g, "\\n") + '"';
  }

  function setBusy(isBusy) {
    operationInFlight = isBusy;
    applyButton.disabled = isBusy;
    clearButton.disabled = isBusy;
    pickButton.disabled = isBusy;
    qa(".sample-source").forEach(function (button) { button.disabled = isBusy; });
  }

  function setCustomMode(isCustom) {
    q(".custom-background").hidden = !isCustom;
  }

  function requestLiveUpdate() {
    if (!q("#autoApply").checked) return;
    if (autoTimer) window.clearTimeout(autoTimer);
    autoTimer = window.setTimeout(function () {
      autoTimer = null;
      if (operationInFlight) {
        pendingLiveUpdate = true;
        return;
      }
      invoke("iconFlowProcess", getConfig(), true);
    }, 260);
  }

  function addMapping(from, to) {
    if (mappings.children.length >= maxMappings) {
      setStatus("الحد الأقصى هو " + maxMappings + " ألوان في تمرير واحد.", "error");
      return false;
    }
    var row = document.createElement("div");
    row.className = "mapping";
    row.innerHTML =
      '<div class="color-input"><input class="from-picker" type="color" value="' + from + '"><input class="hex from-hex" type="text" maxlength="7" value="' + from + '" spellcheck="false"><button class="sample-source" type="button" title="التقاط لون يدويًا من شاشة الـComposition" aria-label="التقاط لون يدويًا من شاشة الـComposition"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M14.8 3.2 20.8 9.2 17.7 12.3 15.7 10.3 8.2 17.8 5.1 17.8 5.1 14.7 12.6 7.2 10.6 5.2zM4 20h10v2H4z"/></svg></button></div>' +
      '<div class="color-input"><input class="to-picker" type="color" value="' + to + '"><input class="hex to-hex" type="text" maxlength="7" value="' + to + '" spellcheck="false"></div>' +
      '<button class="remove" type="button" title="حذف اللون" aria-label="حذف اللون">×</button>';

    function connectInside(pickerSelector, hexSelector) {
      var picker = row.querySelector(pickerSelector);
      var hex = row.querySelector(hexSelector);
      function syncPicker() {
        hex.value = picker.value.toUpperCase();
        requestLiveUpdate();
      }
      picker.addEventListener("input", syncPicker);
      picker.addEventListener("change", syncPicker);
      function syncHex() {
        var normalized = normalizeHex(hex.value);
        if (validHex(normalized)) {
          hex.value = normalized;
          picker.value = normalized;
          hex.classList.remove("invalid");
          requestLiveUpdate();
        } else {
          hex.classList.add("invalid");
        }
      }
      hex.addEventListener("change", syncHex);
      hex.addEventListener("blur", syncHex);
    }

    connectInside(".from-picker", ".from-hex");
    connectInside(".to-picker", ".to-hex");
    row.querySelector(".remove").addEventListener("click", function () {
      row.remove();
      requestLiveUpdate();
    });
    row.querySelector(".sample-source").addEventListener("click", function () {
      openManualColorPicker(row.querySelector(".from-picker"));
    });
    mappings.appendChild(row);
    return true;
  }

  function getConfig() {
    var background = q('input[name="background"]:checked').value;
    var colorMaps = [];
    qa(".mapping").forEach(function (row) {
      var from = normalizeHex(row.querySelector(".from-hex").value);
      var to = normalizeHex(row.querySelector(".to-hex").value);
      if (validHex(from) && validHex(to)) colorMaps.push({ from: from, to: to });
    });

    return {
      background: background,
      customBackground: normalizeHex(q("#customBgHex").value),
      matteTolerance: Number(q("#matteTolerance").value),
      matteSoftness: Number(q("#matteSoftness").value),
      edgeChoke: Number(q("#edgeChoke").value),
      edgeCleaner: q("#edgeCleaner").checked,
      colorMaps: colorMaps,
      colorTolerance: Number(q("#colorTolerance").value),
      tintAll: {
        enabled: q("#tintAll").checked,
        color: normalizeHex(q("#tintAllHex").value)
      },
      useGlow: q("#useGlow").checked && q("#glowEngine").value !== "none",
      glowEngine: q("#glowEngine").value,
      glowStrength: Number(q("#glowStrength").value),
      glowRadius: Number(q("#glowRadius").value),
      deepExposure: Number(q("#deepExposure").value),
      makeBackup: q("#makeBackup").checked,
      despill: q("#despill").checked
    };
  }

  function handleHostResponse(response, isLive) {
    if (!response || response.indexOf("ERR|") === 0) {
      var reason = response ? response.split("|").slice(1).join("|") : "UNKNOWN";
      var messages = {
        NO_ACTIVE_COMP: "افتح Composition وحدّد طبقة واحدة على الأقل.",
        NO_SELECTED_LAYER: "حدّد طبقة فيديو أو Pre-comp واحدة على الأقل.",
        CEP_RUNTIME_NOT_FOUND: "هذه اللوحة يجب تشغيلها من داخل After Effects.",
        NO_EFFECTS_GROUP: "الطبقة المحددة لا تقبل التأثيرات.",
        LINEAR_KEY_UNAVAILABLE: "لم يُعثر على تأثير Linear Color Key في After Effects."
      };
      setStatus(messages[reason] || "تعذّر تنفيذ العملية: " + reason, "error");
      return;
    }

    var parts = response.split("|");
    if (parts[0] === "OK") {
      var processed = parts[1] || "0";
      var deep = parts[2] || "0";
      var fallback = parts[3] || "0";
      var message = (isLive ? "تم التحديث الحي لـ " : "تمت معالجة ") + processed + " طبقة.";
      if (Number(deep) > 0) message += " تم تطبيق Deep Glow على " + deep + ".";
      if (Number(fallback) > 0) message += " استُخدم Glow المدمج على " + fallback + ".";
      setStatus(message, "success");
      refreshGlowStatus();
      return;
    }
    if (parts[0] === "CLEARED") {
      setStatus("أُزيلت تأثيرات IconFlow من " + (parts[1] || "0") + " طبقة.", "success");
      return;
    }
    setStatus("استجابة غير متوقعة من After Effects.", "error");
  }

  function invoke(functionName, config, isLive, responseHandler) {
    if (operationInFlight) {
      if (isLive) pendingLiveUpdate = true;
      return;
    }
    setBusy(true);
    setStatus(isLive ? "جارٍ التحديث الحي…" : "جارٍ تنفيذ التأثيرات…");
    var argument = config ? escapeForExtendScript(JSON.stringify(config)) : "";
    cs.evalScript(functionName + "(" + argument + ")", function (response) {
      setBusy(false);
      if (responseHandler) responseHandler(response);
      else handleHostResponse(response, isLive);
      if (pendingLiveUpdate) {
        pendingLiveUpdate = false;
        requestLiveUpdate();
      }
    });
  }

  function updateDeepGlowStatus(response) {
    var tag = q("#deepGlowState");
    if (response === "FX|PENDING") {
      tag.textContent = "حدّد طبقة لفحص التوهج";
      tag.className = "tag neutral";
      return;
    }
    var parts = String(response || "").split("|");
    var glowAvailable = parts[0] === "FX" && parts[1] === "1";
    var deepAvailable = parts[0] === "FX" && parts[3] === "1";
    if (glowAvailable && deepAvailable) {
      tag.textContent = "Glow وDeep Glow متصلان";
      tag.className = "tag";
    } else if (glowAvailable) {
      tag.textContent = "Glow متاح · Deep Glow غير موجود";
      tag.className = "tag missing";
    } else {
      tag.textContent = "Glow غير متاح على الطبقة";
      tag.className = "tag missing";
    }
  }

  function refreshGlowStatus() {
    cs.evalScript("iconFlowGlowStatus()", updateDeepGlowStatus);
  }

  function openManualColorPicker(picker) {
    function notifyPickerChanged() {
      var event;
      try {
        if (typeof window.Event === "function") event = new window.Event("input", { bubbles: true });
      } catch (ignoreEventConstructor) {}
      if (!event) {
        event = document.createEvent("HTMLEvents");
        event.initEvent("input", true, false);
      }
      picker.dispatchEvent(event);
    }

    function openWindowsPicker() {
      setStatus("اضغط قطّارة Windows ثم انقر على اللون داخل شاشة الـComposition.");
      picker.focus();
      picker.click();
    }

    /* Newer CEP runtimes expose a direct screen eyedropper. */
    if (typeof window.EyeDropper === "function") {
      try {
        setStatus("انقر الآن على اللون المطلوب داخل شاشة الـComposition.");
        (new window.EyeDropper()).open().then(function (result) {
          var color = normalizeHex(result && result.sRGBHex);
          if (!validHex(color)) {
            setStatus("لم تُرجع القطّارة لونًا صالحًا.", "error");
            return;
          }
          picker.value = color;
          notifyPickerChanged();
          setStatus("تم اختيار اللون " + color + " من شاشة الـComposition.", "success");
        }, function (error) {
          if (error && error.name === "AbortError") {
            setStatus("تم إلغاء اختيار اللون.");
          } else {
            openWindowsPicker();
          }
        });
        return;
      } catch (ignoreEyeDropper) {}
    }
    openWindowsPicker();
  }

  function bindColorPair(pickerSelector, hexSelector) {
    var picker = q(pickerSelector);
    var hex = q(hexSelector);
    function syncPicker() {
      hex.value = picker.value.toUpperCase();
      requestLiveUpdate();
    }
    picker.addEventListener("input", syncPicker);
    picker.addEventListener("change", syncPicker);
    function syncHex() {
      var value = normalizeHex(hex.value);
      if (validHex(value)) {
        hex.value = value;
        picker.value = value;
        hex.classList.remove("invalid");
        requestLiveUpdate();
      } else {
        hex.classList.add("invalid");
      }
    }
    hex.addEventListener("change", syncHex);
    hex.addEventListener("blur", syncHex);
  }

  q("#addColor").addEventListener("click", function () {
    if (addMapping("#FFFFFF", "#7D83FF")) requestLiveUpdate();
  });
  q("#apply").addEventListener("click", function () { invoke("iconFlowProcess", getConfig(), false); });
  q("#clear").addEventListener("click", function () {
    if (autoTimer) window.clearTimeout(autoTimer);
    pendingLiveUpdate = false;
    invoke("iconFlowClear", null, false);
  });
  q("#pickBackground").addEventListener("click", function () {
    q('input[name="background"][value="custom"]').checked = true;
    setCustomMode(true);
    openManualColorPicker(q("#customBg"));
  });

  qa('input[name="background"]').forEach(function (radio) {
    radio.addEventListener("change", function () {
      setCustomMode(q('input[name="background"]:checked').value === "custom");
      requestLiveUpdate();
    });
  });

  bindColorPair("#customBg", "#customBgHex");
  bindColorPair("#tintAllPicker", "#tintAllHex");

  [
    ["#matteTolerance", "#toleranceValue", function (value) { return value; }],
    ["#matteSoftness", "#softnessValue", function (value) { return value; }],
    ["#edgeChoke", "#edgeChokeValue", function (value) { return Number(value).toFixed(1) + " px"; }],
    ["#colorTolerance", "#colorToleranceValue", function (value) { return value; }],
    ["#glowStrength", "#glowValue", function (value) { return Number(value).toFixed(1); }],
    ["#glowRadius", "#glowRadiusValue", function (value) { return value + " px"; }],
    ["#deepExposure", "#deepExposureValue", function (value) { return Number(value).toFixed(1); }]
  ].forEach(function (binding) {
    q(binding[0]).addEventListener("input", function () {
      q(binding[1]).textContent = binding[2](this.value);
      requestLiveUpdate();
    });
  });

  ["#edgeCleaner", "#tintAll", "#useGlow", "#glowEngine", "#makeBackup", "#despill"].forEach(function (selector) {
    q(selector).addEventListener("change", requestLiveUpdate);
  });
  q("#autoApply").addEventListener("change", function () {
    if (this.checked) {
      setStatus("التحديث الحي مفعّل؛ ستُطبّق التغييرات على الطبقة المحددة.");
      requestLiveUpdate();
    } else {
      if (autoTimer) window.clearTimeout(autoTimer);
      setStatus("التحديث الحي متوقف؛ استخدم «تطبيق الآن» عند الجاهزية.");
    }
  });
  q("#palettePreset").addEventListener("change", function () {
    var preset = presets[this.value];
    if (!preset) return;
    q("#tintAll").checked = true;
    q("#tintAllPicker").value = preset.color;
    q("#tintAllHex").value = preset.color;
    q("#glowStrength").value = preset.intensity;
    q("#glowValue").textContent = preset.intensity.toFixed(1);
    q("#glowRadius").value = preset.radius;
    q("#glowRadiusValue").textContent = preset.radius + " px";
    q("#deepExposure").value = preset.exposure;
    q("#deepExposureValue").textContent = preset.exposure.toFixed(1);
    requestLiveUpdate();
  });

  addMapping("#FFFFFF", "#7D83FF");
  setCustomMode(false);
  refreshGlowStatus();
}());
