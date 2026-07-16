/* Minimal CEP bridge used by this panel. No Node.js APIs are required. */
(function (global) {
  "use strict";

  function CSInterface() {}

  CSInterface.prototype.evalScript = function (script, callback) {
    if (global.__adobe_cep__ && global.__adobe_cep__.evalScript) {
      global.__adobe_cep__.evalScript(script, callback || function () {});
      return;
    }
    window.setTimeout(function () {
      if (callback) callback("ERR|CEP_RUNTIME_NOT_FOUND");
    }, 0);
  };

  global.CSInterface = CSInterface;
}(this));
