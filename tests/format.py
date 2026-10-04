#!/usr/bin/env python3
"""Regression checks for the project's C formatting conventions."""

import os
from pathlib import Path
import runpy
import subprocess
import unittest


ROOT = Path(__file__).resolve().parent.parent
KNF = runpy.run_path(str(ROOT / "scripts/format"))["knf"]
CLANG_FORMAT = os.getenv("CLANG_FORMAT", "clang-format")


def format_code(source):
	result = subprocess.run(
		[CLANG_FORMAT, "--style=file:" + str(ROOT / ".clang-format")],
		input=source, text=True, capture_output=True, check=True,
	)
	return KNF(result.stdout)


class FormattingTests(unittest.TestCase):
	def check_stable(self, source):
		formatted = format_code(source)
		self.assertEqual(formatted, format_code(formatted))
		for line in formatted.splitlines():
			self.assertLessEqual(len(line.expandtabs(8)), 80)
		return formatted

	def test_header_and_prototypes(self):
		formatted = self.check_stable("""#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <stddef.h>

int config_directory(char *, size_t);
void close_directory(void);
struct directory *find_directory(const char *);
#endif
""")
		self.assertIn("#ifndef EXAMPLE_H\n#define EXAMPLE_H\n", formatted)
		self.assertIn("\n#endif\n", formatted)
		columns = []
		for line in formatted.splitlines():
			if "(" in line:
				prefix = line.expandtabs(8).split("(")[0]
				columns.append(len(prefix) - len(prefix.split()[-1].lstrip("*")))
		self.assertEqual(len(set(columns)), 1)

	def test_indentation_and_returns(self):
		formatted = self.check_stable("""int f(int value)
{
    if (value < 0) {
        warnx("invalid");

        return 0;
    }
    if (value == 0) {
        release();
        value = 1;
        return value;
    }
    value++;
    return value;
}
int only(void) { return 1; }
""")
		self.assertIn('warnx("invalid");\n\t\treturn 0;', formatted)
		self.assertIn("value = 1;\n\t\treturn value;", formatted)
		self.assertIn("value++;\n\n\treturn value;", formatted)
		self.assertIn("only(void)\n{\n\treturn 1;\n}", formatted)
		for line in formatted.splitlines():
			self.assertNotIn(" ", line[:len(line) - len(line.lstrip())])

	def test_grouped_early_return(self):
		formatted = self.check_stable("""int f(int value)
{
    if (value < 0) {
        report();
        release();

        reset();
        value = 0;
        return value;
    }
    if (value == 0) {
        report();
        release();

        return 0;
    }
    return value;
}
""")
		self.assertIn("value = 0;\n\n\t\treturn value;", formatted)
		self.assertIn("release();\n\t\treturn 0;", formatted)

	def test_switch_return(self):
		formatted = self.check_stable("""int f(int value)
{
    switch (value) {
    case 1:
        value++;
        break;
    default:
        release();
        return 0;
    }
    return value;
}
""")
		self.assertIn("release();\n\t\treturn 0;", formatted)
		self.assertIn("\t}\n\n\treturn value;", formatted)

	def test_enum_values_on_separate_lines(self):
		formatted = self.check_stable(
			"enum { FIRST = 1, SECOND = 2 };\n"
			"enum named { ONLY = 3 };\n"
		)
		self.assertIn("enum {\n\tFIRST = 1,\n\tSECOND = 2\n};", formatted)
		self.assertIn("enum named {\n\tONLY = 3\n};", formatted)

	def test_multiline_comments(self):
		source = "/*\nAn example comment.\n*/\n"
		self.assertEqual(KNF(source), "/*\n * An example comment.\n */\n")

	def test_record_members(self):
		formatted = self.check_stable("""struct example {
    unsigned long width;
    const char *name;
    struct example *next;
    int height;
    void (*update)(struct example *, double);
    char label[32];
    /* unsigned long stays untouched */
    union {
        unsigned long number;
        const char *text;
    } value;
};
""")
		for kind in ("unsigned long", "const char", "struct example"):
			self.assertIn("\t" + kind + "\t", formatted)
		columns = []
		for member in ("width;", "*name;", "*next;", "height;",
			"(*update)", "label[32];"):
			line = next(line for line in formatted.splitlines() if member in line)
			columns.append(line.expandtabs(8).index(member))
		self.assertEqual(len(set(columns)), 1)
		self.assertIn("/* unsigned long stays untouched */", formatted)
		self.assertIn("\t\tunsigned long\t", formatted)
		self.assertIn("\t\tconst char\t", formatted)
		self.assertNotIn("unsigned\tlong", formatted)

	def test_record_comment_and_initializer(self):
		formatted = self.check_stable("""struct item {
    unsigned long id;
    const char *description; /* A description that is long enough to require wrapping. */
};
static struct item item = {1, "unsigned long text; struct fake { int member; };"};
""")
		self.assertIn('"unsigned long text; struct fake { int member; };"', formatted)


	def test_array_and_wrapped_prototype(self):
		formatted = self.check_stable("""static const char *names[] = {
    "first",
    "second",
};
static struct very_long_result_type *some_function_with_a_long_name(
    const char *, unsigned long, unsigned long, unsigned long, unsigned long);
""")
		self.assertIn('\n\t"first"', formatted)
		for line in formatted.splitlines():
			self.assertFalse(line.startswith(" "))


if __name__ == "__main__":
	unittest.main()
