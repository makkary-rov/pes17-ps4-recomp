# SPDX-License-Identifier: GPL-2.0-or-later
"""PT_SCE_RELRO load segments are mapped writable; other segments keep their header flags."""
from paths import ROOT  # noqa: F401  (puts scripts/ on sys.path)
import unittest

import link_modules
import prepare


class SegmentFlagTests(unittest.TestCase):
    def test_relro_gets_the_write_bit(self):
        self.assertEqual(prepare.segment_flags(dict(type=0x61000010, flags=4)), 6)   # R -> RW
        self.assertEqual(prepare.segment_flags(dict(type=0x61000010, flags=6)), 6)   # already RW

    def test_other_segments_keep_their_flags(self):
        self.assertEqual(prepare.segment_flags(dict(type=1, flags=5)), 5)            # text: R+X stays
        self.assertEqual(prepare.segment_flags(dict(type=1, flags=4)), 4)            # read-only data stays
        self.assertEqual(prepare.segment_flags(dict(type=1, flags=6)), 6)

    def test_linker_uses_the_same_rule(self):
        self.assertIs(link_modules.segment_flags, prepare.segment_flags)


if __name__ == '__main__':
    unittest.main()
