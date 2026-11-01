// Select before first paint. An explicit choice wins over the browser language.
(function () {
  var root = document.documentElement;
  var current = root.lang === "de" ? "de" : "en";
  var script = document.currentScript;
  var base = new URL("../", script.src);
  var page = new URL(location.href);
  var explicit = page.searchParams.get("lang");
  var valid = function (value) { return value === "en" || value === "de"; };
  var saved = null;
  try { saved = localStorage.getItem("nuvus-lang"); } catch (_) {}
  var primary = (navigator.languages && navigator.languages[0]) || navigator.language || "en";
  var desired = valid(explicit) ? explicit : valid(saved) ? saved : /^de(?:-|$)/i.test(primary) ? "de" : "en";
  if (valid(explicit)) {
    try { localStorage.setItem("nuvus-lang", explicit); } catch (_) {}
  }
  if (desired !== current) {
    var target = new URL(desired === "de" ? "de/index.html" : "index.html", base);
    target.search = page.search;
    target.hash = page.hash;
    location.replace(target.href);
    return;
  }
  // Keep the explicit query as a fallback when browser storage is unavailable.
  function updateLinks() {
    document.querySelectorAll("[data-lang-link]").forEach(function (link) {
      var language = link.getAttribute("data-lang-link");
      var target = new URL(language === "de" ? "de/index.html" : "index.html", base);
      target.search = location.search;
      target.searchParams.set("lang", language);
      target.hash = location.hash;
      link.href = target.href;
    });
  }
  document.addEventListener("DOMContentLoaded", updateLinks);
  window.addEventListener("hashchange", updateLinks);
})();
