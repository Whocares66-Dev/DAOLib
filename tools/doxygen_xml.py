"""Doxygen's XML of DAOLib's public headers, as Markdown pages for the docs site (DAOLib #49).

Doxygen reads the headers (through tools/doxygen_filter.py) and writes XML,
one file per namespace and class; this turns that into the site's API pages,
which Jekyll and Just the Docs build with the guide and the front page:

- api/index.md, the reference's landing page, listing the namespaces;
- api/<namespace>.md for each namespace under dao (dao itself is api/dao.md),
  its classes listed and its type aliases, enumerations, functions and
  variables documented, each a heading with an anchor of its name;
- api/<namespace>/<Class>.md for each class or struct, a child of its
  namespace's page.

A member whose name is unique on its page has the anchor #Name; an overloaded
name has #Name, #Name-2, ... in the header's order. Doxygen's references
become links to these pages and anchors.

    python tools/doxygen_xml.py <doxygen xml folder> <site folder>
"""

import argparse
import html
import pathlib
import posixpath
import re
import sys
import xml.etree.ElementTree as ET

# The order kinds of member appear in on a page, and their headings.
SECTIONS = [
    ("typedef", "Type aliases"),
    ("enum", "Enumerations"),
    ("function", "Functions"),
    ("variable", "Variables"),
]


def slug(name):
    """A page's file name for a namespace or class name's last part."""
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


class Site:
    """Every page and anchor, by Doxygen's ids, so that references link."""

    def __init__(
        self,
        xml_dir,
        ref="master",
        repository="https://github.com/Whocares66-Dev/DAOLib",
    ):
        self.xml_dir = pathlib.Path(xml_dir)
        self.source = (
            f"{repository}/blob/{ref}/src/"  # a header's source, at this version
        )
        self.compounds = {}  # refid -> element of the compound's XML
        self.url = {}  # refid -> "/api/..." page URL (no anchor) or URL#anchor
        self.namespaces = []  # refids of dao's namespaces, dao first
        self.classes = {}  # namespace refid -> [class refids]
        self.current = "/api/"  # the URL of the page being made, for relative links
        index = ET.parse(self.xml_dir / "index.xml").getroot()
        for compound in index.findall("compound"):
            kind, refid = compound.get("kind"), compound.get("refid")
            name = compound.findtext("name")
            if not name.startswith("dao") or kind not in (
                "namespace",
                "class",
                "struct",
                "union",
            ):
                continue
            path = self.xml_dir / f"{refid}.xml"
            if not path.exists():
                continue
            self.compounds[refid] = ET.parse(path).getroot().find("compounddef")
        for refid, compound in self.compounds.items():
            if compound.get("kind") == "namespace":
                self.namespaces.append(refid)
        self.namespaces.sort(
            key=lambda r: (self.name(r) != "dao", self.name(r).lower())
        )
        for refid in self.namespaces:
            self.url[refid] = "/api/" + slug(self.short(refid)) + ".html"
            self.classes[refid] = []
            for inner in self.compounds[refid].findall("innerclass"):
                if inner.get("refid") in self.compounds:
                    self.classes[refid].append(inner.get("refid"))
        for namespace, classes in self.classes.items():
            folder = "/api/" + slug(self.short(namespace)) + "/"
            for refid in classes:
                self.url[refid] = (
                    folder + slug(self.name(refid).split("::")[-1]) + ".html"
                )
                for inner in self.compounds[refid].findall("innerclass"):
                    nested = inner.get("refid")
                    if nested in self.compounds:
                        # A nested class is documented on its outer class's page.
                        self.url[nested] = self.url[refid]
        for refid in list(self.url):
            self.anchor_members(refid)

    def name(self, refid):
        return self.compounds[refid].findtext("compoundname")

    def short(self, refid):
        """dao::effects is "effects"; dao itself is "dao"."""
        name = self.name(refid)
        return name.split("::", 1)[1] if "::" in name else name

    def members(self, refid):
        compound = self.compounds[refid]
        for section in compound.findall("sectiondef"):
            for member in section.findall("memberdef"):
                if member.get("prot", "public") == "public":
                    yield member

    def anchor_members(self, refid):
        """Each member's anchor on its page, and its URL by Doxygen's id."""
        seen = {}
        for member in self.members(refid):
            name = member.findtext("name") or ""
            base = slug(name) or "member"
            seen[base] = seen.get(base, 0) + 1
            anchor = base if seen[base] == 1 else f"{base}-{seen[base]}"
            member.set("dao-anchor", anchor)
            self.url[member.get("id")] = f"{self.url[refid]}#{anchor}"
            for value in member.findall("enumvalue"):
                self.url[value.get("id")] = f"{self.url[refid]}#{anchor}"

    def link(self, target):
        """A URL of the site (/api/x.html#a), as a path relative to the page being made."""
        path, _, anchor = target.partition("#")
        here = (
            self.current
            if self.current.endswith("/")
            else posixpath.dirname(self.current) + "/"
        )
        relative = posixpath.relpath(path, here) if path != self.current else ""
        return relative + (f"#{anchor}" if anchor else "")

    # -- descriptions as Markdown ------------------------------------------

    def text(self, element):
        """An element's text with its markup dropped (a type, a name)."""
        return (
            re.sub(r"\s+", " ", "".join(element.itertext())).strip()
            if element is not None
            else ""
        )

    def inline(self, element, code=False):
        """Inline markup as Markdown: code, references, emphasis."""
        out = [
            html.escape(element.text or "", quote=False)
            if not code
            else (element.text or "")
        ]
        for child in element:
            tag = child.tag
            if tag == "computeroutput":
                inner = self.inline(child, code=True)
                out.append(f"`{inner}`" if inner and "`" not in inner else inner)
            elif tag == "ref":
                label = self.inline(child, code=code)
                target = self.url.get(child.get("refid"))
                if target and not code:
                    out.append(f"[{label}]({self.link(target)})")
                else:
                    out.append(label)
            elif tag in ("bold", "emphasis"):
                mark = "**" if tag == "bold" else "*"
                out.append(f"{mark}{self.inline(child, code)}{mark}")
            elif tag == "ulink":
                out.append(f"[{self.inline(child, code)}]({child.get('url')})")
            elif tag == "linebreak":
                out.append("  \n")
            elif tag == "sp":
                out.append(" ")
            elif tag in (
                "parameterlist",
                "simplesect",
                "itemizedlist",
                "orderedlist",
                "programlisting",
            ):
                pass  # blocks, handled by block()
            else:
                out.append(self.inline(child, code))
            tail = child.tail or ""
            out.append(tail if code else html.escape(tail, quote=False))
        return "".join(out)

    def block(self, description):
        """A description's paragraphs, lists, examples and sections."""
        if description is None:
            return [], [], [], []
        paragraphs, params, returns, notes = [], [], [], []
        for para in description.findall("para"):
            text = re.sub(r"[ \t]+", " ", self.inline(para)).strip()
            if text:
                paragraphs.append(text)
            for child in para:
                if child.tag == "parameterlist" and child.get("kind") in (
                    "param",
                    "templateparam",
                ):
                    for item in child.findall("parameteritem"):
                        names = ", ".join(
                            f"`{self.text(n)}`" for n in item.iter("parametername")
                        )
                        desc = " ".join(
                            re.sub(r"\s+", " ", self.inline(p)).strip()
                            for p in item.find("parameterdescription").findall("para")
                        )
                        params.append(f"| {names} | {desc} |")
                elif child.tag == "simplesect":
                    body = " ".join(
                        re.sub(r"\s+", " ", self.inline(p)).strip()
                        for p in child.findall("para")
                    )
                    if child.get("kind") == "return":
                        returns.append(body)
                    else:
                        notes.append((child.get("kind"), body))
                elif child.tag in ("itemizedlist", "orderedlist"):
                    marker = "-" if child.tag == "itemizedlist" else "1."
                    items = []
                    for item in child.findall("listitem"):
                        lines = [
                            re.sub(r"\s+", " ", self.inline(p)).strip()
                            for p in item.findall("para")
                        ]
                        items.append(
                            f"{marker} " + " ".join(line for line in lines if line)
                        )
                    paragraphs.append("\n".join(items))
                elif child.tag == "programlisting":
                    lines = []
                    for line in child.findall("codeline"):
                        lines.append(self.inline(line, code=True))
                    paragraphs.append("```cpp\n" + "\n".join(lines).rstrip() + "\n```")
        return paragraphs, params, returns, notes

    def description(self, element):
        """A member's or compound's whole description, as Markdown lines."""
        brief, _, _, _ = self.block(element.find("briefdescription"))
        detail, params, returns, notes = self.block(element.find("detaileddescription"))
        out = []
        out += brief
        out += detail
        if params:
            out.append("| Parameter | |\n|:--|:--|\n" + "\n".join(params))
        for body in returns:
            out.append(f"**Returns** {body}")
        for kind, body in notes:
            label = {"note": "Note", "see": "See also", "warning": "Warning"}.get(
                kind, kind.capitalize()
            )
            out.append(f"**{label}:** {body}")
        return out

    def brief(self, element):
        brief, _, _, _ = self.block(element.find("briefdescription"))
        return " ".join(brief)

    # -- pages --------------------------------------------------------------

    def signature(self, member):
        kind = member.get("kind")
        if kind == "function":
            text = f"{self.text(member.find('type'))} {member.findtext('name')}{self.text(member.find('argsstring'))}"
        elif kind == "typedef":
            text = f"using {member.findtext('name')} = {self.text(member.find('type'))}"
        elif kind == "variable":
            # "= value" after a space; a brace initializer ({...}) right after the name.
            init = self.text(member.find("initializer"))
            init = f" {init}" if init.startswith("=") else init
            text = f"{self.text(member.find('type'))} {member.findtext('name')}{init}"
        elif kind == "enum":
            underlying = self.text(member.find("type"))
            strong = " class" if member.get("strong") == "yes" else ""
            text = f"enum{strong} {member.findtext('name')}" + (
                f" : {underlying}" if underlying else ""
            )
        else:
            text = self.text(member.find("definition"))
        # Not "inline": it says how a function is compiled, not how it is used.
        for prefix in ("static ", "constexpr "):
            if member.get(prefix.strip()) == "yes" and not text.startswith(prefix):
                text = prefix + text
        text = re.sub(r"\s+", " ", text).replace(" ,", ",").strip()
        return re.sub(r"\s*>", ">", re.sub(r"<\s*", "<", text))

    def member_markdown(self, member, level):
        anchor = member.get("dao-anchor")
        name = member.findtext("name")
        call = "()" if member.get("kind") == "function" else ""
        out = [
            # The class marks a member's heading, set in the code face (custom.scss).
            f"{'#' * level} {name}{call} {{#{anchor}}}",
            "{: .member}",
            "",
            "```cpp",
            self.signature(member),
            "```",
            "",
        ]
        for line in self.description(member):
            out += [line, ""]
        values = member.findall("enumvalue")
        if values:
            rows = []
            for value in values:
                init = self.text(value.find("initializer"))
                rows.append(
                    f"| `{value.findtext('name')}`{(' ' + init) if init else ''} | {self.brief(value)} |"
                )
            out += ["| Value | |", "|:--|:--|", *rows, ""]
        return out

    def members_markdown(self, refid, level):
        out = []
        members = list(self.members(refid))
        for kind, heading in SECTIONS:
            chosen = sorted(
                (m for m in members if m.get("kind") == kind),
                key=lambda m: m.findtext("name").lower(),
            )
            if not chosen:
                continue
            if kind == "variable" and self.compounds[refid].get("kind") != "namespace":
                out += [f"{'#' * level} Fields", ""] + self.fields_table(chosen)
                continue
            out += [f"{'#' * level} {heading}", ""]
            for member in chosen:
                out += self.member_markdown(member, level + 1)
        return out

    def fields_table(self, fields):
        """A class's or struct's fields as one table: name, type and default,
        description. Each row keeps its member's anchor."""
        rows = ["| Field | Type | |", "|:--|:--|:--|"]
        for field in fields:
            init = self.text(field.find("initializer"))
            kind = self.text(field.find("type")) + (
                f" {init}" if init.startswith("=") else init
            )
            kind = re.sub(r"\s*>", ">", re.sub(r"<\s*", "<", kind)).replace("|", "\\|")
            description = (
                " ".join(self.description(field)).replace("\n", " ").replace("|", "\\|")
            )
            name = f'<span id="{field.get("dao-anchor")}"></span>`{field.findtext("name")}`'
            rows.append(f"| {name} | `{kind}` | {description} |")
        return rows + [""]

    def header_link(self, header):
        """A header's name, linked to its source on GitHub at this version."""
        return f"[`{header}`]({self.source}{header})"

    def headers_of(self, refid):
        files = sorted(
            {
                m.find("location").get("file")
                for m in self.members(refid)
                if m.find("location") is not None
            }
        )
        return files

    @staticmethod
    def raw(lines):
        """A page's body kept from Liquid: C++ braces ({{...}}) are not its tags."""
        front_end = lines.index("---", 1) + 1
        return (
            "\n".join(lines[:front_end] + ["{% raw %}"] + lines[front_end:]).rstrip()
            + "\n{% endraw %}\n"
        )

    def front_matter(self, **fields):
        lines = ["---"]
        for key, value in fields.items():
            if value is None:
                continue
            if isinstance(value, bool):
                text = "true" if value else "false"
            elif isinstance(value, str):
                text = '"' + value.replace('"', '\\"') + '"'
            else:
                text = str(value)
            lines.append(f"{key}: {text}")
        return lines + ["---", ""]

    def namespace_page(self, refid, order):
        name = self.name(refid)
        self.current = self.url[refid]
        out = self.front_matter(
            title=name,
            parent="API reference",
            nav_order=order,
            has_children=bool(self.classes[refid]),
            permalink=self.url[refid],
        )
        out += [f"# {name}", ""]
        for line in self.description(self.compounds[refid]):
            out += [line, ""]
        headers = self.headers_of(refid)
        if headers:
            out += [
                "Declared in " + ", ".join(self.header_link(h) for h in headers) + ".",
                "",
            ]
        classes = sorted(self.classes[refid], key=lambda r: self.name(r).lower())
        if classes:
            out += ["## Classes", "", "| Class | |", "|:--|:--|"]
            for cls in classes:
                short = self.name(cls).split("::")[-1]
                out.append(
                    f"| [{short}]({self.link(self.url[cls])}) | {self.brief(self.compounds[cls])} |"
                )
            out.append("")
        out += self.members_markdown(refid, 2)
        return self.raw(out)

    def class_page(self, refid, namespace, order):
        compound = self.compounds[refid]
        name = self.name(refid)
        kind = compound.get("kind")
        self.current = self.url[refid]
        out = self.front_matter(
            title=name.split("::")[-1],
            parent=self.name(namespace),
            grand_parent="API reference",
            nav_order=order,
            permalink=self.url[refid],
        )
        out += [f"# {name}", "", f"*{kind}*", ""]
        bases = []
        for base in compound.findall("basecompoundref"):
            if base.get("prot") != "public":
                continue
            target = self.url.get(base.get("refid"))
            bases.append(
                f"[`{self.text(base)}`]({self.link(target)})"
                if target
                else f"`{self.text(base)}`"
            )
        if bases:
            out += ["Derives from " + ", ".join(bases) + ".", ""]
        for line in self.description(compound):
            out += [line, ""]
        header = compound.find("location")
        if header is not None:
            out += [f"Declared in {self.header_link(header.get('file'))}.", ""]
        out += self.members_markdown(refid, 2)
        for inner in compound.findall("innerclass"):
            nested = inner.get("refid")
            if nested in self.compounds:
                out += [f"## {self.name(nested).split('::')[-1]}", ""]
                for line in self.description(self.compounds[nested]):
                    out += [line, ""]
                out += self.members_markdown(nested, 3)
        return self.raw(out)

    def index_page(self):
        self.current = "/api/"
        out = self.front_matter(
            title="API reference", nav_order=4, has_children=True, permalink="/api/"
        )
        out += [
            "# API reference",
            "",
            "The library's namespaces, each with what it is for.",
            "",
            "| Namespace | |",
            "|:--|:--|",
        ]
        for refid in self.namespaces:
            out.append(
                f"| [{self.name(refid)}]({self.link(self.url[refid])}) | {self.brief(self.compounds[refid])} |"
            )
        return self.raw(out)

    def write(self, site_dir):
        site = pathlib.Path(site_dir)
        api = site / "api"
        api.mkdir(parents=True, exist_ok=True)
        (api / "index.md").write_text(self.index_page(), encoding="utf-8")
        count = 1
        for order, refid in enumerate(self.namespaces, start=1):
            (api / f"{slug(self.short(refid))}.md").write_text(
                self.namespace_page(refid, order), encoding="utf-8"
            )
            count += 1
            classes = sorted(self.classes[refid], key=lambda r: self.name(r).lower())
            for corder, cls in enumerate(classes, start=1):
                path = site / self.url[cls].lstrip("/").replace(".html", ".md")
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(self.class_page(cls, refid, corder), encoding="utf-8")
                count += 1
        return count


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("xml", help="Doxygen's xml folder")
    parser.add_argument("site", help="the Jekyll source folder to write api/ into")
    args = parser.parse_args()
    count = Site(args.xml).write(args.site)
    print(f"{count} API pages in {pathlib.Path(args.site) / 'api'}")


if __name__ == "__main__":
    sys.exit(main())
