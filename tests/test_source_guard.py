# SPDX-License-Identifier: GPL-2.0-or-later
"""Source guards: keep retired patterns from coming back into src/ (no build or binary needed)."""
from paths import ROOT
import re
import unittest

# A strncmp call whose third (length) argument is the literal 11, with an optional C integer suffix.
LITERAL_11 = re.compile(r'11[uUlL]*')


def call_arguments(text, open_paren):
    """Top-level arguments of the call whose '(' is at text[open_paren]; None if it never closes."""
    args, depth, start, i = [], 0, open_paren + 1, open_paren
    while i < len(text):
        c = text[i]
        if c in '"\'':
            end = i + 1
            while end < len(text) and text[end] != c:
                end += 2 if text[end] == '\\' else 1
            i = end + 1
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(text[start:i])
                return args
        elif c == ',' and depth == 1:
            args.append(text[start:i])
            start = i + 1
        i += 1
    return None


def strncmp_literal_11_lines(text):
    """1-based line numbers of every strncmp call in text whose length argument is the literal 11."""
    lines = []
    for m in re.finditer(r'\bstrncmp\s*\(', text):
        args = call_arguments(text, m.end() - 1)
        if args and len(args) == 3 and LITERAL_11.fullmatch(args[2].strip()):
            lines.append(text.count('\n', 0, m.start()) + 1)
    return lines


class SourceGuardTests(unittest.TestCase):
    def test_scanner_recognises_the_retired_nid_prefix_compare(self):
        # The C5/T3 rule: a NID is compared by strncmp over 11 characters. The scanner must see it.
        self.assertEqual(strncmp_literal_11_lines('if (candidate[11] != \'#\' || strncmp(candidate,name,11)) continue;'), [1])
        self.assertEqual(strncmp_literal_11_lines('x = strncmp(\n    a, f(b, c), 11u);'), [1])
        self.assertEqual(strncmp_literal_11_lines('strncmp(a,b,n); strncmp(a,b,111); strncmp(a,b,12);'), [])
        self.assertEqual(strncmp_literal_11_lines('memcmp(a,b,11); strcmp(a,b);'), [])

    def test_no_strncmp_call_with_a_literal_11_length_in_src(self):
        files = sorted([*(ROOT / 'src').glob('*.c'), *(ROOT / 'src').glob('*.h')])
        self.assertTrue(files, 'no src/*.c or src/*.h files found')
        hits = []
        for path in files:
            text = path.read_text(encoding='utf-8', errors='replace')
            hits += [f'{path.relative_to(ROOT).as_posix()}:{line}' for line in strncmp_literal_11_lines(text)]
        self.assertEqual(hits, [], 'strncmp with length 11 (the old NID-prefix rule) found in src')


if __name__ == '__main__':
    unittest.main()
