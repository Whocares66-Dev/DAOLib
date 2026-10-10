// DAOLib's documentation site (DAOLib #45): the version selector in the title
// bar. The current docs are at the site's root, each release's under its
// version number (0.1.0/); versions.json at the root lists them, and a
// change of version keeps the reader on the same page where that version has
// it, else takes them to its front page.
(function () {
    "use strict";

    // The sidebars, each hidden or shown by its button in the title bar, the
    // choice kept across pages; set before the page is drawn, so a hidden
    // one never flashes.
    var sidebars = { nav: "dao-hide-nav", outline: "dao-hide-outline" };
    // The sidebars' widths are fixed (no drag bars), so a width Doxygen saved
    // is dropped before its tree script reads it, and the default holds.
    try {
        window.localStorage.removeItem("doxygen_width");
        window.localStorage.removeItem("doxygen_pagenav");
    } catch (e) {
        // No storage: Doxygen keeps its default too.
    }
    Object.keys(sidebars).forEach(function (key) {
        try {
            if (window.localStorage.getItem(sidebars[key]) === "1") {
                document.documentElement.classList.add(sidebars[key]);
            }
        } catch (e) {
            // No storage (a page opened from disk in some browsers): shown.
        }
    });

    // A sidebar's button, at the top of the sidebar itself.
    function addToggle(sidebar, id) {
        if (!sidebar) {
            return null;
        }
        var button = document.createElement("button");
        button.id = id;
        button.className = "dao-toggle";
        button.type = "button";
        button.appendChild(document.createElement("span"));
        sidebar.insertBefore(button, sidebar.firstChild);
        return button;
    }

    function wireSidebar(key, button) {
        if (!button) {
            return;
        }
        var label = key === "nav" ? "contents" : "outline";
        var update = function () {
            var hidden = document.documentElement.classList.contains(sidebars[key]);
            button.title = (hidden ? "Show the " : "Hide the ") + label;
            button.setAttribute("aria-pressed", hidden ? "true" : "false");
        };
        button.addEventListener("click", function () {
            var hidden = document.documentElement.classList.toggle(sidebars[key]);
            try {
                window.localStorage.setItem(sidebars[key], hidden ? "1" : "0");
            } catch (e) {
                // Not kept; shown again on the next page.
            }
            update();
        });
        update();
    }

    // The front page hides its own title bar (dao.css); marked before the
    // page is drawn, so the bar never shows.
    if (/\/(index\.html)?$/.test(window.location.pathname)) {
        document.documentElement.classList.add("dao-front-page");
    }

    // Where this page is: the site's root, the release folder it is in (or
    // none for the current docs), and the page's own file name.
    function place() {
        var path = window.location.pathname;
        var release = path.match(/^(.*\/)(\d+\.\d+\.\d+)\/([^/]*)$/);
        if (release) {
            return { root: release[1], version: release[2] + "/", page: release[3] || "index.html" };
        }
        var dir = path.substring(0, path.lastIndexOf("/") + 1);
        return { root: dir, version: "", page: path.substring(dir.length) || "index.html" };
    }

    // The same page in the chosen version, at the same member, if it has it.
    function go(url, fallback, hash) {
        fetch(url, { method: "HEAD" })
            .then(function (response) { window.location.href = response.ok ? url + hash : fallback; })
            .catch(function () { window.location.href = fallback; });
    }

    // The search box in the title bar, where the eye looks first, rather
    // than at the end of the menu: Doxygen's menu script makes it after the
    // page loads, so it is moved once it exists.
    function moveSearch() {
        var slot = document.getElementById("dao-search-slot");
        var box = document.getElementById("MSearchBox");
        if (!slot || !box) {
            return false;
        }
        slot.appendChild(box);
        // The results drop from the box: their window moves with it.
        var results = document.getElementById("MSearchResultsWindow");
        if (results) {
            slot.appendChild(results);
        }
        document.documentElement.classList.add("dao-search-moved");
        return true;
    }

    // The outline at the right marks one section, the one being read, as
    // the side tree marks its page. Doxygen marks every section on screen
    // ("vis"); the first of those with none inside it is the one.
    function markReading() {
        var outline = document.getElementById("page-nav-contents");
        if (!outline) {
            return;
        }
        var update = function () {
            var shown = outline.querySelectorAll("li.vis");
            var reading = null;
            for (var i = 0; i < shown.length && !reading; i++) {
                if (!shown[i].querySelector("li.vis")) {
                    reading = shown[i];
                }
            }
            outline.querySelectorAll("li.dao-reading").forEach(function (li) {
                if (li !== reading) {
                    li.classList.remove("dao-reading");
                }
            });
            if (reading && !reading.classList.contains("dao-reading")) {
                reading.classList.add("dao-reading");
            }
        };
        // Only Doxygen's changes count: the observer is off while this
        // writes its own class, so the write is not seen as a change.
        var observer = new MutationObserver(function () {
            observer.disconnect();
            update();
            observer.observe(outline, { subtree: true, attributes: true, attributeFilter: ["class"] });
        });
        update();
        observer.observe(outline, { subtree: true, attributes: true, attributeFilter: ["class"] });
    }

    document.addEventListener("DOMContentLoaded", function () {
        markReading();
        wireSidebar("nav", addToggle(document.getElementById("side-nav"), "dao-toggle-nav"));
        wireSidebar("outline", addToggle(document.getElementById("page-nav"), "dao-toggle-outline"));
        if (!moveSearch()) {
            var observer = new MutationObserver(function () {
                if (moveSearch()) {
                    observer.disconnect();
                }
            });
            observer.observe(document.body, { childList: true, subtree: true });
        }
        var select = document.getElementById("dao-version");
        if (!select) {
            return;
        }
        var here = place();
        fetch(here.root + "versions.json")
            .then(function (response) { return response.json(); })
            .then(function (versions) {
                select.innerHTML = "";
                versions.forEach(function (version) {
                    var option = document.createElement("option");
                    option.value = version.path;
                    option.textContent = version.label;
                    option.selected = version.path === here.version;
                    select.appendChild(option);
                });
            })
            .catch(function () {
                // No list (a page opened from disk): the current docs alone.
            });
        select.addEventListener("change", function () {
            var folder = here.root + select.value;
            go(folder + here.page, folder + "index.html", window.location.hash);
        });
    });
})();
