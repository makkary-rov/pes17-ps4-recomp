# SPDX-License-Identifier: GPL-2.0-or-later
"""Title config (schema "pes-title-1"): load and validate. Stdlib only.

A title config holds public-safe facts only: ids, the MD5 of the input, expected
counts and addresses, and name mappings. It never holds game bytes.
"""
import json
import re
from pathlib import Path

SCHEMA = "pes-title-1"
LICENSE = "GPL-2.0-or-later"
SUFFIX_RE = re.compile(r'^#[A-Za-z0-9+-]{1,3}#[A-Za-z0-9+-]{1,3}$')
TITLE_ID_RE = re.compile(r'^[A-Z]{4}[0-9]{5}$')
MD5_RE = re.compile(r'^[0-9a-f]{32}$')
ENTRY_RE = re.compile(r'^0x[0-9a-fA-F]+$')
HEX_RE = re.compile(r'^(0x)?[0-9a-fA-F]+$')
HEX_STRING_LIMIT = 64
FORBIDDEN_KEYS = {"bytes", "signature"}

TOP_KEYS = {"_license", "schema", "title_id", "save_title_id", "eboot_elf_md5", "expect",
            "gnm_region", "import_suffix_map", "import_aliases"}
EXPECT_KEYS = {"entry", "relocations_written", "imports"}
REGION_KEYS = {"lo", "hi"}
SUFFIX_ROW_KEYS = {"suffix", "library", "module", "canonical", "evidence"}
ALIAS_ROW_KEYS = {"import", "canonical", "reason", "evidence"}


class TitleConfigError(ValueError):
    """Raised with every problem found in a title config (see .problems)."""

    def __init__(self, problems):
        self.problems = list(problems)
        super().__init__("; ".join(self.problems))


def _is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def _is_text(value):
    return isinstance(value, str) and value.strip() != ""


def _check_object(where, obj, keys, required, errors):
    if not isinstance(obj, dict):
        errors.append(f"{where}: must be an object")
        return False
    for key in obj:
        if key not in keys:
            errors.append(f"{where}: unknown key {key!r}")
    missing = [key for key in required if key not in obj]
    for key in missing:
        errors.append(f"{where}: missing key {key!r}")
    return not missing


def _scan_raw(where, value, errors):
    """Reject raw-byte carriers: keys named bytes/signature and long hex strings."""
    if isinstance(value, dict):
        for key, child in value.items():
            if key.lower() in FORBIDDEN_KEYS:
                errors.append(f"{where}: key {key!r} is not allowed (raw bytes)")
            _scan_raw(f"{where}.{key}", child, errors)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _scan_raw(f"{where}[{index}]", child, errors)
    elif isinstance(value, str):
        if len(value) > HEX_STRING_LIMIT and HEX_RE.match(value):
            errors.append(f"{where}: hex string longer than {HEX_STRING_LIMIT} characters (raw bytes)")


def _check_expect(expect, errors):
    if not _check_object("expect", expect, EXPECT_KEYS, EXPECT_KEYS, errors):
        return
    if "entry" in expect and not (isinstance(expect["entry"], str) and ENTRY_RE.match(expect["entry"])):
        errors.append(f"expect.entry: must look like 0x<hex>, got {expect['entry']!r}")
    for key in ("relocations_written", "imports"):
        if key in expect and not (_is_int(expect[key]) and expect[key] >= 0):
            errors.append(f"expect.{key}: must be a non-negative integer")


def _check_region(region, errors):
    if region is None:
        return
    if not _check_object("gnm_region", region, REGION_KEYS, REGION_KEYS, errors):
        return
    if not all(_is_int(region[key]) for key in REGION_KEYS):
        errors.append("gnm_region: lo and hi must be integers")
    elif not 0 <= region["lo"] < region["hi"]:
        errors.append("gnm_region: need 0 <= lo < hi")


def _check_suffix_rows(rows, errors):
    if not isinstance(rows, list):
        errors.append("import_suffix_map: must be a list")
        return
    seen = set()
    for index, row in enumerate(rows):
        where = f"import_suffix_map[{index}]"
        if not _check_object(where, row, SUFFIX_ROW_KEYS, SUFFIX_ROW_KEYS, errors):
            continue
        for key in ("suffix", "canonical"):
            if not (isinstance(row[key], str) and SUFFIX_RE.match(row[key])):
                errors.append(f"{where}.{key}: bad suffix syntax {row[key]!r}")
        for key in ("library", "module", "evidence"):
            if not _is_text(row[key]):
                errors.append(f"{where}.{key}: must be a non-empty string")
        if row["suffix"] in seen:
            errors.append(f"{where}.suffix: duplicate {row['suffix']!r}")
        seen.add(row["suffix"])


def _check_alias_rows(rows, errors):
    if not isinstance(rows, list):
        errors.append("import_aliases: must be a list")
        return
    seen = set()
    for index, row in enumerate(rows):
        where = f"import_aliases[{index}]"
        if not _check_object(where, row, ALIAS_ROW_KEYS, ALIAS_ROW_KEYS, errors):
            continue
        for key in ALIAS_ROW_KEYS:
            if not _is_text(row[key]):
                errors.append(f"{where}.{key}: must be a non-empty string")
        if _is_text(row["import"]):
            if row["import"] in seen:
                errors.append(f"{where}.import: duplicate {row['import']!r}")
            seen.add(row["import"])


def validate(data):
    """Return data if it is a valid pes-title-1 config; otherwise raise TitleConfigError."""
    errors = []
    if not isinstance(data, dict):
        raise TitleConfigError(["title config must be a JSON object"])
    _scan_raw("config", data, errors)
    _check_object("config", data, TOP_KEYS, TOP_KEYS, errors)
    if data.get("_license") != LICENSE:
        errors.append(f"_license: must be {LICENSE!r}")
    if data.get("schema") != SCHEMA:
        errors.append(f"schema: must be {SCHEMA!r}")
    for key in ("title_id", "save_title_id"):
        if key in data and not (isinstance(data[key], str) and TITLE_ID_RE.match(data[key])):
            errors.append(f"{key}: must look like CUSA12345, got {data[key]!r}")
    if "eboot_elf_md5" in data and not (isinstance(data["eboot_elf_md5"], str)
                                        and MD5_RE.match(data["eboot_elf_md5"])):
        errors.append("eboot_elf_md5: must be 32 lowercase hex characters")
    if "expect" in data:
        _check_expect(data["expect"], errors)
    if "gnm_region" in data:
        _check_region(data["gnm_region"], errors)
    if "import_suffix_map" in data:
        _check_suffix_rows(data["import_suffix_map"], errors)
    if "import_aliases" in data:
        _check_alias_rows(data["import_aliases"], errors)
    if errors:
        raise TitleConfigError(errors)
    return data


def load(path):
    """Read, parse and validate a title config file; return the dict."""
    text = Path(path).read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise TitleConfigError([f"{path}: invalid JSON: {exc}"]) from exc
    return validate(data)
