# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import tempfile
import unittest
from unittest import mock

from support import FakeDBusHandler, app  # noqa: F401
from ui.pages.dict_editor import DictEditorPage


class DictionaryPageTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        bundled = os.path.join(tmp.name, "bundled.dict")
        with open(bundled, "w", encoding="utf-8") as f:
            f.write("an\nanh\nba\nbo ba\n")
        patcher = mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp.name})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.page = DictEditorPage(FakeDBusHandler())
        self.page._get_global_dict_path = lambda: bundled
        self.page.load_data()

    def shown(self):
        return self.page.shown_words()

    def test_search_filters_the_words(self):
        self.page.search_input.setText("AN")
        self.assertEqual(self.shown(), ["an", "anh"])
        self.page.search_input.clear()
        self.assertEqual(len(self.shown()), 4)

    def test_remove_takes_out_the_selected_words(self):
        self.page.search_input.setText("an")
        self.page.select_words(["anh"])
        self.page.on_remove()
        self.assertNotIn("anh", self.page.words)
        self.assertIn("an", self.page.words)
        self.assertTrue(self.page.is_modified())

    def test_add_shows_the_new_word(self):
        self.page.input_word.setText("xyz")
        self.page.on_add()
        self.assertIn("xyz", self.shown())

    def test_long_word_is_not_cut_off(self):
        self.page.words = ["an", "nghiêng"]
        self.page._show_words()
        self.page.resize(700, 500)
        self.page.show()
        app.processEvents()
        view = self.page.list_view
        rect = view.visualRect(self.page.model.index(1, 0))
        self.assertGreater(rect.width(), view.fontMetrics().horizontalAdvance("nghiêng"))

    def test_word_with_a_space_is_flagged(self):
        self.assertEqual(self.page.flagged_words(), ["bo ba"])


if __name__ == "__main__":
    unittest.main()
