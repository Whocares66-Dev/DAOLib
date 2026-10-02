"""The claim on the game: which session may start a game or change a game
folder now. One file every agent's deploy and launch scripts honour, as the
user decided on 2026-10-02 ("use shared claim file to prevent multiple game
openings"):

    Documents\\BioWare\\Dragon Age\\daolib\\game.claim.json
    {"owner": "computer-use-agent", "note": "what for", "since": <unix s>, "until": <unix s>}

A claim stands while `until` is later than now. While another's stands,
start no game, deploy nothing and change nothing in a game folder: tell its
owner, or wait. A file that cannot be read is no claim. The owner writes it
whole, renews it by writing it again and releases it by deleting it. The
player at the keyboard is not held by it. Until every long-running bridge
has restarted, the old name, eluvian.claim.json, is read as well (the
Eluvian server's README, "The claim").

tools/ea_play.py honours it here; tools/deploy.ps1 does the same in
PowerShell.
"""

import ctypes
import json
import os
import time

NAMES = ("game.claim.json", "eluvian.claim.json")


def folder():
    """Documents\\BioWare\\Dragon Age\\daolib, wherever Documents is (under
    OneDrive on this machine)."""
    buffer = ctypes.create_unicode_buffer(260)
    csidl_personal = 5
    if (
        ctypes.windll.shell32.SHGetFolderPathW(None, csidl_personal, None, 0, buffer)
        != 0
    ):
        documents = os.path.join(os.path.expanduser("~"), "Documents")
    else:
        documents = buffer.value
    return os.path.join(documents, "BioWare", "Dragon Age", "daolib")


def standing(text, now, owner=None):
    """The claim a file's text makes, if it stands against `owner`: its
    `until` later than `now` and its owner another. None for no claim, an
    unreadable one, one run out, or the caller's own."""
    try:
        claim = json.loads(text)
        until = float(claim["until"])
    except (ValueError, TypeError, KeyError):
        return None
    if not isinstance(claim, dict) or until <= now:
        return None
    if owner is not None and claim.get("owner") == owner:
        return None
    return claim


def holder(owner=None, now=None):
    """The claim that stands against `owner` now, or None."""
    now = time.time() if now is None else now
    base = folder()
    for name in NAMES:
        try:
            with open(os.path.join(base, name), encoding="utf-8-sig") as f:
                text = f.read()
        except OSError:
            continue
        claim = standing(text, now, owner)
        if claim is not None:
            return claim
    return None


def describe(claim, now=None):
    """A line saying who holds the game and for how long."""
    now = time.time() if now is None else now
    minutes = max(0.0, (float(claim["until"]) - now) / 60)
    return (
        f"the game is claimed by {claim.get('owner', 'someone')} for another "
        f"{minutes:.1f} minutes ({claim.get('note', 'no note')})"
    )
