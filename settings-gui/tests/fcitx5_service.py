# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A fake fcitx5 controller on a private D-Bus session, for tests that go through dbus-python.

Run as a script, it serves the controller and appends each SetConfig to a JSON lines log.
"""

import importlib.util
import json
import os
import select
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GUI_DIR = os.path.dirname(HERE)
AVAILABLE = bool(shutil.which("dbus-daemon") and importlib.util.find_spec("gi"))
BUS_CONFIG = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir={}</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
"""


class PrivateFcitx5:
    """A session bus of its own, so a test never talks to the fcitx5 of whoever runs it."""

    def __init__(self, tmp_dir):
        self.log = os.path.join(tmp_dir, "set-config.jsonl")
        # No service directories: a GTK window would otherwise start the desktop's portals,
        # which outlive the test.
        config = os.path.join(tmp_dir, "bus.conf")
        with open(config, "w", encoding="utf-8") as f:
            f.write(BUS_CONFIG.format(tmp_dir))
        self.daemon = subprocess.Popen(
            ["dbus-daemon", "--config-file", config, "--nofork", "--print-address=1"],
            stdout=subprocess.PIPE,
            text=True,
        )
        address = self.daemon.stdout.readline().strip()
        self.env = dict(os.environ, DBUS_SESSION_BUS_ADDRESS=address)
        self.service = None

    def start(self, log=None):
        """Starts, or restarts, fcitx5; SetConfig calls go to `log`."""
        self.stop_service()
        self.service = subprocess.Popen(
            [sys.executable, __file__, log or self.log],
            env=self.env,
            stdout=subprocess.PIPE,
            text=True,
        )
        ready, _, _ = select.select([self.service.stdout], [], [], 10)
        if not ready or self.service.stdout.readline().strip() != "ready":
            raise RuntimeError("fake fcitx5 did not start")

    def set_config_calls(self, log=None):
        try:
            with open(log or self.log, encoding="utf-8") as f:
                return [json.loads(line) for line in f]
        except FileNotFoundError:
            return []

    def stop_service(self):
        if self.service:
            self.service.kill()
            self.service.communicate()
            self.service = None

    def close(self):
        self.stop_service()
        self.daemon.kill()
        self.daemon.communicate()


def _serve(log):
    import dbus
    import dbus.service
    from dbus.mainloop.glib import DBusGMainLoop
    from gi.repository import GLib

    with open(os.path.join(HERE, "config-metadata.json"), encoding="utf-8") as f:
        metadata = json.load(f)
    values = {item[0]: item[3] for group in metadata for item in group[1]}
    addon = "fcitx://config/addon/ngosen"

    def to_dbus(value, variant_level=0):
        if isinstance(value, dict):
            items = {k: to_dbus(v) for k, v in value.items()}
            return dbus.Dictionary(items, signature="sv", variant_level=variant_level)
        return dbus.String(value)

    def fcitx5_reads(value):
        if isinstance(value, dbus.Dictionary):
            return value.signature == "sv" and all(map(fcitx5_reads, value.values()))
        return isinstance(value, dbus.String)

    def plain(value):
        if isinstance(value, dict):
            return {str(k): plain(v) for k, v in value.items()}
        if isinstance(value, list):
            return [plain(v) for v in value]
        return str(value)

    groups = [
        (name, [(o[0], o[1], o[2], to_dbus(o[3]), to_dbus(o[4])) for o in options])
        for name, options in metadata
    ]

    class Controller(dbus.service.Object):
        @dbus.service.method(
            "org.fcitx.Fcitx.Controller1", in_signature="s", out_signature="va(sa(sssva{sv}))"
        )
        def GetConfig(self, uri):
            if uri == addon:
                return to_dbus(values, variant_level=1), groups
            return dbus.Dictionary({}, signature="sv", variant_level=1), []

        @dbus.service.method(
            "org.fcitx.Fcitx.Controller1", in_signature="sv", message_keyword="message"
        )
        def SetConfig(self, uri, new_values, message):
            # fcitx5 rejects a call whose wire signature differs from "sv", and ignores a value
            # that is not a string or a{sv}; dbus-python accepts both.
            if message.get_signature() != "sv" or not fcitx5_reads(new_values):
                raise dbus.exceptions.DBusException(
                    "got " + message.get_signature(), name="org.freedesktop.DBus.Error.InvalidArgs"
                )
            if uri == addon:
                values.update(plain(new_values))
            with open(log, "a", encoding="utf-8") as f:
                f.write(json.dumps({"uri": str(uri), "values": plain(new_values)}) + "\n")

    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    _name = dbus.service.BusName("org.fcitx.Fcitx5", bus)
    Controller(bus, "/controller")
    print("ready", flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    _serve(sys.argv[1])
