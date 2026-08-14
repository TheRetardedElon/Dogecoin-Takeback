/** Dogecoin Dev Docs — shared sidebar + active link highlight. */
(function () {
  var path = (window.location.pathname || "").replace(/\\/g, "/");
  var inPages = /\/pages\//.test(path) || /\\pages\\/.test(window.location.href);
  // file:// paths vary; also check relative script location via document
  if (!inPages) {
    var scripts = document.getElementsByTagName("script");
    for (var i = 0; i < scripts.length; i++) {
      var src = scripts[i].getAttribute("src") || "";
      if (src.indexOf("../assets/nav.js") !== -1) inPages = true;
    }
  }

  var p = inPages ? "" : "pages/";
  var root = inPages ? "../" : "";

  function link(href, label) {
    return '<a href="' + href + '">' + label + "</a>";
  }

  var html = "";
  html += '<div class="nav-section">Start here</div>';
  html += link(root + "index.html", "Dashboard");
  html += link(p + "recovery-status.html", "Recovery status");
  html += link(p + "roadmap.html", "Roadmap");

  html += '<div class="nav-section">Understanding</div>';
  html += link(p + "how-it-works.html", "How it works");
  html += link(p + "install-roles.html", "Install roles");
  html += link(p + "testnet.html", "Testnet");
  html += link(p + "architecture.html", "Architecture");
  html += link(p + "module-map.html", "Module map");
  html += link(p + "diagrams.html", "Diagrams");
  html += link(p + "glossary.html", "Glossary");

  html += '<div class="nav-section">Core Pro product</div>';
  html += link(p + "ui-reference-core-pro.html", "Core Pro UI reference");
  html += link(p + "modern-ui.html", "Modern UI &amp; themes");
  html += link(p + "payment-layer.html", "Payment layer");
  html += link(p + "pure-doge-strategy.html", "Pure DOGE strategy");
  html += link(p + "memestream-integration.html", "MemeStream");
  html += link(p + "arcade.html", "Arcade");

  html += '<div class="nav-section">Node performance</div>';
  html += link(p + "ibd-and-p2p.html", "IBD &amp; P2P");
  html += link(p + "assumeutxo.html", "AssumeUTXO");
  html += link(p + "fast-sync.html", "Fast Sync explained");
  html += link(p + "gpenode.html", "GPENode operator");
  html += link(p + "multi-operator-mesh.html", "Multi-operator mesh");

  html += '<div class="nav-section">Storage &amp; security</div>';
  html += link(p + "storage-stack.html", "Storage stack");
  html += link(p + "fast-sync-threat-model.html", "Fast Sync threat model");
  html += link(p + "sqlite-wallet-plan.html", "SQLite wallet plan");
  html += link(p + "security-hardening.html", "Security hardening");
  html += link(p + "security-audit-core.html", "Core attack audit");
  html += link(p + "security-changelog.html", "Security changelog");

  html += '<div class="nav-section">Workstreams</div>';
  html += link(p + "rebrand-audit.html", "Rebrand audit");
  html += link(p + "build-and-run.html", "Build &amp; run");

  html += '<div class="nav-section">Repo</div>';
  html += link(p + "source-of-truth.html", "Source of truth");

  var nav = document.querySelector(".sidebar nav");
  if (nav) {
    nav.innerHTML = html;
  }

  var file = path.split("/").pop() || "index.html";
  if (!file || file === "docs" || file.indexOf(".") === -1) file = "index.html";

  document.querySelectorAll(".sidebar nav a[href]").forEach(function (a) {
    var href = a.getAttribute("href") || "";
    var target = href.split("/").pop();
    if (target === file) {
      a.classList.add("active");
    }
  });
})();
