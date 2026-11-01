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

(function () {
  var terms = document.querySelectorAll(".term");
  if (!terms.length) return;

  var tip = document.createElement("p");
  tip.className = "term-tip";
  tip.id = "term-tip";
  tip.setAttribute("role", "tooltip");
  tip.hidden = true;
  document.body.appendChild(tip);

  var current = null;

  function textOf(el) {
    return el.getAttribute("data-tip") || el.getAttribute("title") || "";
  }

  function place(el) {
    var margin = 8;
    var header = document.querySelector(".site-header");
    var minTop = header ? header.getBoundingClientRect().bottom + margin : margin;
    tip.hidden = false;
    var width = tip.offsetWidth;
    var height = tip.offsetHeight;
    var rect = el.getBoundingClientRect();
    var left = rect.left;
    if (left + width > window.innerWidth - margin) left = window.innerWidth - width - margin;
    if (left < margin) left = margin;
    var top = rect.bottom + margin;
    if (top + height > window.innerHeight - margin) top = rect.top - height - margin;
    if (top < minTop) top = Math.min(rect.bottom + margin, window.innerHeight - height - margin);
    tip.style.left = left + "px";
    tip.style.top = top + "px";
  }

  function show(el) {
    var text = textOf(el);
    if (!text) return;
    if (!el.getAttribute("data-tip")) el.setAttribute("data-tip", text);
    el.removeAttribute("title");
    if (current && current !== el) current.removeAttribute("aria-describedby");
    current = el;
    tip.textContent = text;
    el.setAttribute("aria-describedby", "term-tip");
    place(el);
  }

  function hide(el) {
    if (el && el.getAttribute("data-tip")) el.setAttribute("title", el.getAttribute("data-tip"));
    if (current === el) {
      el.removeAttribute("aria-describedby");
      tip.hidden = true;
      current = null;
    }
  }

  for (var i = 0; i < terms.length; i++) {
    (function (el) {
      el.addEventListener("pointerenter", function (event) {
        if (event.pointerType === "touch") return;
        show(el);
      });
      el.addEventListener("pointerleave", function (event) {
        if (event.pointerType === "touch") return;
        hide(el);
      });
      el.addEventListener("focus", function () { show(el); });
      el.addEventListener("blur", function () { hide(el); });
      el.addEventListener("click", function () {
        if (window.matchMedia("(hover: hover)").matches) return;
        if (current === el) hide(el);
        else show(el);
      });
    })(terms[i]);
  }

  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape" && current) hide(current);
  });

  window.addEventListener("scroll", function () {
    if (current) place(current);
  }, true);
  window.addEventListener("resize", function () {
    if (current) place(current);
  });
})();
