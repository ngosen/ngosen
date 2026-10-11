# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Main window assembling all configuration tabs with a modern layout.
"""

from core.dbus_handler import NgoSenDBusHandler
from i18n import _
from qtpy.QtCore import QSize, Qt, QTimer
from qtpy.QtGui import QIcon, QPalette
from qtpy.QtWidgets import (
    QApplication,
    QFrame,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from core import settings_snapshot

# Lazy loading pages on demand


class NgoSenSettingsWindow(QMainWindow):
    """Main entry window for the Ngó Sen settings."""

    def __init__(self, dbus_handler=None):
        super().__init__()
        self.setWindowTitle(_("Ngó Sen Settings"))

        self.dbus_handler = dbus_handler or NgoSenDBusHandler()
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
        self._apply_global_styles()
        self.update_reset_button_state()
        if self._saved is None:
            self._show_message(_("Cannot reach fcitx5, so changes will not be saved."), error=True)

    def update_reset_button_state(self):
        any_modified_from_default = any(
            (
                hasattr(self.content_stack.widget(i), "is_modified_from_default")
                and self.content_stack.widget(i).is_modified_from_default()
            )
            or (
                hasattr(self.content_stack.widget(i), "is_modified")
                and self.content_stack.widget(i).is_modified()
            )
            for i in range(self.content_stack.count())
        )
        self.btn_reset.setEnabled(any_modified_from_default)

    def _apply_global_styles(self):
        self.setStyleSheet("""
            QLabel#CategoryTitle {
                font-size: 22px;
            }
            QLabel#AboutTitle {
                font-size: 26px;
            }
            QLabel#KeyCap {
                background-color: palette(button);
                color: palette(button-text);
                border: 1px solid palette(mid);
                border-bottom: 2px solid palette(dark);
                border-radius: 4px;
                padding: 2px 6px;
                font-weight: bold;
                font-family: monospace;
            }
            QLabel#ShortcutWarning {
                color: palette(link-visited);
                font-size: 12px;
            }
        """)

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
        self.sidebar.setFixedWidth(200)
        self.sidebar.setStyleSheet("""
            QListWidget {
                border: none;
                background: transparent;
                outline: none;
                padding-top: 15px;
            }
            QListWidget::item {
                padding: 10px 15px;
                border-radius: 8px;
                margin: 2px 10px;
            }
            QListWidget::item:selected {
                background: palette(highlight);
                color: palette(highlighted-text);
            }
            QListWidget::item:hover:!selected {
                background: palette(alternate-base);
            }
        """)
        self.sidebar.setObjectName("Sidebar")
        self.sidebar.setFrameShape(QFrame.NoFrame)

        self.content_stack = QStackedWidget()

        page_layout = QVBoxLayout()
        page_layout.setContentsMargins(0, 0, 0, 0)
        page_layout.addWidget(self._setup_message_bar())
        page_layout.addWidget(self.content_stack, 1)

        main_h_layout.addWidget(self.sidebar)
        main_h_layout.addLayout(page_layout, 1)

        main_v_layout.addLayout(main_h_layout, 1)

        # Bottom Bar
        self._setup_bottom_bar(main_v_layout)

        # Pages Mapping
        self._setup_pages()

        self.sidebar.currentRowChanged.connect(self._on_sidebar_changed)
        self.sidebar.setCurrentRow(0)

    def _setup_bottom_bar(self, layout):
        container = QFrame()
        container.setObjectName("BottomBar")
        bar_layout = QHBoxLayout(container)
        bar_layout.setContentsMargins(20, 12, 20, 12)
        bar_layout.setSpacing(10)

        bar_layout.addSpacing(180)

        self.btn_reset = QPushButton(QIcon.fromTheme("edit-undo"), _("&Reset"))
        self.btn_reset.clicked.connect(self.on_restore_defaults)
        bar_layout.addWidget(self.btn_reset)

        bar_layout.addStretch()

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
        dark = self.palette().color(QPalette.Window).lightness() < 128
        if error:
            fill, border = ("#3d1d18", "#d0533f") if dark else ("#f8e3df", "#c2301c")
        else:
            fill, border = ("#183a33", "#3f9c8b") if dark else ("#e1efe9", "#1c6b5f")
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

        def create_backup():
            from ui.pages.backup import BackupPage

            return BackupPage(self.dbus_handler)

        def create_about():
            from ui.pages.about import AboutPage

            return AboutPage()

        self._add_page(_("Typing"), "input-keyboard", create_typing)
        self._add_page(_("Applications"), "applications-other", create_applications)
        self._add_page(_("Macros"), "accessories-text-editor", create_macros)
        self._add_page(_("Dictionary"), "edit-copy", create_dict)
        self._add_page(_("Keymap"), "preferences-desktop-keyboard", create_keymap)
        self._add_page(_("Shortcuts"), "preferences-desktop-keyboard-shortcuts", create_shortcuts)
        self._add_page(_("Appearance"), "preferences-desktop-theme", create_appearance)
        self._add_page(_("Backup"), "document-save-as", create_backup)

        spacer = QListWidgetItem()
        spacer.setFlags(Qt.NoItemFlags)
        spacer.setSizeHint(QSize(0, 20))
        self.sidebar.addItem(spacer)
        self._add_page(_("About"), "help-about", create_about)

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
            # Pages are built when first opened; one never opened still has settings to reset.
            for row in range(self.sidebar.count()):
                self._page_for_item(self.sidebar.item(row))
            for i in range(self.content_stack.count()):
                page = self.content_stack.widget(i)
                if hasattr(page, "restore_defaults"):
                    page.restore_defaults()
            self._reset_pending = True
            self.on_changed()

    def _modified_pages(self):
        pages = (self.content_stack.widget(i) for i in range(self.content_stack.count()))
        return [p for p in pages if hasattr(p, "is_modified") and p.is_modified()]

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
            hasattr(self.content_stack.widget(i), "has_validation_errors")
            and self.content_stack.widget(i).has_validation_errors()
            for i in range(self.content_stack.count())
        )

    def validation_message(self):
        messages = []
        for i in range(self.content_stack.count()):
            page = self.content_stack.widget(i)
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
        for i in range(self.content_stack.count()):
            page = self.content_stack.widget(i)
            if hasattr(page, "load_data"):
                page.load_data()
            elif hasattr(page, "load_config"):
                page.load_config()
        self._saved = settings_snapshot.take(self.dbus_handler)
        self.update_reset_button_state()

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
            factory = item.data(Qt.UserRole + 1)
            if factory:
                widget = factory()
                self._page_titles[widget] = item.text()
                self.content_stack.addWidget(widget)
                item.setData(Qt.UserRole + 2, widget)
        return widget

    def _setup_window_size(self):
        screen = QApplication.primaryScreen().availableGeometry()
        w = int(screen.width() * 0.45)
        h = int(screen.height() * 0.60)
        self.setMinimumSize(750, 500)
        self.resize(w, h)
        self.move((screen.width() - w) // 2, (screen.height() - h) // 2)

    def _add_page(self, title: str, icon_name: str, page_factory):
        item = QListWidgetItem(QIcon.fromTheme(icon_name), title)
        item.setData(Qt.UserRole, "page")
        item.setData(Qt.UserRole + 1, page_factory)
        item.setData(Qt.UserRole + 2, None)

        self.sidebar.addItem(item)
