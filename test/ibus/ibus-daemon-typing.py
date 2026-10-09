# Types into the engine through a real ibus-daemon, as an IBus client that keeps the text a simple
# app would show. Run by run-with-daemon.sh.
import os
import sys
import time

import gi

gi.require_version("IBus", "1.0")
from gi.repository import GLib, IBus  # noqa: E402

failures = 0


def pump(ms):
    context = GLib.MainContext.default()
    end = time.monotonic() + ms / 1000
    while time.monotonic() < end:
        context.iteration(False)
        time.sleep(0.001)


def connect():
    IBus.init()
    end = time.monotonic() + 10
    while time.monotonic() < end:
        bus = IBus.Bus()
        if bus.is_connected():
            return bus
        time.sleep(0.1)
    sys.exit("cannot connect to ibus-daemon")


bus = connect()


class App:
    def __init__(self, name, surrounding):
        self.text = ""
        self.commits = []
        self.forwarded = []
        self.surrounding = surrounding
        self.ic = bus.create_input_context(name)
        caps = IBus.Capabilite.FOCUS | IBus.Capabilite.PREEDIT_TEXT
        if surrounding:
            caps |= IBus.Capabilite.SURROUNDING_TEXT
        self.ic.set_capabilities(caps)
        self.ic.connect("commit-text", self.on_commit)
        self.ic.connect("forward-key-event", self.on_forward)
        self.ic.connect("delete-surrounding-text", self.on_delete)
        self.ic.focus_in()
        # An input context of a plain client cannot pick its own engine.
        bus.set_global_engine("ngosen")
        pump(500)
        engine = self.ic.get_engine()
        if engine is None or engine.get_name() != "ngosen":
            sys.exit("the engine did not start")
        self.report()

    def report(self):
        if self.surrounding:
            n = len(self.text)
            self.ic.set_surrounding_text(IBus.Text.new_from_string(self.text), n, n)

    def on_commit(self, _ic, text):
        self.commits.append(text.get_text())
        self.text += text.get_text()
        self.report()

    def on_delete(self, _ic, offset, n):
        end = len(self.text) + offset
        self.text = self.text[:end] + self.text[end + n :]
        self.report()

    def on_forward(self, _ic, keyval, _keycode, state):
        release = bool(state & IBus.ModifierType.RELEASE_MASK)
        self.forwarded.append((IBus.keyval_name(keyval), release))
        if release:
            return
        if keyval == IBus.KEY_BackSpace:
            self.text = self.text[:-1]
        else:
            self.text += chr(IBus.keyval_to_unicode(keyval))
        self.report()

    def type(self, keys):
        for c in keys:
            keyval = IBus.unicode_to_keyval(c)
            if not self.ic.process_key_event(keyval, 0, 0):
                self.text += c
                self.report()
            self.ic.process_key_event(keyval, 0, IBus.ModifierType.RELEASE_MASK)
            pump(40)
        pump(300)

    def close(self):
        self.ic.focus_out()
        pump(100)


def check(ok, what, got):
    global failures
    if not ok:
        failures += 1
        print(f"FAIL {what}: {got}")


def write_mode(mode):
    path = os.path.join(os.environ["XDG_CONFIG_HOME"], "fcitx5/conf/lotus.conf")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(f"Mode={mode}\n")
    # The engine rereads the file when its modification time changes.
    time.sleep(0.01)


write_mode("Sen")
app = App("test-surrounding", True)
app.type("tiee")
check(app.text == "tiê", "sen with surrounding text: field", repr(app.text))
check(app.commits == ["ê"], "sen with surrounding text: commits", app.commits)
check(
    app.forwarded == [("BackSpace", False), ("BackSpace", True)],
    "sen with surrounding text: forwarded",
    app.forwarded,
)
app.close()

app = App("test-plain", False)
app.type("tiee")
check(app.text == "tiê", "sen without surrounding text: field", repr(app.text))
app.close()

write_mode("Preedit")
app = App("test-preedit", True)
app.type("vieetj ")
check(app.commits == ["việt "], "preedit: commits", app.commits)
app.close()

if failures == 0:
    print("all IBus daemon checks passed")
sys.exit(1 if failures else 0)
