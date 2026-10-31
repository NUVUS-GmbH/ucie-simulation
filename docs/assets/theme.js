(function () {
  var key = "nuvus-theme";
  var root = document.documentElement;

  function preferredDark() {
    return window.matchMedia("(prefers-color-scheme: dark)").matches;
  }

  function apply(choice) {
    var dark = choice === "dark" || (choice === "system" && preferredDark());
    if (dark) root.setAttribute("data-theme", "dark");
    else root.removeAttribute("data-theme");
    root.setAttribute("data-theme-choice", choice);
    var buttons = document.querySelectorAll("[data-theme-set]");
    for (var i = 0; i < buttons.length; i++) {
      var on = buttons[i].getAttribute("data-theme-set") === choice;
      buttons[i].setAttribute("aria-pressed", on ? "true" : "false");
    }
  }

  var buttons = document.querySelectorAll("[data-theme-set]");
  for (var i = 0; i < buttons.length; i++) {
    buttons[i].addEventListener("click", function (event) {
      var choice = event.currentTarget.getAttribute("data-theme-set");
      try {
        localStorage.setItem(key, choice);
      } catch (error) {
        /* Private mode can reject storage. The choice still applies. */
      }
      apply(choice);
    });
  }

  window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", function () {
    if ((root.getAttribute("data-theme-choice") || "system") === "system") apply("system");
  });

  apply(root.getAttribute("data-theme-choice") || "system");
})();
