/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Which emoji the typed words offer (#110).
#include "ngosen-emoji-keywords.h"

#include <iostream>
#include <string>
#include <vector>

using ngosen::EmojiKind;
using ngosen::suggestEmoji;

namespace {

    int  failures = 0;

    void check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    std::string first(const std::vector<std::string>& words) {
        const auto s = suggestEmoji(words);
        return s ? s->emoji.front() : std::string("none");
    }

    void offers(const std::vector<std::string>& words, const std::string& emoji, size_t covered = 1) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        const auto s = suggestEmoji(words);
        check(s && s->emoji.front() == emoji && s->words == covered, typed + "-> " + emoji + ", got " + first(words) + (s ? " over " + std::to_string(s->words) : ""));
    }

    void offersNothing(const std::vector<std::string>& words) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        check(!suggestEmoji(words), typed + "-> nothing, got " + first(words));
    }

} // namespace

int main() {
    offers({"cười"}, "😂");
    offers({"Cười"}, "😂");
    offers({"con", "mèo"}, "🐈");
    offers({"khóc"}, "😢");

    // Without marks: only a word that cannot be Vietnamese as written is looked up without them.
    offers({"cuoi"}, "😂");
    offers({"khoc"}, "😢");
    offersNothing({"cho"});
    offersNothing({"meo"});
    offersNothing({"trong"});

    offers({"cảm", "ơn"}, "🙏", 2);
    offers({"chúc", "mừng", "sinh", "nhật"}, "🎂", 2);

    offers({"kkkk"}, "😂");
    offers({"hahaha"}, "😂");
    offers({"hihihi"}, "😁");
    check(suggestEmoji({"haha"})->kind == EmojiKind::Sound, "haha is a sound");
    check(suggestEmoji({"mèo"})->kind == EmojiKind::Word, "mèo is a word");
    offers({":)"}, "🙂");
    offers({":D"}, "😃");
    offers({"<3"}, "❤️");
    check(suggestEmoji({":)"})->kind == EmojiKind::Emoticon, ":) is an emoticon");

    // A country needs its capital, which keeps flags off words such as "biện pháp".
    offers({"nước", "Pháp"}, "🇫🇷");
    offers({"nước", "Đức"}, "🇩🇪");
    offers({"Úc"}, "🇦🇺");
    offers({"Ý"}, "🇮🇹");
    offers({"Ấn", "Độ"}, "🇮🇳", 2);
    offersNothing({"úc"});
    offers({"nước", "Mỹ"}, "🇺🇸");
    offers({"Nhật"}, "🇯🇵");
    offersNothing({"biện", "pháp"});
    offersNothing({"đạo", "đức"});
    offers({"Việt", "Nam"}, "🇻🇳", 2);
    offers({"Viet", "Nam"}, "🇻🇳", 2);
    offersNothing({"việt", "nam"});
    offers({"Vietnam"}, "🇻🇳");

    offersNothing({});
    offersNothing({"có"});
    offersNothing({"không"});
    offersNothing({"free"});

    const auto many = suggestEmoji({"cười"});
    check(many && many->emoji.size() == 5, "cười offers five emoji");

    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
