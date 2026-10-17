"""Check audit classification around literals, comments and meaningful tokens."""
import unittest

from audit_sources import compare, normalized, strip_comments


class SourceAuditTests(unittest.TestCase):
    def test_provenance_comments_and_whitespace_do_not_count_as_code_changes(self):
        self.assertEqual(normalized("// OFFSET: 0x123\nint x = 3;"),
                         normalized("/* OFFSET: 0x456 */ int x=3;"))

    def test_comment_markers_inside_literals_are_preserved(self):
        text = 'const char* url = "https://example.test/a/*b*/"; // note\n'
        self.assertIn('"https://example.test/a/*b*/"', strip_comments(text))
        self.assertNotIn("// note", strip_comments(text))
        self.assertNotEqual(normalized(text), normalized(text.replace("a/*b*/", "c")))

    def test_raw_strings_and_character_literals_are_preserved(self):
        text = 'auto s = R"tag(//text /*literal*/)tag"; char c = \'/\'; /* note */'
        self.assertIn('R"tag(//text /*literal*/)tag"', strip_comments(text))
        self.assertIn("'/'", strip_comments(text))
        self.assertNotIn("/* note */", strip_comments(text))

    def test_adjacent_operators_and_changed_values_remain_distinct(self):
        self.assertNotEqual(normalized("a + +b"), normalized("a++b"))
        self.assertNotEqual(normalized("i = 60"), normalized("i = 59"))

    def test_comparison_keeps_added_removed_and_changed_files_separate(self):
        root = "Engine/Source/Os/"
        old = {root + "a.cpp": "// old\nint x=1;", root + "b.h": "int b;",
               root + "c.cpp": "int c=1;", root + "same.h": "int a;"}
        new = {root + "a.cpp": "// new\nint x = 1;", root + "d.h": "int d;",
               root + "c.cpp": "int c=2;", root + "same.h": "int a;"}
        counts = compare(old, new)[root]["counts"]
        self.assertEqual(counts, {"identical": 1, "comments_or_whitespace_only": 1,
                                  "changed_tokens": 1, "donor_only": 1, "baseline_only": 1})


if __name__ == "__main__":
    unittest.main()
