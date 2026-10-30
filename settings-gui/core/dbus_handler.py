# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
D-Bus handler to communicate with Fcitx5 Controller.
"""

import dbus


class NgoSenDBusHandler:
    def __init__(self):
        self.addon_name = "fcitx://config/addon/ngosen"
        try:
            self.bus = dbus.SessionBus()
        except dbus.DBusException as e:
            print(f"Fcitx5 D-Bus Error: {e}")
            self.bus = None

    @property
    def iface(self):
        # Looked up on each call: a proxy binds to the fcitx5 running when it is made, and the window
        # outlives an fcitx5 restart.
        if not self.bus:
            return None
        try:
            proxy = self.bus.get_object("org.fcitx.Fcitx5", "/controller", introspect=False)
        except dbus.DBusException as e:
            print(f"Fcitx5 D-Bus Error: {e}")
            return None
        return dbus.Interface(proxy, "org.fcitx.Fcitx.Controller1")

    def get_config(self) -> dict:
        """Get config from Fcitx5 and convert to Python dict/list."""
        if not self.iface:
            return {}
        try:
            values, metadata = self.iface.GetConfig(self.addon_name)
            return {
                "values": self._clean_dbus(values),
                "metadata": self._clean_dbus(metadata),
            }
        except Exception as e:
            print(f"Failed to fetch config: {e}")
            return {}

    def set_config(self, values_dict: dict) -> bool:
        """Set config and send to Fcitx5. Returns True on success, False otherwise."""
        if not self.iface:
            return False
        try:
            dbus_dict = self._prepare_dbus_data(values_dict, variant_level=1)
            self.iface.SetConfig(self.addon_name, dbus_dict)
            return True
        except Exception as e:
            print(f"Failed to set config: {e}")
            return False

    def reload_addon_config(self) -> bool:
        """Makes the addon read its files again, which SetConfig does not do for the dictionary."""
        if not self.iface:
            return False
        try:
            self.iface.ReloadAddonConfig(self.addon_name.rsplit("/", 1)[-1])
            return True
        except Exception as e:
            print(f"Failed to reload addon config: {e}")
            return False

    def input_method_names(self):
        """Input methods of the current group, or None when fcitx5 does not answer."""
        if not self.iface:
            return None
        try:
            group = self.iface.CurrentInputMethodGroup()
            items = self.iface.FullInputMethodGroupInfo(group)[4]
            return [str(item[0]) for item in items]
        except Exception as e:
            print(f"Failed to read the input method group: {e}")
            return None

    def has_input_method(self, name):
        """Whether fcitx5 has loaded the input method, or None when it does not answer."""
        if not self.iface:
            return None
        try:
            return any(str(im[0]) == name for im in self.iface.AvailableInputMethods())
        except Exception as e:
            print(f"Failed to read the available input methods: {e}")
            return None

    def input_method_group(self):
        """(name, default layout, [(input method, layout)]) of the current group, or None."""
        if not self.iface:
            return None
        try:
            name = str(self.iface.CurrentInputMethodGroup())
            layout, items = self.iface.InputMethodGroupInfo(name)
            return name, str(layout), [(str(im), str(im_layout)) for im, im_layout in items]
        except Exception as e:
            print(f"Failed to read the input method group: {e}")
            return None

    def set_input_method_group(self, name, layout, items) -> bool:
        if not self.iface:
            return False
        try:
            entries = dbus.Array([dbus.Struct(item) for item in items], signature="(ss)")
            self.iface.SetInputMethodGroupInfo(name, layout, entries)
            return True
        except Exception as e:
            print(f"Failed to set the input method group: {e}")
            return False

    def restart(self) -> bool:
        """Restarts fcitx5 so it loads the installed addon."""
        if not self.iface:
            return False
        try:
            if not self.iface.CanRestart():
                return False
            self.iface.Restart()
            return True
        except Exception as e:
            print(f"Failed to restart fcitx5: {e}")
            return False

    def debug_info(self) -> str:
        """fcitx5's list of connected input contexts, with each one's program and frontend."""
        if not self.iface:
            return ""
        try:
            return str(self.iface.DebugInfo())
        except Exception as e:
            print(f"Failed to read fcitx5 debug info: {e}")
            return ""

    def get_sub_config_list(self, path: str, root_key: str) -> list:
        """Get sub config list from Fcitx5 and convert to Python list."""
        if not self.iface:
            return []
        try:
            full_path = f"{self.addon_name}/{path}"
            values, metadata = self.iface.GetConfig(full_path)
            clean_values = self._clean_dbus(values)

            array_dict = clean_values.get(root_key, {})
            if isinstance(array_dict, dict):
                sorted_keys = sorted(
                    array_dict.keys(),
                    key=lambda k: (0, int(k)) if str(k).isdigit() else (1, str(k)),
                )
                return [array_dict[k] for k in sorted_keys]
            elif isinstance(array_dict, list):
                return array_dict
            return []
        except Exception as e:
            print(f"Failed to fetch sub config ({path}): {e}")
            return []

    def set_sub_config_list(self, path: str, root_key: str, data_list: list) -> bool:
        """Set sub config list and send to Fcitx5. Returns True on success, False otherwise."""
        if not self.iface:
            return False
        try:
            full_path = f"{self.addon_name}/{path}"
            fcitx_array = {str(i): item for i, item in enumerate(data_list)}
            dbus_payload = {root_key: fcitx_array}
            dbus_dict = self._prepare_dbus_data(dbus_payload, variant_level=1)
            self.iface.SetConfig(full_path, dbus_dict)
            return True
        except Exception as e:
            print(f"Failed to set sub config ({path}): {e}")
            return False

    def _prepare_dbus_data(self, data, variant_level=0):
        """Prepare data to be sent to Fcitx5 in dbus types with signatures."""
        if isinstance(data, dict):
            # Fcitx5 expects dicts to be a{sv} (String to Variant)
            formatted = {str(k): self._prepare_dbus_data(v) for k, v in data.items()}
            return dbus.Dictionary(formatted, signature="sv", variant_level=variant_level)
        elif isinstance(data, list):
            # Arrays must be Array of Variants (av)
            formatted = [self._prepare_dbus_data(v) for v in data]
            return dbus.Array(formatted, signature="v")
        elif isinstance(data, bool):
            return dbus.Boolean(data)
        elif isinstance(data, int):
            return dbus.Int32(data)
        elif isinstance(data, float):
            return dbus.Double(data)
        elif data is None:
            return dbus.String("")
        else:
            return dbus.String(str(data))

    def _clean_dbus(self, data):
        """Convert dbus types to Python types."""
        if isinstance(data, dbus.Dictionary):
            return {str(k): self._clean_dbus(v) for k, v in data.items()}
        elif isinstance(data, (dbus.Array, dbus.Struct, list, tuple)):
            return [self._clean_dbus(v) for v in data]
        elif isinstance(data, dbus.Boolean):
            return bool(data)
        elif isinstance(
            data,
            (dbus.Int16, dbus.Int32, dbus.Int64, dbus.UInt16, dbus.UInt32, dbus.UInt64),
        ):
            return int(data)
        elif isinstance(data, dbus.Double):
            return float(data)
        elif isinstance(data, dbus.String):
            return str(data)
        return data
