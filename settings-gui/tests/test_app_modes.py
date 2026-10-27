# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from unittest import mock

# isort: off
from fake_fcitx import FakeDBusHandler
from core.app_modes import RememberedAppModes

# isort: on

FIREFOX = {"App": "firefox", "Mode": "5"}


class RememberedAppModesTest(unittest.TestCase):
    def test_defaults_forget_every_app(self):
        fake = FakeDBusHandler(sub_configs={"app_rules": [FIREFOX]})
        modes = RememberedAppModes(fake)
        self.assertTrue(modes.is_modified_from_default())
        modes.restore_defaults()
        self.assertTrue(modes.is_modified())
        self.assertTrue(modes.save_data())
        self.assertEqual(fake.sub_configs["app_rules"], [])
        self.assertFalse(modes.is_modified())

    def test_a_refused_save_stays_unsaved(self):
        fake = FakeDBusHandler(sub_configs={"app_rules": [FIREFOX]})
        modes = RememberedAppModes(fake)
        modes.restore_defaults()
        with mock.patch.object(fake, "set_sub_config_list", return_value=False):
            self.assertFalse(modes.save_data())
        self.assertTrue(modes.is_modified())


if __name__ == "__main__":
    unittest.main()
