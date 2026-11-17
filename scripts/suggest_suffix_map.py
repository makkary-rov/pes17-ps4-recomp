# SPDX-License-Identifier: GPL-2.0-or-later
"""Suggest title-config alias rows from one verify run (stdlib only).

Input: analysis.json (import_identities {name: [nid, library, module]}) and the import
report TSV (columns: index name kind looked_up_as result symbol prefix_candidate).
Output: JSON with import_suffix_map rows, import_aliases rows and a conflicts report.

Rules (per PES import suffix '#X#Y' that has prefix_only imports, i.e. result trap and a
prefix_candidate):
- accepted as a suffix-map row only if every import with that suffix has a decoded,
  non-null library and module that agree, the prefix_only imports agree on one candidate
  suffix, and no import with that suffix already resolves by name (a suffix row would
  rewrite its lookup too);
- otherwise the suffix is a conflict. Each prefix_only import of a conflicting suffix gets
  a per-import alias row (its own single candidate) only if the candidate suffix is valid.

The output holds names, suffixes and counts only. No game bytes.
"""
import argparse
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

from title_config import SUFFIX_RE


def suffix_of(name):
    """'#X#Y' tail of a three-part import name, else None (same split as prepare.py)."""
    parts = name.split('#')
    if len(parts) != 3:
        return None
    return '#' + parts[1] + '#' + parts[2]


def is_prefix_only(row):
    return row['result'] == 'trap' and row['prefix_candidate'] not in ('', '-')


def load_identities(analysis_path):
    data = json.loads(Path(analysis_path).read_text(encoding='utf-8'))
    return data.get('import_identities', {})


def load_rows(tsv_path):
    with open(tsv_path, encoding='utf-8', newline='') as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def suggest(rows, identities):
    """Return (import_suffix_map, import_aliases, conflicts, summary) for the TSV rows."""
    groups = defaultdict(list)
    for row in rows:
        suffix = suffix_of(row['name'])
        if suffix is not None:
            groups[suffix].append(row)

    suffix_rows, alias_rows, conflicts = [], [], []
    covered = 0
    for suffix in sorted(groups):
        members = groups[suffix]
        prefix_only = [row for row in members if is_prefix_only(row)]
        if not prefix_only:
            continue
        candidates = sorted({suffix_of(row['prefix_candidate']) or row['prefix_candidate']
                             for row in prefix_only})
        identity = {row['name']: identities.get(row['name'], [None, None, None]) for row in members}
        libraries = {(identity[row['name']][1], identity[row['name']][2]) for row in members}
        resolved = sorted(row['name'] for row in members if row['result'] != 'trap')
        reasons = []
        if any(None in pair for pair in libraries):
            reasons.append('library or module not decoded for some import with this suffix')
        elif len(libraries) > 1:
            reasons.append('decoded library/module differs within this suffix')
        if len(candidates) != 1:
            reasons.append('prefix candidates disagree: ' + ', '.join(candidates))
        if resolved:
            reasons.append('imports already resolve by name: %d' % len(resolved))

        valid = len(candidates) == 1 and SUFFIX_RE.match(candidates[0]) is not None
        if not reasons and valid:
            library, module = next(iter(libraries))
            suffix_rows.append({
                'suffix': suffix,
                'library': library,
                'module': module,
                'canonical': candidates[0],
                'evidence': 'same-hash rows: %d' % len(prefix_only),
            })
            covered += len(prefix_only)
            continue

        conflicts.append({
            'suffix': suffix,
            'reasons': reasons,
            'imports': [{'name': row['name'], 'result': row['result'],
                         'prefix_candidate': row['prefix_candidate'],
                         'library': identity[row['name']][1], 'module': identity[row['name']][2]}
                        for row in sorted(members, key=lambda r: int(r['index']))],
        })
        for row in sorted(prefix_only, key=lambda r: int(r['index'])):
            candidate_suffix = suffix_of(row['prefix_candidate'])
            if candidate_suffix is None or SUFFIX_RE.match(candidate_suffix) is None:
                continue
            nid = row['name'].split('#')[0]
            alias_rows.append({
                'import': row['name'],
                'canonical': nid + candidate_suffix,
                'reason': 'suffix %s conflict: %s' % (suffix, '; '.join(reasons)),
                'evidence': 'same-hash rows: 1',
            })
            covered += 1

    total = sum(1 for row in rows if is_prefix_only(row))
    summary = {
        'prefix_only_total': total,
        'suffix_rows': len(suffix_rows),
        'alias_rows': len(alias_rows),
        'conflict_suffixes': len(conflicts),
        'prefix_only_covered': covered,
        'prefix_only_uncovered': total - covered,
    }
    return suffix_rows, alias_rows, conflicts, summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--analysis', required=True, help='analysis.json from prepare.py')
    parser.add_argument('--imports', required=True, help='import report TSV from the loader')
    parser.add_argument('--out', help='write JSON here (default: stdout)')
    args = parser.parse_args(argv)
    suffix_rows, alias_rows, conflicts, summary = suggest(
        load_rows(args.imports), load_identities(args.analysis))
    text = json.dumps({'import_suffix_map': suffix_rows, 'import_aliases': alias_rows,
                       'conflicts': conflicts, 'summary': summary}, indent=2) + '\n'
    if args.out:
        Path(args.out).write_text(text, encoding='utf-8', newline='\n')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
