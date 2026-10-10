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


class Faults(unittest.TestCase):
    """An ID one build lacks would be called at address 0 there (DAOLib #46)."""

    def faults(self, addresses):
        row = SimpleNamespace(kind="function", name="Engine::Fn", addresses=addresses)
        db = SimpleNamespace(rows={10547: row}, columns=["gog", "ea", "steam"])
        path = address_ids.os.path.join(address_ids.ROOT, "src", "dao", "Addresses.h")
        return address_ids.faults_of(db, "gog", path, "DAO_ADDRESS(kFn, 10547)\n")

    def test_an_id_in_every_build_is_no_fault(self):
        self.assertEqual(self.faults({"gog": 1, "ea": 2, "steam": 1}), [])

    def test_an_id_a_build_lacks_is_a_fault_naming_that_build(self):
        faults = self.faults({"gog": 1, "ea": 2})
        self.assertEqual(len(faults), 1)
        self.assertIn("steam", faults[0])


if __name__ == "__main__":
    unittest.main()
