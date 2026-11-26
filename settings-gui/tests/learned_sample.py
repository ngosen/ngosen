# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""A file of what Đoán dấu learned, as the engine saves it."""

import os

SAVED = (
    "timsen-personal 1\n"
    "1\t2\t<s>\n"
    "1\t3\tngó\n"
    "2\t1\t<s>\tngó\n"
    "2\t5\tngó\tsen\n"
    "3\t1\tngó\tsen\t</s>\n"
)


def write(file, text):
    os.makedirs(os.path.dirname(file), exist_ok=True)
    with open(file, "w", encoding="utf-8") as f:
        f.write(text)
