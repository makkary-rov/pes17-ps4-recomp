<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Title config (`pes-title-1`)

A title config is one JSON file per title, stored at `src/title/<TITLE_ID>.json`.
It records public-safe facts that the prepare, loader and tests need: ids, the MD5
of the input executable, expected counts and addresses, and name mappings.

It never holds game bytes. The validator rejects keys named `bytes` or `signature`
at any depth, and any string longer than 64 characters that is all hex.

Loader code: `scripts/title_config.py` (stdlib only). `load(path)` reads, parses
and validates a file. `validate(data)` checks a parsed dict. Both raise
`TitleConfigError`, whose `.problems` lists every problem found.

## Fields

| Key | Type | Meaning |
|---|---|---|
| `_license` | string | Must be `"GPL-2.0-or-later"`. JSON has no comments, so this key carries the licence line. |
| `schema` | string | Must be `"pes-title-1"`. |
| `title_id` | string | Title id, `CUSA` plus five digits (`CUSA04957`). |
| `save_title_id` | string | Title id used for the save root (`<user dir>/savedata/1/<save_title_id>`, where the user dir is set as in BUILDING.md section 5). Same format. |
| `eboot_elf_md5` | string | MD5 of the input `eboot.elf`, 32 lowercase hex characters. |
| `expect` | object | `entry` (`0x` hex address), `relocations_written` (integer), `imports` (integer). No other keys. |
| `gnm_region` | object or null | `{"lo": int, "hi": int}` (JSON decimal integers, not hex strings) with `0 <= lo < hi`, guest addresses. `null` until the region is measured. |
| `import_suffix_map` | list | Per-library suffix rows (see below). |
| `import_aliases` | list | Per-import alias rows (see below). |

All keys are required. Unknown keys fail validation at every level.

### `import_suffix_map` rows

Each row has exactly these keys: `suffix`, `library`, `module`, `canonical`, `evidence`.

- `suffix` and `canonical` must match `^#[A-Za-z0-9+-]{1,3}#[A-Za-z0-9+-]{1,3}$`.
- `library`, `module` and `evidence` must be non-empty strings.
- `suffix` must be unique within the list.

### `import_aliases` rows

Each row has exactly these keys: `import`, `canonical`, `reason`, `evidence`.
All four must be non-empty strings. `import` must be unique within the list.

## Example

```json
{
  "_license": "GPL-2.0-or-later",
  "schema": "pes-title-1",
  "title_id": "CUSA04957",
  "save_title_id": "CUSA04957",
  "eboot_elf_md5": "5213cf72d23f5bc6f3728f45091dea0c",
  "expect": {"entry": "0x910", "relocations_written": 184284, "imports": 828},
  "gnm_region": null,
  "import_suffix_map": [],
  "import_aliases": []
}
```

## Tests

`python3 -m unittest discover -s tests` runs `tests/test_title_config.py`. It loads
the shipped config and checks valid and invalid synthetic configs.
