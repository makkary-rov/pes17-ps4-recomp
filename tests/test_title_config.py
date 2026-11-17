# SPDX-License-Identifier: GPL-2.0-or-later
import copy
import json
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on sys.path)
import title_config as tc

REAL = ROOT / "src" / "title" / "CUSA04957.json"

VALID = {
    "_license": "GPL-2.0-or-later",
    "schema": "pes-title-1",
    "title_id": "CUSA04957",
    "save_title_id": "CUSA04957",
    "eboot_elf_md5": "5213cf72d23f5bc6f3728f45091dea0c",
    "expect": {"entry": "0x910", "relocations_written": 184284, "imports": 828},
    "gnm_region": None,
    "import_suffix_map": [],
    "import_aliases": [],
}


def with_row_map(row):
    data = copy.deepcopy(VALID)
    data["import_suffix_map"] = [row]
    return data


def with_alias(row):
    data = copy.deepcopy(VALID)
    data["import_aliases"] = [row]
    return data


SUFFIX_ROW = {"suffix": "#q#q", "library": "libA", "module": "modA", "canonical": "#q#q",
              "evidence": "decoded dynamic tags"}
ALIAS_ROW = {"import": "NID#libA#modA", "canonical": "NID#q#q", "reason": "suffix differs",
             "evidence": "decoded dynamic tags"}


class RealConfigTests(unittest.TestCase):
    def test_shipped_config_loads(self):
        data = tc.load(REAL)
        self.assertEqual(data["schema"], "pes-title-1")
        self.assertEqual(data["title_id"], "CUSA04957")
        self.assertEqual(data["save_title_id"], "CUSA04957")
        self.assertEqual(data["eboot_elf_md5"], "5213cf72d23f5bc6f3728f45091dea0c")
        self.assertEqual(data["expect"], {"entry": "0x910", "relocations_written": 184284,
                                          "imports": 828})
        self.assertIsNone(data["gnm_region"])
        self.assertEqual(data["import_suffix_map"], [])
        self.assertEqual(data["import_aliases"], [])


class ValidConfigTests(unittest.TestCase):
    def test_valid_config_passes(self):
        data = copy.deepcopy(VALID)
        self.assertIs(tc.validate(data), data)

    def test_region_and_rows_pass(self):
        data = copy.deepcopy(VALID)
        data["gnm_region"] = {"lo": 0x1000, "hi": 0x2000}
        data["import_suffix_map"] = [SUFFIX_ROW]
        data["import_aliases"] = [ALIAS_ROW]
        tc.validate(data)

    def test_missing_required_key_fails(self):
        data = copy.deepcopy(VALID)
        del data["eboot_elf_md5"]
        with self.assertRaises(tc.TitleConfigError) as ctx:
            tc.validate(data)
        self.assertIn("missing key 'eboot_elf_md5'", " ".join(ctx.exception.problems))

    def test_wrong_schema_fails(self):
        data = copy.deepcopy(VALID)
        data["schema"] = "pes-title-0"
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(data)

    def test_bad_md5_fails(self):
        for value in ("5213CF72D23F5BC6F3728F45091DEA0C", "5213cf72", "z" * 32):
            with self.subTest(value=value):
                data = copy.deepcopy(VALID)
                data["eboot_elf_md5"] = value
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(data)

    def test_bad_expect_values_fail(self):
        for key, value in (("entry", "910"), ("relocations_written", -1), ("imports", True),
                           ("imports", "828")):
            with self.subTest(key=key, value=value):
                data = copy.deepcopy(VALID)
                data["expect"][key] = value
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(data)

    def test_bad_region_fails(self):
        for region in ({"lo": 5, "hi": 5}, {"lo": 1}, {"lo": 1, "hi": 2, "x": 3}, [1, 2]):
            with self.subTest(region=region):
                data = copy.deepcopy(VALID)
                data["gnm_region"] = region
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(data)


class UnknownKeyTests(unittest.TestCase):
    def test_unknown_top_level_key_fails(self):
        data = copy.deepcopy(VALID)
        data["notes"] = "extra"
        with self.assertRaises(tc.TitleConfigError) as ctx:
            tc.validate(data)
        self.assertIn("unknown key 'notes'", " ".join(ctx.exception.problems))

    def test_unknown_expect_key_fails(self):
        data = copy.deepcopy(VALID)
        data["expect"]["extra"] = 1
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(data)

    def test_unknown_row_key_fails(self):
        row = dict(SUFFIX_ROW, extra="x")
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(with_row_map(row))
        alias = dict(ALIAS_ROW, extra="x")
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(with_alias(alias))

    def test_license_key_is_allowed(self):
        tc.validate(copy.deepcopy(VALID))

    def test_wrong_license_fails(self):
        data = copy.deepcopy(VALID)
        data["_license"] = "MIT"
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(data)


class SuffixSyntaxTests(unittest.TestCase):
    def test_good_suffixes_pass(self):
        for suffix in ("#q#q", "#p#J", "#A+-#9", "#abc#xyz"):
            with self.subTest(suffix=suffix):
                row = dict(SUFFIX_ROW, suffix=suffix, canonical=suffix)
                tc.validate(with_row_map(row))

    def test_bad_suffixes_fail(self):
        for suffix in ("q#q", "#q#q#", "#qqqq#q", "#q#qqqq", "##q", "#q#", "#!#q", "#q#q "):
            with self.subTest(suffix=suffix):
                row = dict(SUFFIX_ROW, suffix=suffix)
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(with_row_map(row))

    def test_bad_canonical_fails(self):
        row = dict(SUFFIX_ROW, canonical="q#q")
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(with_row_map(row))

    def test_duplicate_suffix_fails(self):
        data = copy.deepcopy(VALID)
        data["import_suffix_map"] = [SUFFIX_ROW, dict(SUFFIX_ROW, library="other")]
        with self.assertRaises(tc.TitleConfigError):
            tc.validate(data)


class RawBytesTests(unittest.TestCase):
    def test_forbidden_keys_fail_at_any_depth(self):
        for key in ("bytes", "signature", "Bytes"):
            with self.subTest(key=key):
                data = copy.deepcopy(VALID)
                data[key] = "x"
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(data)
                nested = with_alias(dict(ALIAS_ROW, **{key: "x"}))
                with self.assertRaises(tc.TitleConfigError):
                    tc.validate(nested)

    def test_long_hex_string_fails(self):
        data = with_alias(dict(ALIAS_ROW, reason="ab" * 33))
        with self.assertRaises(tc.TitleConfigError) as ctx:
            tc.validate(data)
        self.assertIn("raw bytes", " ".join(ctx.exception.problems))

    def test_hex_string_at_limit_passes(self):
        tc.validate(with_alias(dict(ALIAS_ROW, reason="ab" * 32)))

    def test_long_text_is_not_hex(self):
        tc.validate(with_alias(dict(ALIAS_ROW, reason="a" * 70 + " note")))


class LoadTests(unittest.TestCase):
    def test_load_reports_invalid_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bad.json"
            path.write_text("{ not json", encoding="utf-8")
            with self.assertRaises(tc.TitleConfigError):
                tc.load(path)

    def test_load_valid_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ok.json"
            path.write_text(json.dumps(VALID), encoding="utf-8")
            self.assertEqual(tc.load(path), VALID)


if __name__ == "__main__":
    unittest.main()
