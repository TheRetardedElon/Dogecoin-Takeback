/** Highlight active sidebar link based on current page filename. */
(function () {
  var path = window.location.pathname.replace(/\\/g, "/");
  var file = path.split("/").pop() || "index.html";
  if (!file || file === "docs") file = "index.html";

  document.querySelectorAll(".sidebar nav a[href]").forEach(function (a) {
    var href = a.getAttribute("href") || "";
    var target = href.split("/").pop();
    if (target === file) {
      a.classList.add("active");
    }
  });
})();
