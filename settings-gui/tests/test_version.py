# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import unittest

TEMPLATE = os.path.join(os.path.dirname(__file__), "..", "version.py.in")


class VersionTest(unittest.TestCase):
    def test_version_is_the_project_version_alone(self):
        # Pre-release builds from next use the same template, so it must not claim a channel.
        with open(TEMPLATE, encoding="utf-8") as f:
            source = f.read().replace("@PROJECT_VERSION@", "1.6.4")
        namespace = {}
        exec(compile(source, TEMPLATE, "exec"), namespace)  # noqa: S102
        self.assertEqual(namespace["__version__"], "1.6.4")


if __name__ == "__main__":
    unittest.main()
