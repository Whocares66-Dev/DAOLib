"""The listing stays GOG's when Steam shares its timestamp in the catalog."""

import unittest
from types import SimpleNamespace

import address_ids


class Listing(unittest.TestCase):
    def test_shared_timestamp_selects_the_full_gog_fingerprint(self):
        gog = {k: hex(v) for k, v in address_ids.LISTING.items()} | {
            "key": "renamed-gog"
        }
        steam = gog | {
            "key": "steam",
            "size_of_image": "0xa49000",
            "entry_point_rva": "0x9ed2ee",
        }
        db = SimpleNamespace(builds=[steam, gog], columns=["steam", "renamed-gog"])
        self.assertEqual(address_ids.listing(db), "renamed-gog")
        db.builds = [steam]
        with self.assertRaises(SystemExit):
            address_ids.listing(db)

    def test_missing_column_or_duplicate_fingerprint_is_rejected(self):
        gog = {k: hex(v) for k, v in address_ids.LISTING.items()} | {"key": "gog"}
        db = SimpleNamespace(builds=[gog], columns=[])
        with self.assertRaises(SystemExit):
            address_ids.listing(db)
        db.columns = ["gog", "duplicate"]
        db.builds.append(gog | {"key": "duplicate"})
        with self.assertRaises(SystemExit):
            address_ids.listing(db)


if __name__ == "__main__":
    unittest.main()
