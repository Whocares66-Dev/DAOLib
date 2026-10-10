"""Doxygen's input filter for DAOLib's public headers (DAOLib #45).

The headers are commented in plain `//` prose, as the project's style asks,
and Doxygen reads only its own forms (`///`, `///<`). So each header passes
through this filter on its way into Doxygen, and the sources keep their style:

- the comment block at the head of the file becomes the file's description
  (`/// @file`), so a header's page opens with what the header is for, and
  ends with a link to the header's source on GitHub;
- a comment on lines of its own becomes `///`, the documentation of the
  declaration below it;
- within one, lines indented past the prose (a usage example, `//   code`)
  are fenced as a code block, unless they continue a list item above them;
- in prose, a backslash and angle brackets are escaped, so a Windows path
  (`Documents\BioWare`) or `<slot>` reads as text, not a Doxygen command or
  an HTML tag; a `code span` and a fenced example are left as they are;
- a comment after code on the same line (a struct field's) becomes `///<`,
  the documentation of that member;
- a comment just before a `namespace` line, a lint marker (`// NOLINT...`)
  and a line of dashes are left alone.

Usage, as Doxygen calls it (INPUT_FILTER): doxygen_filter.py <file>; the
filtered text goes to standard output.
"""

import os
import re
import sys

# The file's source on GitHub, linked from its page in place of a copy:
# tools/build_docs.py says which version (master, or a release's tag).
REPOSITORY = "https://github.com/Whocares66-Dev/DAOLib"
REF = os.environ.get("DAOLIB_DOCS_REF", "master")
SOURCE = None  # the file's path from the repository's root, set in main()

OWN_LINE = re.compile(r"^(\s*)//(?!/)(.*)$")
AFTER_CODE = re.compile(r"^(\s*\S.*?)\s+//(?![/<])\s?(.*)$")
NOT_DOCUMENTATION = re.compile(r"^\s*(NOLINT|-{3,}|={3,})")
LIST_ITEM = re.compile(r"^\s*([-*]|\d+\.)\s")
CODE_INDENT = 3  # spaces after `//` from which a comment line is code, not prose


def is_code(text):
    return text.startswith(" " * CODE_INDENT) and text.strip() != ""


def escaped(text):
    """Prose as Doxygen should show it: outside `code spans`, a backslash and
    angle brackets escaped."""
    parts = text.split("`")
    for i in range(0, len(parts), 2):
        parts[i] = (
            parts[i].replace("\\", "\\\\").replace("<", "\\<").replace(">", "\\>")
        )
    return "`".join(parts)


def block(comment, indent, file_head):
    """One run of own-line comments as Doxygen's, its examples fenced."""
    out = [f"{indent}/// @file"] if file_head else []
    in_code = False
    in_item = False  # the lines since a list item, with no blank between
    for i, text in enumerate(comment):
        blank = text.strip() == ""
        code = is_code(text) and not in_item
        if code and not in_code:
            out.append(f"{indent}/// ```")
            in_code = True
        elif in_code and not code:
            # A blank line inside an example keeps it open if code follows.
            rest = comment[i + 1 :]
            if not (blank and rest and is_code(rest[0])):
                out.append(f"{indent}/// ```")
                in_code = False
        if in_code:
            out.append(f"{indent}///{'' if blank else text[CODE_INDENT:]}")
        else:
            out.append(f"{indent}///{escaped(text)}")
            in_item = not blank and (in_item or bool(LIST_ITEM.match(text)))
    if in_code:
        out.append(f"{indent}/// ```")
    if file_head and SOURCE:
        out.append(f"{indent}///")
        out.append(f"{indent}/// Source: [{SOURCE}]({REPOSITORY}/blob/{REF}/{SOURCE})")
    return out


def filtered(lines):
    """The lines of a header, its comments in Doxygen's forms."""
    out = []
    i = 0
    while i < len(lines):
        own = OWN_LINE.match(lines[i])
        if own and not NOT_DOCUMENTATION.match(own.group(2)):
            indent = own.group(1)
            start = i
            comment = []
            while i < len(lines):
                more = OWN_LINE.match(lines[i])
                if not more or NOT_DOCUMENTATION.match(more.group(2)):
                    break
                comment.append(more.group(2))
                i += 1
            # A comment that documents nothing below it stays a plain comment:
            # one before `namespace dao {` is the file's, not the namespace's
            # (many headers open the same namespace); one before a
            # preprocessor line or a blank line is a note, not a declaration's.
            following = lines[i].strip() if i < len(lines) else ""
            if start != 0 and (
                following == "" or following.startswith(("namespace", "#"))
            ):
                out.extend(lines[start:i])
                continue
            out.extend(block(comment, indent, file_head=start == 0))
            continue
        line = lines[i]
        i += 1
        after = AFTER_CODE.match(line)
        # A member's comment: not a `//` inside a string literal (an odd
        # number of quotes before it), not a lint marker, and not the note
        # after a closing brace (`} // namespace dao`).
        if (
            not own
            and after
            and after.group(1).count('"') % 2 == 0
            and "NOLINT" not in after.group(2)
            and not after.group(1).strip().startswith("}")
        ):
            code, text = after.groups()
            out.append(f"{code} ///< {escaped(text)}")
            continue
        out.append(line)
    return out


def main():
    global SOURCE  # noqa: PLW0603
    SOURCE = os.path.relpath(sys.argv[1], os.getcwd()).replace(os.sep, "/")
    with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdout.write("\n".join(filtered(lines)))


if __name__ == "__main__":
    main()
