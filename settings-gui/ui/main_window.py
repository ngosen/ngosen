# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Main window assembling all configuration tabs with a modern layout.
"""

import os
from datetime import datetime

from core.dbus_handler import NgoSenDBusHandler
from i18n import _
from qtpy.QtCore import Qt, QTimer
from qtpy.QtGui import QIcon, QKeySequence
from qtpy.QtWidgets import (
    QApplication,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPushButton,
    QScrollArea,
    QShortcut,
    QStackedWidget,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from core import backup, settings_snapshot
from ui import brand, search

# Lazy loading pages on demand


class NgoSenSettingsWindow(QMainWindow):
    """Main entry window for the Ngó Sen settings."""

    def __init__(self, dbus_handler=None):
        super().__init__()
        self.setWindowTitle(_("Ngó Sen Settings"))

        self.dbus_handler = dbus_handler or NgoSenDBusHandler()
        self._pages = []
        self._page_rows = {}
        self._page_titles = {}
        self._reset_pending = False
        # Saves once typing pauses instead of on every key in a text field.
        self._save_timer = QTimer(self)
        self._save_timer.setSingleShot(True)
        self._save_timer.setInterval(400)
        self._save_timer.timeout.connect(self.save_pending)
        self._saved = settings_snapshot.take(self.dbus_handler)
        self._undo_to = None

        self._setup_ui()
        self._setup_window_size()
        self.update_reset_button_state()
        if self._saved is None:
            self._show_message(_("Cannot reach fcitx5, so changes will not be saved."), error=True)

    def update_reset_button_state(self):
        self.btn_reset.setEnabled(
            any(
                (hasattr(p, "is_modified_from_default") and p.is_modified_from_default())
                or (hasattr(p, "is_modified") and p.is_modified())
                for p in self._pages
            )
        )

    def _setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_v_layout = QVBoxLayout(central_widget)
        main_v_layout.setContentsMargins(0, 0, 0, 0)
        main_v_layout.setSpacing(0)

        main_h_layout = QHBoxLayout()
        main_h_layout.setContentsMargins(0, 0, 0, 0)
        main_h_layout.setSpacing(0)

        self.sidebar = QListWidget()
        self.sidebar.setObjectName("Sidebar")
        self.sidebar.setFrameShape(QFrame.NoFrame)
        self.sidebar.setSpacing(3)

        self.content_stack = QStackedWidget()

        page_layout = QVBoxLayout()
        page_layout.setContentsMargins(0, 0, 0, 0)
        page_layout.addWidget(self._setup_message_bar())
        page_layout.addWidget(self.content_stack, 1)

        main_h_layout.addWidget(self._setup_navigation())
        main_h_layout.addLayout(page_layout, 1)

        main_v_layout.addLayout(main_h_layout, 1)

        # Bottom Bar
        self._setup_bottom_bar(main_v_layout)

        # Pages Mapping
        self._setup_pages()

        self.sidebar.currentRowChanged.connect(self._on_sidebar_changed)
        self.sidebar.setCurrentRow(0)

    def _setup_navigation(self):
        column = QWidget()
        column.setFixedWidth(200)
        layout = QVBoxLayout(column)
        layout.setContentsMargins(10, 12, 0, 0)
        layout.setSpacing(4)

        header = QHBoxLayout()
        header.setSpacing(8)
        icon = brand.logo_icon()
        if not icon.isNull():
            logo = QLabel()
            logo.setPixmap(icon.pixmap(28, 28))
            header.addWidget(logo)
        wordmark = QLabel("Ngó Sen")
        wordmark.setObjectName("Wordmark")
        wordmark.setFont(brand.wordmark_font(22))
        header.addWidget(wordmark)
        header.addStretch()
        layout.addLayout(header)
        layout.addSpacing(6)

        self.search_field = QLineEdit()
        self.search_field.setPlaceholderText(_("Search settings"))
        self.search_field.setClearButtonEnabled(True)
        self.search_field.textChanged.connect(self._on_search)
        self.search_field.returnPressed.connect(self._open_first_result)
        find = QShortcut(QKeySequence.Find, self)
        find.activated.connect(self.search_field.setFocus)
        find.activated.connect(self.search_field.selectAll)
        layout.addWidget(self.search_field)

        self.search_results = QListWidget()
        self.search_results.setFrameShape(QFrame.NoFrame)
        self.search_results.setWordWrap(True)
        self.search_results.itemActivated.connect(self.open_search_result)
        self.search_results.itemClicked.connect(self.open_search_result)
        self.search_results.hide()

        layout.addWidget(self.sidebar, 1)
        layout.addWidget(self.search_results, 1)
        return column

    def _on_search(self, text):
        searching = bool(text.strip())
        self.sidebar.setVisible(not searching)
        self.search_results.setVisible(searching)
        self.search_results.clear()
        if not searching:
            return
        self._build_all_pages()
        for label, target, page in search.find(self._pages, text):
            item = QListWidgetItem(f"{label} — {self._page_titles[page]}")
            item.setData(Qt.UserRole, (target, page))
            self.search_results.addItem(item)
        if not self.search_results.count():
            item = QListWidgetItem(_("No settings found"))
            item.setFlags(Qt.NoItemFlags)
            self.search_results.addItem(item)

    def _open_first_result(self):
        item = self.search_results.item(0)
        if item and item.data(Qt.UserRole):
            self.open_search_result(item)

    def open_search_result(self, item):
        """Shows the page and tab holding a found setting and puts the focus on it."""
        found = item.data(Qt.UserRole)
        if not found:
            return
        target, page = found
        self.search_field.clear()
        self.sidebar.setCurrentRow(self._page_rows[page])
        tabs = self.content_stack.currentWidget()
        if isinstance(tabs, QTabWidget):
            tabs.setCurrentWidget(page)
        parent = target.parentWidget()
        while parent and not isinstance(parent, QScrollArea):
            parent = parent.parentWidget()
        if parent:
            parent.ensureWidgetVisible(target)
        target.setFocus(Qt.OtherFocusReason)

    def _build_all_pages(self):
        # Pages are built when first opened; reset and search need all of them.
        for row in range(self.sidebar.count()):
            self._page_for_item(self.sidebar.item(row))

    def _setup_bottom_bar(self, layout):
        container = QFrame()
        container.setObjectName("BottomBar")
        bar_layout = QHBoxLayout(container)
        bar_layout.setContentsMargins(20, 12, 20, 12)
        bar_layout.setSpacing(10)

        bar_layout.addSpacing(180)

        self.btn_reset = QPushButton(QIcon.fromTheme("edit-undo"), _("&Defaults"))
        self.btn_reset.clicked.connect(self.on_restore_defaults)
        bar_layout.addWidget(self.btn_reset)

        bar_layout.addStretch()

        btn_backup = QPushButton(QIcon.fromTheme("document-save-as"), _("Back &Up…"))
        btn_backup.clicked.connect(self.on_export_backup)
        bar_layout.addWidget(btn_backup)
        btn_restore = QPushButton(QIcon.fromTheme("document-open"), _("Res&tore…"))
        btn_restore.clicked.connect(self.on_restore_backup)
        bar_layout.addWidget(btn_restore)
        self._bottom_buttons = [self.btn_reset, btn_backup, btn_restore]

        layout.addWidget(container)

    def _setup_message_bar(self):
        self.message_bar = QFrame()
        self.message_bar.setObjectName("SaveMessage")
        bar_layout = QHBoxLayout(self.message_bar)
        bar_layout.setContentsMargins(12, 6, 6, 6)
        self.message_label = QLabel()
        self.message_label.setWordWrap(True)
        bar_layout.addWidget(self.message_label, 1)
        self.btn_undo = QPushButton(_("&Undo"))
        self.btn_undo.clicked.connect(self.undo)
        bar_layout.addWidget(self.btn_undo)
        self.message_bar.hide()

        wrapper = QWidget()
        wrapper_layout = QVBoxLayout(wrapper)
        wrapper_layout.setContentsMargins(20, 12, 20, 0)
        wrapper_layout.addWidget(self.message_bar)
        return wrapper

    def _show_message(self, text, undo=False, error=False):
        if error:
            fill, border = brand.pick(self, brand.ERROR_FILL), brand.pick(self, brand.ERROR_BORDER)
        else:
            fill, border = brand.pick(self, brand.SAVED_FILL), brand.pick(self, brand.SAVED_BORDER)
        self.message_bar.setStyleSheet(
            f"QFrame#SaveMessage {{ background: {fill}; border: 1px solid {border};"
            " border-radius: 6px; }"
        )
        self.message_label.setText(text)
        self.btn_undo.setVisible(undo)
        self.message_bar.show()

    def _setup_pages(self):
        def create_typing():
            from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

            return DynamicSettingsPage(self.dbus_handler, category=SettingsCategory.TYPING)

        def create_applications():
            from ui.pages.mode_manager import ModeManagerPage

            return ModeManagerPage(self.dbus_handler)

        def create_macros():
            from ui.pages.macro_editor import MacroEditorPage

            return MacroEditorPage(self.dbus_handler)

        def create_dict():
            from ui.pages.dict_editor import DictEditorPage

            return DictEditorPage(self.dbus_handler)

        def create_keymap():
            from ui.pages.keymap_editor import KeymapEditorPage

            return KeymapEditorPage(self.dbus_handler)

        def create_shortcuts():
            from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

            return DynamicSettingsPage(self.dbus_handler, category=SettingsCategory.SHORTCUTS)

        def create_appearance():
            from ui.pages.dynamic_settings import DynamicSettingsPage, SettingsCategory

            return DynamicSettingsPage(self.dbus_handler, category=SettingsCategory.APPEARANCE)

        def create_about():
            from ui.pages.about import AboutPage

            return AboutPage()

        self._add_page(_("Typing"), "input-keyboard", (None, create_typing))
        self._add_page(_("Applications"), "applications-other", (None, create_applications))
        self._add_page(
            _("Macros & Dictionary"),
            "accessories-text-editor",
            (_("Macros"), create_macros),
            (_("Dictionary"), create_dict),
        )
        self._add_page(
            _("Keys"),
            "preferences-desktop-keyboard",
            (_("Shortcuts"), create_shortcuts),
            (_("Keymap"), create_keymap),
        )
        self._add_page(
            _("More"),
            # Adwaita and Yaru have no preferences-other.
            ("preferences-other", "preferences-system"),
            (_("Appearance"), create_appearance),
            (_("About"), create_about),
        )

    def on_restore_defaults(self):
        """Resets all settings to their default values."""
        from qtpy.QtWidgets import QMessageBox

        reply = QMessageBox.question(
            self,
            _("Confirm Reset"),
            _("Restore all settings to defaults?"),
            QMessageBox.Yes | QMessageBox.No,
        )
        if reply == QMessageBox.Yes:
            self._build_all_pages()
            for page in self._pages:
                if hasattr(page, "restore_defaults"):
                    page.restore_defaults()
            self._reset_pending = True
            self.on_changed()

    def _modified_pages(self):
        return [p for p in self._pages if hasattr(p, "is_modified") and p.is_modified()]

    def on_changed(self):
        """Saves the change once the user pauses, unless a setting is invalid."""
        self.update_reset_button_state()
        if self.has_validation_errors():
            self._save_timer.stop()
            self._show_message(self.validation_message(), error=True)
            return
        if self._modified_pages():
            self._save_timer.start()

    def has_validation_errors(self):
        return any(
            hasattr(p, "has_validation_errors") and p.has_validation_errors() for p in self._pages
        )

    def validation_message(self):
        messages = []
        for page in self._pages:
            if hasattr(page, "validation_message"):
                message = page.validation_message()
                if message:
                    messages.append(message)
        return "\n".join(messages)

    def save_pending(self):
        """Saves every changed page now; one saved message covers them all."""
        self._save_timer.stop()
        pages = self._modified_pages()
        if not pages or self.has_validation_errors():
            return
        label = None
        if len(pages) == 1:
            page = pages[0]
            label = page.change_label() if hasattr(page, "change_label") else None
            label = label or self._page_titles.get(page)
        for page in pages:
            if page.save_data() is False:
                # Show what fcitx5 kept, so the window does not suggest an unsaved change took.
                if settings_snapshot.take(self.dbus_handler) is not None:
                    self.reload_pages()
                self._show_message(_("Could not save. Check that fcitx5 is running."), error=True)
                return
        before, self._saved = self._saved, settings_snapshot.take(self.dbus_handler)
        self._undo_to = before
        if self._reset_pending:
            text = _("Defaults restored.")
        elif label:
            text = _("Saved “{}”.").format(label)
        else:
            text = _("Saved.")
        self._reset_pending = False
        self._show_message(text, undo=before is not None)
        self.update_reset_button_state()

    def undo(self):
        """Puts back the settings from before the last save."""
        self._save_timer.stop()
        if self._undo_to is None:
            return
        if not settings_snapshot.restore(self.dbus_handler, self._undo_to):
            self._show_message(_("Could not undo. Check that fcitx5 is running."), error=True)
            return
        self._undo_to = None
        self.reload_pages()
        self._show_message(_("Change undone."))

    def reload_pages(self):
        """Shows the saved settings again after they changed outside the pages."""
        for page in self._pages:
            if hasattr(page, "load_data"):
                page.load_data()
            elif hasattr(page, "load_config"):
                page.load_config()
        self._saved = settings_snapshot.take(self.dbus_handler)
        self.update_reset_button_state()

    def on_export_backup(self):
        name = f"ngosen-backup-{datetime.now():%Y%m%d-%H%M%S}.json"
        path, _filter = QFileDialog.getSaveFileName(
            self,
            _("Export Backup"),
            os.path.join(os.path.expanduser("~"), name),
            _("JSON Backup (*.json);;All Files (*)"),
        )
        if path:
            self.export_backup(path)

    def export_backup(self, path):
        self.save_pending()
        try:
            backup.export(self.dbus_handler, path)
        except (OSError, ValueError) as e:
            self._show_message(_("Failed to export backup:\n") + str(e), error=True)
            return
        self._show_message(_("Backed up to {}.").format(path))

    def on_restore_backup(self):
        path, _filter = QFileDialog.getOpenFileName(
            self,
            _("Select Backup File"),
            os.path.expanduser("~"),
            _("JSON Backup (*.json);;All Files (*)"),
        )
        if path:
            self.restore_backup(path)

    def restore_backup(self, path):
        """Applies a backup at once; Undo puts back what was there before."""
        self.save_pending()
        try:
            data = backup.load(path)
        except (OSError, ValueError) as e:
            self._show_message(_("Failed to open backup file:\n") + str(e), error=True)
            return
        before = self._saved
        restored = backup.apply(self.dbus_handler, data)
        self.reload_pages()
        if not restored:
            self._show_message(_("Could not restore. Check that fcitx5 is running."), error=True)
            return
        self._undo_to = before
        self._show_message(
            _("Restored from {}.").format(os.path.basename(path)), undo=before is not None
        )

    def closeEvent(self, event):
        self.save_pending()
        super().closeEvent(event)

    def _on_sidebar_changed(self, index):
        item = self.sidebar.item(index)
        if not item:
            return

        role = item.data(Qt.UserRole)
        if role == "page":
            widget = self._page_for_item(item)
            if widget:
                self.content_stack.setCurrentWidget(widget)
            self.update_reset_button_state()
        elif role == "header":
            # Don't allow selecting headers, move to next item
            if index + 1 < self.sidebar.count():
                self.sidebar.setCurrentRow(index + 1)

    def _page_for_item(self, item):
        if item.data(Qt.UserRole) != "page":
            return None
        widget = item.data(Qt.UserRole + 2)
        if widget is None:
            parts = [(tab, factory()) for tab, factory in item.data(Qt.UserRole + 1)]
            if len(parts) == 1:
                widget = parts[0][1]
            else:
                widget = QTabWidget()
                widget.setDocumentMode(True)
                for tab, page in parts:
                    widget.addTab(page, tab)
            for tab, page in parts:
                if len(parts) > 1:
                    # The tab already names the page.
                    page.setProperty("inTab", True)
                    for title in page.findChildren(QLabel, "CategoryTitle"):
                        title.hide()
                self._pages.append(page)
                self._page_titles[page] = tab or item.text()
                self._page_rows[page] = self.sidebar.row(item)
            self.content_stack.addWidget(widget)
            item.setData(Qt.UserRole + 2, widget)
            self._keep_bottom_bar_last()
        return widget

    def _keep_bottom_bar_last(self):
        # A page built later joins the end of the Tab order, after the bottom bar.
        last = self.previousInFocusChain()
        if last in self._bottom_buttons:
            return
        for button in self._bottom_buttons:
            QWidget.setTabOrder(last, button)
            last = button

    def _setup_window_size(self):
        screen = QApplication.primaryScreen().availableGeometry()
        w = int(screen.width() * 0.45)
        h = int(screen.height() * 0.60)
        self.setMinimumSize(750, 500)
        self.resize(w, h)
        self.move((screen.width() - w) // 2, (screen.height() - h) // 2)

    def _add_page(self, title: str, icon_name, *parts):
        """Adds a sidebar entry; several (tab title, factory) parts show as tabs."""
        names = (icon_name,) if isinstance(icon_name, str) else icon_name
        icon = next((QIcon.fromTheme(n) for n in names if QIcon.hasThemeIcon(n)), QIcon())
        item = QListWidgetItem(icon, title)
        item.setData(Qt.UserRole, "page")
        item.setData(Qt.UserRole + 1, list(parts))
        item.setData(Qt.UserRole + 2, None)

        self.sidebar.addItem(item)
