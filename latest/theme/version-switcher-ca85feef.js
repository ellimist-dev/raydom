// Adds a version menu to the top bar when the book is served from the versioned site, which has
// versions.json one level above each version's root. Hidden for local builds.
(function () {
  if (typeof path_to_root === "undefined") return;
  var versionRoot = new URL(path_to_root, location.href);
  var siteRoot = new URL("../", versionRoot);
  var current = versionRoot.pathname.slice(siteRoot.pathname.length).replace(/\/$/, "");
  var page = location.pathname.slice(versionRoot.pathname.length);

  fetch(new URL("versions.json", siteRoot))
    .then(function (r) { return r.ok ? r.json() : Promise.reject(); })
    .then(function (data) {
      var bar = document.querySelector("#mdbook-menu-bar .right-buttons");
      if (!bar) return;
      var select = document.createElement("select");
      select.className = "version-switcher";
      select.setAttribute("aria-label", "Documentation version");
      var names = ["latest", "dev"].concat(data.versions);
      names.forEach(function (name) {
        var o = document.createElement("option");
        o.value = name;
        o.textContent = name === "latest" && data.latest ? "latest (" + data.latest + ")" : name;
        if (name === current) o.selected = true;
        select.appendChild(o);
      });
      if (names.indexOf(current) < 0) {
        var o = document.createElement("option");
        o.textContent = current;
        o.selected = true;
        select.insertBefore(o, select.firstChild);
      }
      select.addEventListener("change", function () {
        var base = new URL(select.value + "/", siteRoot);
        var target = new URL(page, base);
        fetch(target, { method: "HEAD" })
          .then(function (r) { location.href = r.ok ? target.href : base.href; })
          .catch(function () { location.href = base.href; });
      });
      bar.insertBefore(select, bar.firstChild);

      if (data.latest && current !== "latest" && current !== data.latest && current !== "dev") {
        var note = document.createElement("div");
        note.className = "version-note";
        note.innerHTML = "This is the documentation for " + current + ". The latest release is " +
          "<a href=\"" + new URL("latest/" + page, siteRoot).href + "\">" + data.latest + "</a>.";
        var main = document.querySelector("main");
        if (main) main.insertBefore(note, main.firstChild);
      }
    })
    .catch(function () {});
})();
