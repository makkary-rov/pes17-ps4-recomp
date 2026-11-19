# SPDX-License-Identifier: GPL-2.0-or-later
import subprocess
import unittest
from unittest import mock

from paths import ROOT  # noqa: F401  (puts scripts/ on sys.path)
import audit_publish as ap

# Terms that the audit rejects are assembled at run time so this file has no R0 hit.
BLOOD = 'blood' + 'borne'
BAD_GPU = 'bb' + 'gpu'
BAD_PORT = 'bb' + 'port'
BAD_UNDERSCORE = 'BB' + '_FOO'
SL2 = 'DATA0000' + '.' + 'sl2'
EBOOT_LOWER = 'eboot.bin'
EBOOT_UPPER = 'EBOOT.BIN'


class NameTests(unittest.TestCase):
    def test_rejected_names(self):
        for rel in (EBOOT_LOWER, EBOOT_UPPER, 'out/boot-linked.bin', 'x.dmp', SL2,
                    'user/savedata/a.dat', 'src/title/generated/t.inc', 'gpu/shim/pes_gnm_table.inc'):
            with self.subTest(rel=rel):
                self.assertTrue(ap.check_name(rel), rel)

    def test_accepted_names(self):
        for rel in ('src/main.c', 'scripts/prepare.py', 'scripts/gen_gnm_table.py',
                    'scripts/gnm/callgraph.py', 'tests/gnm/test_callgraph.py',
                    'gpu/third_party/gcn/include/gcn/si_ci_vi_merged_offset.h'):
            with self.subTest(rel=rel):
                self.assertEqual(ap.check_name(rel), [], rel)

    def test_no_pending_entries(self):
        self.assertEqual(ap.PENDING, {})


class MessageTests(unittest.TestCase):
    def test_both_r0_patterns_apply_to_messages(self):
        self.assertTrue(ap.message_bad('uses ' + BAD_GPU + ' here'))
        self.assertTrue(ap.message_bad('sets ' + BAD_UNDERSCORE))
        self.assertFalse(ap.message_bad('Display: window opens at the chosen resolution'))


class EmailTests(unittest.TestCase):
    def test_personal_address_in_own_file_is_found(self):
        hits = ap.email_hits('gpu/patches/a.patch', 'From: someone <person' + '@mail.example.com>\n')
        self.assertEqual([h[0] for h in hits], [1])

    def test_project_identity_and_service_address_are_allowed(self):
        self.assertEqual(ap.email_hits('docs/x.md', 'noreply@example.invalid\n'), [])
        self.assertEqual(ap.email_hits('docs/x.md', 'Co-Authored-By: X <noreply@anthropic.com>\n'), [])

    def test_vendored_and_licence_files_are_exempt(self):
        text = 'Copyright 2012 Someone <person' + '@mail.example.com>\n'
        for rel in ('gpu/shadps4/common/sha1.h', 'gpu/third_party/x/y.h', 'LICENSES/upstream/a.md'):
            with self.subTest(rel=rel):
                self.assertEqual(ap.email_hits(rel, text), [])

    def test_identity_constant(self):
        self.assertEqual(ap.IDENTITY, 'noreply@example.invalid')


class SizeTests(unittest.TestCase):
    MIB2 = 2 << 20

    def test_large_text_passes(self):
        self.assertEqual(ap.check_size('x.h', b'a' * self.MIB2), [])

    def test_large_text_with_nul_fails(self):
        self.assertTrue(ap.check_size('x.h', b'\0' + b'a' * self.MIB2))

    def test_large_bin_fails(self):
        self.assertTrue(ap.check_size('x.bin', b'a' * self.MIB2))


class R0Tests(unittest.TestCase):
    def test_hits(self):
        self.assertTrue(ap.r0_hits('src/x.c', 'line ' + BLOOD + '\n', set()))
        self.assertTrue(ap.r0_hits('src/x.c', BAD_UNDERSCORE + '\n', set()))
        self.assertTrue(ap.r0_hits('src/x.c', BAD_PORT + ':\n', set()))

    def test_no_hits(self):
        self.assertEqual(ap.r0_hits('src/x.c', 'SUBB_U32\nUBB_U32\nhunt\n', set()), [])

    def test_allowlisted_line_suppressed(self):
        line = 'line ' + BLOOD
        self.assertEqual(ap.r0_hits('src/x.c', line + '\n', {('src/x.c', line)}), [])

    def test_licence_paths_skipped(self):
        self.assertEqual(ap.r0_hits('LICENSES/x.txt', BLOOD + '\n', set()), [])


class AllowlistTests(unittest.TestCase):
    def test_parse(self):
        md = '# R0\n```allow\nsrc/a.c\tfoo bar\n```\nsrc/b.c\tignored\n'
        self.assertEqual(ap.parse_allowlist(md), {('src/a.c', 'foo bar')})

    def test_empty_block(self):
        self.assertEqual(ap.parse_allowlist('```allow\n```\n'), set())


class PatternTests(unittest.TestCase):
    def test_source_patterns_match_plan(self):
        self.assertTrue(ap.P.search(BAD_GPU))
        self.assertTrue(ap.P.search(SL2))
        self.assertTrue(ap.Q.search(BAD_UNDERSCORE))
        self.assertFalse(ap.Q.search('SUBB_U32'))


class GitStdinTests(unittest.TestCase):
    # Regression: with no usable console stdin, Windows Python raises WinError 6 unless stdin is given.
    def test_git_helpers_pass_devnull_stdin(self):
        calls = []

        class Done:
            stdout = b''

        def fake_run(args, **kwargs):
            calls.append((args, kwargs))
            return Done()

        with mock.patch.object(ap.subprocess, 'run', side_effect=fake_run):
            ap.git_files()
            ap.commit_messages()
        self.assertEqual(len(calls), 3)
        for args, kwargs in calls:
            with self.subTest(args=args):
                self.assertIs(kwargs.get('stdin'), subprocess.DEVNULL)


if __name__ == '__main__':
    unittest.main()
