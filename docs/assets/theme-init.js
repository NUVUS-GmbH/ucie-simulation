// Apply the NUVUS preference before styles paint. System is the default.
(function () {
  var choice = "system";
  try { choice = localStorage.getItem("nuvus-theme") || "system"; } catch (_) {}
  if (!["light", "dark", "system"].includes(choice)) choice = "system";
  var dark = choice === "dark" || (choice === "system" && matchMedia("(prefers-color-scheme: dark)").matches);
  document.documentElement.setAttribute("data-theme-choice", choice);
  if (dark) document.documentElement.setAttribute("data-theme", "dark");
})();
