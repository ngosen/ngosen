/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Which emoji the typed words offer (#110).
#include "ngosen-emoji-keywords.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using ngosen::EmojiKind;
using ngosen::suggestEmoji;

namespace {

    int                                failures = 0;
    bool                               yahoo    = true;
    std::vector<ngosen::EmojiWordEdit> edits;

    void                               check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    std::string first(const std::vector<std::string>& words) {
        const auto s = suggestEmoji(words, yahoo, edits);
        return s ? s->emoji.front() : std::string("none");
    }

    void offers(const std::vector<std::string>& words, const std::string& emoji, size_t covered = 1) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        const auto s = suggestEmoji(words, yahoo, edits);
        check(s && s->emoji.front() == emoji && s->words == covered, typed + "-> " + emoji + ", got " + first(words) + (s ? " over " + std::to_string(s->words) : ""));
    }

    void offersNothing(const std::vector<std::string>& words) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        check(!suggestEmoji(words, yahoo, edits), typed + "-> nothing, got " + first(words));
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
    check(suggestEmoji({"haha"}, yahoo, edits)->kind == EmojiKind::Sound, "haha is a sound");
    check(suggestEmoji({"mèo"}, yahoo, edits)->kind == EmojiKind::Word, "mèo is a word");
    offers({":)"}, "🙂");
    offers({":D"}, "😃");
    offers({"<3"}, "❤️");
    check(suggestEmoji({":)"}, yahoo, edits)->kind == EmojiKind::Emoticon, ":) is an emoticon");

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

    // Whole-word emoji names from CLDR, but not a syllable that mostly sits inside other words.
    offers({"máy", "bay"}, "✈️", 2);
    offers({"đi", "xe", "máy"}, "🏍️", 2);
    offers({"cơm"}, "🍚");
    offersNothing({"báo", "cáo"});
    offersNothing({"ý", "kiến"});
    offersNothing({"kiên", "nhẫn"});
    offersNothing({"cứng", "rắn"});
    offers({"con", "rắn"}, "🐍", 2);
    offers({"gấu", "trúc"}, "🐼", 2);

    offers({"=))"}, "🤣");
    offers({":))"}, "😆");
    offers({"B-)"}, "😎");
    offers({"@};-"}, "🌹");
    offers({":\">"}, "😳");
    offers({"\\m/"}, "🤘");
    offers({"#-o"}, "🤦");
    offers({"#:-S"}, "😅");
    check(suggestEmoji({"=))"}, yahoo, edits)->kind == EmojiKind::Emoticon, "a Yahoo code is replaced like an emoticon");
    yahoo = false;
    offersNothing({"=))"});
    offersNothing({"@};-"});
    offers({":)"}, "🙂");
    yahoo = true;

    offersNothing({});
    offersNothing({"có"});
    offersNothing({"không"});
    offersNothing({"free"});

    const auto many = suggestEmoji({"cười"}, yahoo, edits);
    check(many && many->emoji.size() == 5, "cười offers five emoji");

    // The user's own words, and default ones turned off.
    edits = {{"sếp", "👔", true}, {"cười", "🥲", true}, {"cười", "😂", false}, {"mèo", "🐈", false}, {"mèo", "🐱", false}, {"chúc ngủ ngon", "😴", true}};
    offers({"sếp"}, "👔");
    offers({"Sếp"}, "👔");
    check(suggestEmoji({"sếp"}, yahoo, edits)->kind == EmojiKind::Word, "an added word goes after the word");
    offers({"cười"}, "🥲");
    const auto edited = suggestEmoji({"cười"}, yahoo, edits);
    check(edited && edited->emoji.size() == 5 && std::find(edited->emoji.begin(), edited->emoji.end(), "😂") == edited->emoji.end(), "an emoji turned off for a word is left out");
    offersNothing({"mèo"});
    offers({"mèo", "đen"}, "🐈‍⬛", 2);
    offers({"chúc", "ngủ", "ngon"}, "😴", 3);
    offers({"hahaha"}, "😂");
    edits = {{"xương", "🦴", false}, {"xương", "💀", true}};
    const auto folded = suggestEmoji({"xuong"}, yahoo, edits);
    check(folded && std::find(folded->emoji.begin(), folded->emoji.end(), "🦴") == folded->emoji.end(), "an edit reaches every keyword typed without marks");
    offers({"xuong"}, "💀");
    edits = {{"haha", "🤭", true}};
    offers({"hahaha"}, "🤭");
    check(suggestEmoji({"hahaha"}, yahoo, edits)->kind == EmojiKind::Sound, "an emoji added to a sound replaces it like the others");
    edits.clear();

    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
