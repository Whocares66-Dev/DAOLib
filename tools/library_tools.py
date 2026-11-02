"""Puts the address library's tools on the path, so a tool here can import its
daoexe and msvc_names (extern/DAOAddressLibrary, the only copies): import this
first."""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# After this folder, so a tool of the same name here is the one imported.
sys.path.insert(1, os.path.join(HERE, "..", "extern", "DAOAddressLibrary", "tools"))


def disasm_range(img, start, end):
    """The instructions from start, linearly, in the bytes before end. daoexe's
    disasm goes by a count or to the first ret, and a count alone would decode
    the rest of the section."""
    import capstone

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    return list(md.disasm(img.bytes_at(start, end - start), start))
