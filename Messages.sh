#!/bin/bash

xgettext \
--language=C++ \
--from-code=UTF-8 \
--keyword=_ \
--keyword=N_ \
--keyword=translate \
-o /tmp/ngosen-cpp.pot \
$(find . \( -name "*.cpp" -o -name "*.h" \))

xgettext \
--language=appdata \
--from-code=UTF-8 \
-o /tmp/ngosen-xml.pot \
io.github.ngosen.NgoSen.metainfo.xml.in.in

xgettext \
--language=Python \
--from-code=UTF-8 \
--keyword=_ \
--keyword=N_ \
-o /tmp/ngosen-python.pot \
$(find . -name "*.py")

xgettext \
--language=Desktop \
--from-code=UTF-8 \
--keyword=Name \
--keyword=Comment \
-o /tmp/ngosen-desktop.pot \
settings-gui/io.github.ngosen.NgoSen.Settings.desktop.in

{
    echo 'msgid ""'
    echo 'msgstr ""'
    echo '"Content-Type: text/plain; charset=UTF-8\n"'
    echo ""
    grep -hE "^Name=" \
    src/ngosen.conf.in \
    src/ngosen-addon.conf.in.in \
    | sed 's/^Name=\(.*\)/msgid "\1"\nmsgstr ""\n/'
} > /tmp/ngosen-conf.pot

msgcat \
--use-first \
/tmp/ngosen-cpp.pot \
/tmp/ngosen-xml.pot \
/tmp/ngosen-conf.pot \
/tmp/ngosen-python.pot \
/tmp/ngosen-desktop.pot \
-o po/fcitx5-ngosen.pot
