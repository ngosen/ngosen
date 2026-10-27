# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""The custom keymap: which key does which Vietnamese edit, and whether it is on."""

BAMBOO_ACTIONS = [
    ("XoaDauThanh", "Xóa dấu thanh"),
    ("DauSac", "Dấu sắc"),
    ("DauHuyen", "Dấu huyền"),
    ("DauHoi", "Dấu hỏi"),
    ("DauNga", "Dấu ngã"),
    ("DauNang", "Dấu nặng"),
    ("A_Â", "a -> â"),
    ("E_Ê", "e -> ê"),
    ("O_Ô", "o -> ô"),
    ("AEO_ÂÊÔ", "a/e/o -> â/ê/ô"),
    ("UOA_ƯƠĂ", "u/o/a -> ư/ơ/ă"),
    ("D_Đ", "d -> đ"),
    ("UO_ƯƠ", "u/o -> ư/ơ"),
    ("A_Ă", "a -> ă"),
    ("__ă", "ă"),
    ("_Ă", "Ă"),
    ("__â", "â"),
    ("_Â", "Â"),
    ("__ê", "ê"),
    ("_Ê", "Ê"),
    ("__ô", "ô"),
    ("_Ô", "Ô"),
    ("__ư", "ư"),
    ("_Ư", "Ư"),
    ("__ơ", "ơ"),
    ("_Ơ", "Ơ"),
    ("__đ", "đ"),
    ("_Đ", "Đ"),
    ("UOA_ƯƠĂ__Ư", "u/o/a -> ư/ơ/ă, ư"),
]

PRESETS = {
    "Telex": [
        ("z", "XoaDauThanh"),
        ("s", "DauSac"),
        ("f", "DauHuyen"),
        ("r", "DauHoi"),
        ("x", "DauNga"),
        ("j", "DauNang"),
        ("a", "A_Â"),
        ("e", "E_Ê"),
        ("o", "O_Ô"),
        ("w", "UOA_ƯƠĂ"),
        ("d", "D_Đ"),
    ],
    "VNI": [
        ("0", "XoaDauThanh"),
        ("1", "DauSac"),
        ("2", "DauHuyen"),
        ("3", "DauHoi"),
        ("4", "DauNga"),
        ("5", "DauNang"),
        ("6", "AEO_ÂÊÔ"),
        ("7", "UO_ƯƠ"),
        ("8", "A_Ă"),
        ("9", "D_Đ"),
    ],
    "VIQR": [
        ("0", "XoaDauThanh"),
        ("'", "DauSac"),
        ("`", "DauHuyen"),
        ("?", "DauHoi"),
        ("~", "DauNga"),
        (".", "DauNang"),
        ("^", "AEO_ÂÊÔ"),
        ("+", "UO_ƯƠ"),
        ("*", "UO_ƯƠ"),
        ("(", "A_Ă"),
        ("d", "D_Đ"),
    ],
    "Microsoft layout": [
        ("8", "DauSac"),
        ("5", "DauHuyen"),
        ("6", "DauHoi"),
        ("7", "DauNga"),
        ("9", "DauNang"),
        ("1", "__ă"),
        ("!", "_Ă"),
        ("2", "__â"),
        ("@", "_Â"),
        ("3", "__ê"),
        ("#", "_Ê"),
        ("4", "__ô"),
        ("$", "_Ô"),
        ("0", "__đ"),
        (")", "_Đ"),
    ],
    "Telex + VNI": [
        ("z", "XoaDauThanh"),
        ("s", "DauSac"),
        ("f", "DauHuyen"),
        ("r", "DauHoi"),
        ("x", "DauNga"),
        ("j", "DauNang"),
        ("a", "A_Â"),
        ("e", "E_Ê"),
        ("o", "O_Ô"),
        ("w", "UOA_ƯƠĂ"),
        ("d", "D_Đ"),
        ("0", "XoaDauThanh"),
        ("1", "DauSac"),
        ("2", "DauHuyen"),
        ("3", "DauHoi"),
        ("4", "DauNga"),
        ("5", "DauNang"),
        ("6", "AEO_ÂÊÔ"),
        ("7", "UO_ƯƠ"),
        ("8", "A_Ă"),
        ("9", "D_Đ"),
    ],
    "Telex + VNI + VIQR": [
        ("z", "XoaDauThanh"),
        ("s", "DauSac"),
        ("f", "DauHuyen"),
        ("r", "DauHoi"),
        ("x", "DauNga"),
        ("j", "DauNang"),
        ("a", "A_Â"),
        ("e", "E_Ê"),
        ("o", "O_Ô"),
        ("w", "UOA_ƯƠĂ"),
        ("d", "D_Đ"),
        ("0", "XoaDauThanh"),
        ("1", "DauSac"),
        ("2", "DauHuyen"),
        ("3", "DauHoi"),
        ("4", "DauNga"),
        ("5", "DauNang"),
        ("6", "AEO_ÂÊÔ"),
        ("7", "UO_ƯƠ"),
        ("8", "A_Ă"),
        ("9", "D_Đ"),
        ("'", "DauSac"),
        ("`", "DauHuyen"),
        ("?", "DauHoi"),
        ("~", "DauNga"),
        (".", "DauNang"),
        ("^", "AEO_ÂÊÔ"),
        ("+", "UO_ƯƠ"),
        ("*", "UO_ƯƠ"),
        ("(", "A_Ă"),
        ("\\\\", "D_Đ"),
    ],
    "VNI Bàn phím tiếng Pháp": [
        ("&", "XoaDauThanh"),
        ("é", "DauSac"),
        ('"', "DauHuyen"),
        ("'", "DauHoi"),
        ("(", "DauNga"),
        ("-", "DauNang"),
        ("è", "AEO_ÂÊÔ"),
        ("_", "UO_ƯƠ"),
        ("ç", "A_Ă"),
        ("à", "D_Đ"),
    ],
    "Telex W": [
        ("z", "XoaDauThanh"),
        ("s", "DauSac"),
        ("f", "DauHuyen"),
        ("r", "DauHoi"),
        ("x", "DauNga"),
        ("j", "DauNang"),
        ("a", "A_Â"),
        ("e", "E_Ê"),
        ("o", "O_Ô"),
        ("w", "UOA_ƯƠĂ__Ư"),
        ("d", "D_Đ"),
    ],
}


ACTION_CODES = {code for code, _name in BAMBOO_ACTIONS}


def known_action(code):
    # The table can only show listed actions, so an unknown one falls back to the first.
    return code if code in ACTION_CODES else BAMBOO_ACTIONS[0][0]


def state(enabled, entries):
    return {"data": [dict(entry) for entry in entries], "EnableCustomKeymap": enabled}


def is_default(enabled, entries):
    return not entries and not enabled


class Keymap:
    def __init__(self, dbus):
        self.dbus = dbus
        self.enabled = False
        self.entries = []
        self.saved_state = self.state()

    def load(self):
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            self.enabled = str(values.get("EnableCustomKeymap", "False")).lower() == "true"
        self.entries = [
            {"Key": item.get("Key", ""), "Value": known_action(item.get("Value", ""))}
            for item in self.dbus.get_sub_config_list("custom_keymap", "CustomKeymap")
        ]
        self.saved_state = self.state()

    def reset(self):
        self.enabled = False
        self.entries = []

    def is_modified_from_default(self):
        return not is_default(self.enabled, self.entries)

    def state(self):
        return state(self.enabled, self.entries)

    def is_modified(self):
        return self.state() != self.saved_state

    def save(self):
        """Writes the switch and the table. False when fcitx5 cannot be reached or refuses."""
        config_data = self.dbus.get_config()
        if config_data:
            values = config_data.get("values", {})
            values["EnableCustomKeymap"] = "True" if self.enabled else "False"
            if not self.dbus.set_config(values):
                return False
        elif not self.dbus.iface:
            return False

        if not self.dbus.set_sub_config_list("custom_keymap", "CustomKeymap", self.entries):
            return False
        self.saved_state = self.state()
        return True
