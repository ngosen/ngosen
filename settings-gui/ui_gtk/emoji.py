# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Emoji page: its settings, the apps that offer no emoji, and the words of every emoji."""

from core.catalog import EMOJI_GROUP, SettingsCategory
from core.emoji_table import GROUPS
from core.emoji_words import export_words, read_shared, toggle_hint
from gi.repository import Adw, Gdk, Gio, GLib, GObject, Graphene, Gsk, Gtk
from i18n import N_, _

from ui_gtk.pages import SettingsPage

ICON = "face-smile-symbolic"
# The short names of core.emoji_table.GROUPS, in the same order.
GROUP_LABELS = (
    N_("Smileys"),
    N_("People & gestures"),
    N_("Animals & plants"),
    N_("Food & drink"),
    N_("Travel & weather"),
    N_("Activities & events"),
    N_("Objects"),
    N_("Symbols"),
    N_("Flags"),
)
# The example's typed text, ending in a word that offers emoji.
EXAMPLE_TEXT = ("Hôm nay", "cười")
EXAMPLE_CANDIDATES = ("😂", "🤣", "😄", "😆", "😁")

CSS = """
.emoji-glyph { font-size: 30px; min-width: 44px; }
.emoji-word {
  border-radius: 999px; padding: 2px 10px; min-height: 26px; font-weight: normal;
  background: none; box-shadow: none; color: alpha(currentColor, 0.6);
  border: 1px dashed alpha(currentColor, 0.35);
}
.emoji-word > label { text-decoration-line: line-through; }
.emoji-word:hover { background: alpha(currentColor, 0.07); }
.emoji-word:checked {
  color: %(text)s; background: alpha(%(bg)s, 0.14); border: 1px solid %(text)s;
}
.emoji-word:checked:hover { background: alpha(%(bg)s, 0.22); }
.emoji-word:checked > label { text-decoration-line: none; }
button.emoji-new-word {
  border-radius: 999px; padding: 2px 10px; min-height: 26px; font-weight: normal;
  background: none; box-shadow: none; color: alpha(currentColor, 0.7);
  border: 1px dashed alpha(currentColor, 0.35);
}
button.emoji-new-word:hover { background: alpha(currentColor, 0.07); }
.emoji-reset { color: %(text)s; }
.emoji-table > row:not(:last-child) { border-bottom: 1px solid alpha(currentColor, 0.08); }
entry.emoji-new-word {
  border-radius: 999px; min-height: 26px; padding: 0 10px; background: none;
  border: 1px dashed alpha(currentColor, 0.35); box-shadow: none;
}
.emoji-filter:checked { background: %(bg)s; color: %(fg)s; }
.emoji-table { background: none; }
.emoji-table > row { padding: 8px 12px; }
.emoji-table-card {
  background: %(card)s; border-radius: 12px;
  box-shadow: 0 0 0 1px rgba(0, 0, 6, 0.07), 0 1px 3px 1px rgba(0, 0, 6, 0.07);
}
.emoji-example-field {
  background: %(view)s; border: 1px solid alpha(currentColor, 0.15); border-radius: 8px;
  padding: 8px 10px 10px;
}
.emoji-example-hint {
  background: %(popover)s; border: 1px solid alpha(currentColor, 0.18); border-radius: 8px;
  padding: 4px 8px; box-shadow: 0 2px 6px alpha(black, 0.12);
}
"""
# libadwaita 1.6 moved its colours to CSS variables; 1.5 still reads named colours.
COLORS = (
    {
        "text": "@accent_color",
        "bg": "@accent_bg_color",
        "fg": "@accent_fg_color",
        "card": "@card_bg_color",
        "view": "@view_bg_color",
        "popover": "@popover_bg_color",
    },
    {
        "text": "var(--accent-color)",
        "bg": "var(--accent-bg-color)",
        "fg": "var(--accent-fg-color)",
        "card": "var(--card-bg-color)",
        "view": "var(--view-bg-color)",
        "popover": "var(--popover-bg-color)",
    },
)
_css_installed = False


def _install_css():
    global _css_installed
    if _css_installed:
        return
    provider = Gtk.CssProvider()
    provider.load_from_string(CSS % COLORS[Adw.get_minor_version() >= 6])
    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )
    _css_installed = True


def _wrap_box(spacing):
    """Children that flow onto more lines; libadwaita 1.5 has no WrapBox, so a FlowBox there."""
    if hasattr(Adw, "WrapBox"):
        return Adw.WrapBox(child_spacing=spacing, line_spacing=spacing)
    return Gtk.FlowBox(
        selection_mode=Gtk.SelectionMode.NONE,
        column_spacing=spacing,
        row_spacing=spacing,
        max_children_per_line=40,
    )


def _wrap_append(box, child):
    box.append(child)
    if isinstance(box, Gtk.FlowBox):
        child.get_parent().set_focusable(False)


def _wrap_remove(box, child):
    box.remove(child.get_parent() if isinstance(box, Gtk.FlowBox) else child)


def _wrap_show(child, shown):
    parent = child.get_parent()
    (parent if isinstance(parent, Gtk.FlowBoxChild) else child).set_visible(shown)


def _wrap_insert_before(box, child, sibling, index):
    if isinstance(box, Gtk.FlowBox):
        box.insert(child, index)
        child.get_parent().set_focusable(False)
    else:
        box.insert_child_after(child, sibling.get_prev_sibling())


def _offer():
    """The candidate list as it pops up next to the cursor."""
    candidates = "  ".join(f"<b>{i}</b>\u00a0{e}" for i, e in enumerate(EXAMPLE_CANDIDATES, 1))
    numbers = f"1–{len(EXAMPLE_CANDIDATES)}: "
    offer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
    offer.append(_small(numbers + _("pick, or arrows then Enter, Esc: keep the word")))
    offer.append(
        Gtk.Label(label=candidates, use_markup=True, xalign=0, wrap=True, max_width_chars=16)
    )
    return offer


def _steps(spec):
    """Captioned fields showing what the user types, in order."""
    # Side by side, or one under another on a narrow window.
    steps = Gtk.FlowBox(
        homogeneous=True,
        max_children_per_line=3,
        selection_mode=Gtk.SelectionMode.NONE,
        column_spacing=12,
        row_spacing=12,
    )
    for side in ("top", "bottom", "start", "end"):
        getattr(steps, f"set_margin_{side}")(12)
    for caption, typed, hint in spec:
        field = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, vexpand=True)
        field.add_css_class("emoji-example-field")
        if isinstance(typed, Gtk.Widget):
            field.append(typed)
        else:
            field.append(
                Gtk.Label(
                    label=typed, xalign=0, wrap=True, max_width_chars=16, css_classes=["typed"]
                )
            )
        if hint is not None:
            bubble = Gtk.Box(halign=Gtk.Align.START, css_classes=["emoji-example-hint"])
            bubble.append(hint)
            field.append(bubble)
        step = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        step.append(_small(caption, "heading"))
        step.append(field)
        steps.append(step)
        step.get_parent().set_focusable(False)
    return steps


class EmojiPage(SettingsPage):
    """on_save(write, label) saves each word change with Undo; show_message(text, undo) a toast."""

    def __init__(self, model, on_change, words, on_save, show_message):
        self.words = words
        self.on_save = on_save
        self.show_message = show_message
        self._groups = []
        self._bound = {}
        self._drafts = {}
        self.editor = _NewWord(self)
        _install_css()
        super().__init__(model, on_change, SettingsCategory.EMOJI, _("Emoji"), ICON)
        header = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=24)
        header.set_margin_top(24)
        header.set_margin_bottom(12)
        older = self.older_note.get_ancestor(Adw.PreferencesGroup)
        for group in (older, *self._groups):
            self.page.remove(group)
            header.append(group)
        header.append(self._apps_group())
        header.append(self._words_group())
        self.count = Gtk.Label(xalign=0, css_classes=["dim-label"], margin_top=12)
        self.count.set_margin_bottom(24)
        self.list = self._table()
        self.column = _HeadedList(header, self.list, self.count)
        clamp = Adw.ClampScrollable(child=self.column, maximum_size=600, tightening_threshold=400)
        clamp.set_margin_start(12)
        clamp.set_margin_end(12)
        self.page = Gtk.ScrolledWindow(child=clamp, hscrollbar_policy=Gtk.PolicyType.NEVER)
        self._show_apps()
        self.refilter()

    def _add_extras(self, title, group):
        if title == EMOJI_GROUP:
            # The page title already says this one.
            group.set_title("")
            group.add(self._example())
        self._groups.append(group)

    def _apply_locks(self):
        super()._apply_locks()
        self.example.set_visible(self.value("EmojiSuggestions").lower() == "true")
        replaced = self.value("EmojiPlacement") == "Instead of the word"
        kept = EXAMPLE_TEXT[:1] if replaced else EXAMPLE_TEXT
        self.picked.set_text(" ".join((*kept, EXAMPLE_CANDIDATES[1])))
        sentence = self.value("EmojiOfferAt") == "At the end of a sentence"
        self.word_steps.set_visible(not sentence)
        self.sentence_steps.set_visible(sentence)
        english = self.value("EnglishEmojiKeywords").lower() == "true"
        if english != self.words.english:
            self.words.english = english
            if hasattr(self, "store"):
                self.refilter()

    def _example(self):
        """What typing does, as the steps of the timing the user picked."""
        self.picked = Gtk.Label(xalign=0, wrap=True, max_width_chars=16, css_classes=["typed"])
        typed = " ".join(EXAMPLE_TEXT)
        self.word_steps = _steps(
            (
                (_("1. Type a word"), typed, Gtk.Label(label=f"Tab: {EXAMPLE_CANDIDATES[0]} …")),
                (_("2. Press Tab"), typed, _offer()),
                (_("3. Press 2"), self.picked, None),
            )
        )
        sentence = typed + "."
        picked = Gtk.Label(
            label=f"{sentence} {EXAMPLE_CANDIDATES[1]}",
            xalign=0,
            wrap=True,
            max_width_chars=16,
            css_classes=["typed"],
        )
        self.sentence_steps = _steps(
            (
                (
                    _("1. End a sentence"),
                    sentence,
                    Gtk.Label(label=f"Tab: {EXAMPLE_CANDIDATES[0]} …"),
                ),
                (_("2. Press Tab"), sentence, _offer()),
                (_("3. Press 2"), picked, None),
            )
        )
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        box.append(self.word_steps)
        box.append(self.sentence_steps)
        self.example = Adw.PreferencesRow(activatable=False, focusable=False, child=box)
        self.example.searchable = False
        return self.example

    def _apps_group(self):
        group = Adw.PreferencesGroup(title=_("Apps with emoji"), description=self._toggle_hint())
        self.apps_group = group
        self.apps = Gtk.ListBox(selection_mode=Gtk.SelectionMode.NONE, css_classes=["boxed-list"])
        # App names are the user's own entries, not settings to find.
        self.apps.searchable = False
        group.add(self.apps)
        return group

    def _show_apps(self):
        self.apps.remove_all()
        for app in self.words.on_apps:
            row = Adw.ActionRow(title=app, use_markup=False)
            button = Gtk.Button(label=_("Turn off"), valign=Gtk.Align.CENTER)
            button.update_property(
                [Gtk.AccessibleProperty.LABEL], [_("Turn emoji off in {}").format(app)]
            )
            button.connect("clicked", lambda _b, app=app: self.disable_app(app))
            row.add_suffix(button)
            self.apps.append(row)
        if not self.words.on_apps:
            empty = Adw.ActionRow(title=_("Not on in any app."), css_classes=["dim-label"])
            empty.set_activatable(False)
            self.apps.append(empty)

    def disable_app(self, app):
        undo = self.words.disable_app(app)
        self._show_apps()
        if undo is None:
            self.show_message(_("Could not save. Check that fcitx5 is running."), None)
            return

        def undo_and_show():
            ok = undo()
            self._show_apps()
            return ok

        self.show_message(_("Emoji are off in {}.").format(app), undo_and_show)

    def _toggle_hint(self):
        return toggle_hint(self.value_of("ModeMenuKey"), self.value_of("ShortcutEmoji"))

    def refresh_status(self):
        """Shows the apps turned on from the keyboard while the window was open."""
        self.apps_group.set_description(self._toggle_hint())
        if not self.words.is_modified():
            self.words.load()
            self._show_apps()

    def _words_group(self):
        group = Adw.PreferencesGroup(
            title=_("Emoji and their words"),
            description=_(
                "Click a word to turn it off or on. Dashed words are off; type a new word in the"
                " box next to them to add it. A word on for several emoji offers all of them."
            ),
        )
        self.search = Gtk.SearchEntry(
            placeholder_text=_("Find an emoji or a word, e.g. tiền"), hexpand=True
        )
        self.search_handler = self.search.connect("search-changed", lambda _e: self.refilter())
        self.only_on = Gtk.ToggleButton(
            label=_("Only emoji with a word on"), css_classes=["emoji-filter"]
        )
        self.only_on.connect("toggled", lambda _b: self.refilter())
        top = Gtk.Box(spacing=8)
        top.append(self.search)
        top.append(self.only_on)
        kinds = _wrap_box(6)
        kinds.set_margin_top(8)
        self.group_buttons = []
        for index, label in enumerate((N_("All"), *GROUP_LABELS)):
            button = Gtk.ToggleButton(label=_(label), css_classes=["emoji-filter"])
            button.set_group(self.group_buttons[0] if self.group_buttons else None)
            button.set_active(index == 0)
            button.connect("toggled", lambda b: b.get_active() and self.refilter())
            _wrap_append(kinds, button)
            self.group_buttons.append(button)
        bar = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        bar.append(top)
        bar.append(kinds)
        group.add(bar)
        group.set_header_suffix(self._share_buttons())
        return group

    def _share_buttons(self):
        box = Gtk.Box(spacing=6, valign=Gtk.Align.CENTER)
        self.export_button = Gtk.Button(
            label=_("Export…"), tooltip_text=_("Save the words you turned on, to share them")
        )
        self.export_button.connect("clicked", lambda _b: self._choose_export())
        self.import_button = Gtk.Button(
            label=_("Import…"), tooltip_text=_("Add the words from a file someone shared")
        )
        self.import_button.connect("clicked", lambda _b: self._choose_import())
        box.append(self.export_button)
        box.append(self.import_button)
        return box

    def _choose_export(self):
        dialog = _json_dialog(_("Export Emoji Words"))
        dialog.set_initial_name("ngosen-emoji-words.json")
        dialog.save(self.page.get_root(), None, _on_file, dialog.save_finish, self.export_to)

    def _choose_import(self):
        dialog = _json_dialog(_("Import Emoji Words"))
        dialog.open(self.page.get_root(), None, _on_file, dialog.open_finish, self.import_from)

    def export_to(self, path):
        try:
            count = export_words(self.words, path)
        except OSError as e:
            self.show_message(_("Could not export: {}").format(e), None)
            return
        self.show_message(_("Saved {} words to {}.").format(count, path), None)

    def import_from(self, path):
        try:
            pairs = read_shared(path)
        except (OSError, ValueError) as e:
            self.show_message(_("Could not import: {}").format(e), None)
            return
        before = dict(self.words.edits)
        added = self.words.merge(pairs)
        if not added:
            self.show_message(_("You already have every word in that file."), None)
            return
        if not self.on_save(self.words.save, _("{} words from the file").format(added)):
            self.words.edits = before
        self.refilter()

    def _table(self):
        # Items are places in self.shown, not emoji: filtering then changes what the rows show
        # instead of making the list view replace every row.
        self.shown = []
        self.store = Gtk.StringList()
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self._setup_row)
        factory.connect("bind", lambda _f, item: item.get_child().bind(item.get_position()))
        factory.connect("unbind", lambda _f, item: item.get_child().release())
        view = Gtk.ListView(
            model=Gtk.NoSelection(model=self.store),
            factory=factory,
            show_separators=True,
            css_classes=["emoji-table", "emoji-table-card"],
        )
        # The user's own words are not settings to find.
        view.searchable = False
        return view

    def _setup_row(self, _factory, item):
        item.set_activatable(False)
        item.set_selectable(False)
        item.set_focusable(False)
        item.set_child(_EmojiRow(self))

    def group(self):
        """The picked group's name in GROUPS, or None for all."""
        index = next(i for i, b in enumerate(self.group_buttons) if b.get_active())
        return GROUPS[index - 1] if index else None

    def refilter(self):
        query, only = self.search.get_text(), self.only_on.get_active()
        self.shown = [row[0] for row in self.words.rows(query, self.group(), only)]
        kept = self.store.get_n_items()
        if kept > len(self.shown):
            self.store.splice(len(self.shown), kept - len(self.shown), [])
        else:
            self.store.splice(kept, 0, [str(i) for i in range(kept, len(self.shown))])
        for position, row in list(self._bound.items()):
            row.show(self.shown[position])
        self.column.show_list_top()
        shown = len(self.shown)
        self.count.set_text(_("{} emoji").format(shown) if shown else _("No emoji match"))

    def toggle(self, emoji, word):
        self._change(emoji, lambda: self.words.toggle(emoji, word), f"{emoji} {word}")

    def add(self, emoji, word):
        return self._change(emoji, lambda: self.words.add(emoji, word), f"{emoji} {word}")

    def reset(self, emoji):
        self._change(emoji, lambda: self.words.reset(emoji), emoji)

    def _change(self, emoji, change, label):
        """Applies and saves a change, or puts the words back as they were if fcitx5 refuses it."""
        before = dict(self.words.edits)
        if change() is False:
            return False
        ok = self.on_save(self.words.save, label)
        if not ok:
            self.words.edits = before
        for row in self._bound.values():
            if row.emoji == emoji:
                row.show(emoji)
        return ok


def _json_dialog(title):
    files = Gtk.FileFilter(name=_("Emoji words"))
    files.add_suffix("json")
    filters = Gio.ListStore.new(Gtk.FileFilter)
    filters.append(files)
    return Gtk.FileDialog(title=title, modal=True, filters=filters, default_filter=files)


def _on_file(_dialog, result, finish, use):
    try:
        file = finish(result)
    except GLib.Error:
        return  # Cancelled.
    if file is not None and file.get_path():
        use(file.get_path())


def _small(text, *css):
    return Gtk.Label(
        label=text, xalign=0, wrap=True, max_width_chars=16, css_classes=["caption", *css]
    )


class _EmojiRow(Gtk.Box):
    """One emoji and its words; the list view hands it a new emoji as rows scroll by."""

    def __init__(self, page):
        super().__init__(spacing=12)
        self.page = page
        self.emoji = None
        self.position = None
        self.chips = []
        self.glyph = Gtk.Label(valign=Gtk.Align.START, css_classes=["emoji-glyph"])
        self.words = _wrap_box(6)
        self.words.set_hexpand(True)
        self.words.set_valign(Gtk.Align.CENTER)
        # A button until clicked: an entry in each of the rows kept would double their cost.
        self.new = Gtk.Button(css_classes=["emoji-new-word"])
        self.new.connect("clicked", lambda _b: self.page.editor.open(self))
        _wrap_append(self.words, self.new)
        self.reset = Gtk.Button(
            label=_("As before"), valign=Gtk.Align.START, css_classes=["flat", "emoji-reset"]
        )
        self.reset.connect("clicked", lambda _b: self.page.reset(self.emoji))
        self.append(self.glyph)
        self.append(self.words)
        self.append(self.reset)

    def bind(self, position):
        self.position = position
        self.page._bound[position] = self
        self.show(self.page.shown[position])

    def show(self, emoji):
        if self.emoji != emoji:
            if self.page.editor.row is self:
                self.page.editor.close()
            self.emoji = emoji
            self.glyph.set_text(emoji)
            self.new.update_property(
                [Gtk.AccessibleProperty.LABEL], [_("Add a word for {}").format(emoji)]
            )
        self.new.set_label(self.page._drafts.get(emoji) or _("+ word"))
        words = self.page.words.chips(emoji)
        while len(self.chips) < len(words):
            self._new_chip()
        for chip, (word, on) in zip(self.chips, words):
            chip.word = word
            chip.set_label(word)
            chip.set_active(on)
            text = _("Turn off {word} for {emoji}") if on else _("Turn on {word} for {emoji}")
            chip.update_property(
                [Gtk.AccessibleProperty.LABEL], [text.format(word=word, emoji=emoji)]
            )
        for index, chip in enumerate(self.chips):
            _wrap_show(chip, index < len(words))
        self.reset.set_visible(self.page.words.changed(emoji))

    def _new_chip(self):
        """Chips are kept for the next emoji, so scrolling does not make new buttons."""
        chip = Gtk.ToggleButton(css_classes=["emoji-word"])
        chip.connect("clicked", lambda c: self.page.toggle(self.emoji, c.word))
        _wrap_insert_before(self.words, chip, self.new, len(self.chips))
        self.chips.append(chip)

    def hold(self, editor):
        _wrap_insert_before(self.words, editor, self.new, len(self.chips))
        _wrap_show(self.new, False)

    def drop(self, editor):
        _wrap_remove(self.words, editor)
        self.new.set_label(self.page._drafts.get(self.emoji) or _("+ word"))
        _wrap_show(self.new, True)

    def release(self):
        if self.page._bound.get(self.position) is self:
            del self.page._bound[self.position]
        self.position = None


class _NewWord(Gtk.Box):
    """The box to type a new word in; one for the page, moved into the row it is opened in."""

    def __init__(self, page):
        super().__init__(spacing=4)
        self.page = page
        self.row = None
        self.entry = Gtk.Entry(
            placeholder_text=_("+ word"), width_chars=8, css_classes=["emoji-new-word"]
        )
        self.entry.connect("changed", self._on_changed)
        self.entry.connect("activate", lambda _e: self.add())
        self.add_button = Gtk.Button(
            label=_("Add"), visible=False, css_classes=["suggested-action"]
        )
        self.add_button.connect("clicked", lambda _b: self.add())
        self.append(self.entry)
        self.append(self.add_button)
        focus = Gtk.EventControllerFocus()
        focus.connect("leave", lambda _c: GLib.idle_add(self._close_if_empty))
        self.add_controller(focus)

    def open(self, row):
        if self.row is not None:
            self.close()
        self.row = row
        row.hold(self)
        self.entry.set_text(self.page._drafts.get(row.emoji, ""))
        self.entry.update_property(
            [Gtk.AccessibleProperty.LABEL], [_("Add a word for {}").format(row.emoji)]
        )
        self.entry.grab_focus()

    def close(self):
        row, self.row = self.row, None
        if row is not None:
            row.drop(self)

    def _close_if_empty(self):
        if self.row is not None and not self.entry.get_text() and not self.contains_focus():
            self.close()
        return GLib.SOURCE_REMOVE

    def contains_focus(self):
        root = self.get_root()
        focus = root.get_focus() if root else None
        return focus is not None and (focus is self or focus.is_ancestor(self))

    def _on_changed(self, entry):
        text = entry.get_text()
        self.add_button.set_visible(bool(text.strip()))
        entry.remove_css_class("error")
        if self.row is not None:
            if text:
                self.page._drafts[self.row.emoji] = text
            else:
                self.page._drafts.pop(self.row.emoji, None)

    def add(self):
        if self.row is None or not self.entry.get_text().strip():
            return
        emoji = self.row.emoji
        if self.page.add(emoji, self.entry.get_text()):
            self.page._drafts.pop(emoji, None)
            self.entry.set_text("")
            self.entry.grab_focus()
        else:
            self.entry.add_css_class("error")


class _HeadedList(Gtk.Widget, Gtk.Scrollable):
    """A header, a list view and a footer that scroll as one page.

    A list view laid out inside an ordinary scrolled box makes a row for every item, so this
    gives the list only the part of the scroll that falls on it, and it keeps recycling rows.
    """

    def __init__(self, header, list_view, footer):
        self._vadj = None
        self._hadj = None
        self._allocating = False
        self._focus_handler = None
        self._header_height = 0
        self._list_value = 0
        super().__init__(overflow=Gtk.Overflow.HIDDEN)
        self.header, self.list, self.footer = header, list_view, footer
        for child in (header, list_view, footer):
            child.set_parent(self)
        self._inner = Gtk.Adjustment()
        list_view.set_vadjustment(self._inner)
        self._inner.connect("value-changed", self._on_list_scrolled)

    def _get_vadjustment(self):
        return self._vadj

    def _set_vadjustment(self, adjustment):
        if self._vadj is not None:
            self._vadj.disconnect(self._vadj_handler)
        self._vadj = adjustment or Gtk.Adjustment()
        self._vadj_handler = self._vadj.connect("value-changed", self._on_scrolled)
        self.queue_allocate()

    def _get_hadjustment(self):
        return self._hadj

    def _set_hadjustment(self, adjustment):
        self._hadj = adjustment

    vadjustment = GObject.Property(
        type=Gtk.Adjustment, getter=_get_vadjustment, setter=_set_vadjustment
    )
    hadjustment = GObject.Property(
        type=Gtk.Adjustment, getter=_get_hadjustment, setter=_set_hadjustment
    )
    hscroll_policy = GObject.Property(
        type=Gtk.ScrollablePolicy, default=Gtk.ScrollablePolicy.MINIMUM
    )
    vscroll_policy = GObject.Property(
        type=Gtk.ScrollablePolicy, default=Gtk.ScrollablePolicy.MINIMUM
    )

    def _on_scrolled(self, _adjustment):
        if not self._allocating:
            self.queue_allocate()

    def _on_list_scrolled(self, inner):
        # The list scrolled itself, such as to show a focused row: move the page as far.
        if not self._allocating and self._vadj is not None:
            delta = inner.get_value() - self._list_value
            if abs(delta) >= 1:
                self._vadj.set_value(self._vadj.get_value() + delta)

    def show_list_top(self):
        """Scrolls back to the first row if the page is scrolled past it."""
        if self._vadj is not None and self._vadj.get_value() > self._header_height:
            self._vadj.set_value(self._header_height)

    def do_measure(self, orientation, for_size):
        # Measuring the list would measure every row it holds; its rows wrap to any width.
        sizes = [c.measure(orientation, for_size) for c in (self.header, self.footer)]
        if orientation == Gtk.Orientation.HORIZONTAL:
            return max(s[0] for s in sizes), max(s[1] for s in sizes), -1, -1
        return 0, sizes[0][1] + sizes[1][1], -1, -1

    def do_size_allocate(self, width, height, _baseline):
        self._allocating = True
        header = self.header.measure(Gtk.Orientation.VERTICAL, width)[1]
        footer = self.footer.measure(Gtk.Orientation.VERTICAL, width)[1]
        content = self.list.measure(Gtk.Orientation.VERTICAL, width)[1]
        shown = min(height, content)
        total = header + content + footer
        value = min(max(self._vadj.get_value() if self._vadj else 0, 0), max(total - height, 0))
        list_value = min(max(value - header, 0), max(content - shown, 0))
        self._header_height, self._list_value = header, list_value
        self._inner.set_value(list_value)
        top = header - value + list_value
        self.header.allocate(width, header, -1, _at(0, -value))
        self.list.allocate(width, shown, -1, _at(0, top))
        self.footer.allocate(width, footer, -1, _at(0, top + shown))
        if abs(self._inner.get_value() - list_value) >= 1:
            self._inner.set_value(list_value)
        if self._vadj is not None:
            self._vadj.configure(value, 0, total, height * 0.1, height * 0.9, height)
        self._allocating = False

    def do_root(self):
        Gtk.Widget.do_root(self)
        self._focus_handler = self.get_root().connect("notify::focus-widget", self._on_focus)

    def do_unroot(self):
        self.get_root().disconnect(self._focus_handler)
        Gtk.Widget.do_unroot(self)

    def _on_focus(self, root, _param):
        focus = root.get_focus()
        if focus is not None and focus.is_ancestor(self):
            # After the list has moved to the focused row, if it did.
            GLib.idle_add(self._show_focus, focus)

    def _show_focus(self, focus):
        ok, bounds = focus.compute_bounds(self)
        if ok and self._vadj is not None:
            top, bottom = bounds.get_y(), bounds.get_y() + bounds.get_height()
            height = self.get_height()
            if top < 0:
                self._vadj.set_value(self._vadj.get_value() + top - 12)
            elif bottom > height:
                self._vadj.set_value(self._vadj.get_value() + min(bottom - height + 12, top))
        return GLib.SOURCE_REMOVE

    def do_dispose(self):
        for child in (self.header, self.list, self.footer):
            child.unparent()
        Gtk.Widget.do_dispose(self)


def _at(x, y):
    return Gsk.Transform().translate(Graphene.Point().init(x, y))
