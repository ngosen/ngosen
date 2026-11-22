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

    int             failures = 0;
    ngosen::Options options;

    void            check(bool ok, const std::string& what) {
        if (!ok) {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    std::string first(const std::vector<std::string>& words) {
        const auto s = suggestEmoji(words, options);
        return s ? s->emoji.front() : std::string("none");
    }

    void offers(const std::vector<std::string>& words, const std::string& emoji, size_t covered = 1) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        const auto s = suggestEmoji(words, options);
        check(s && s->emoji.front() == emoji && s->words == covered, typed + "-> " + emoji + ", got " + first(words) + (s ? " over " + std::to_string(s->words) : ""));
    }

    void offersNothing(const std::vector<std::string>& words) {
        std::string typed;
        for (const auto& w : words)
            typed += w + " ";
        check(!suggestEmoji(words, options), typed + "-> nothing, got " + first(words));
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
    check(suggestEmoji({"haha"}, options)->kind == EmojiKind::Sound, "haha is a sound");
    check(suggestEmoji({"mèo"}, options)->kind == EmojiKind::Word, "mèo is a word");
    offers({":)"}, "🙂");
    offers({":D"}, "😃");
    offers({"<3"}, "❤️");
    check(suggestEmoji({":)"}, options)->kind == EmojiKind::Emoticon, ":) is an emoticon");

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
    check(suggestEmoji({"=))"}, options)->kind == EmojiKind::Emoticon, "a Yahoo code is replaced like an emoticon");
    options.yahooEmoticons = false;
    offersNothing({"=))"});
    offersNothing({"@};-"});
    offers({":)"}, "🙂");
    options.yahooEmoticons = true;

    offersNothing({});
    offersNothing({"có"});
    offersNothing({"không"});
    offersNothing({"fre"});

    const auto many = suggestEmoji({"cười"}, options);
    check(many && many->emoji.size() == 5, "cười offers five emoji");

    // The user's own words, and default ones turned off.
    options.emojiWordEdits = {{"sếp", "👔", true}, {"cười", "🥲", true}, {"cười", "😂", false}, {"mèo", "🐈", false}, {"mèo", "🐱", false}, {"chúc ngủ ngon", "😴", true}};
    offers({"sếp"}, "👔");
    offers({"Sếp"}, "👔");
    check(suggestEmoji({"sếp"}, options)->kind == EmojiKind::Word, "an added word goes after the word");
    offers({"cười"}, "🥲");
    const auto edited = suggestEmoji({"cười"}, options);
    check(edited && edited->emoji.size() == 5 && std::find(edited->emoji.begin(), edited->emoji.end(), "😂") == edited->emoji.end(), "an emoji turned off for a word is left out");
    offersNothing({"mèo"});
    offers({"mèo", "đen"}, "🐈‍⬛", 2);
    offers({"chúc", "ngủ", "ngon"}, "😴", 3);
    offers({"hahaha"}, "😂");
    options.emojiWordEdits = {{"xương", "🦴", false}, {"xương", "💀", true}};
    const auto folded      = suggestEmoji({"xuong"}, options);
    check(folded && std::find(folded->emoji.begin(), folded->emoji.end(), "🦴") == folded->emoji.end(), "an edit reaches every keyword typed without marks");
    offers({"xuong"}, "💀");
    options.emojiWordEdits = {{"haha", "🤭", true}};
    offers({"hahaha"}, "🤭");
    check(suggestEmoji({"hahaha"}, options)->kind == EmojiKind::Sound, "an emoji added to a sound replaces it like the others");
    options.emojiWordEdits.clear();

    // English keywords from CLDR, behind their own switch.
    offers({"cat"}, "🐈");
    offers({"thumbs", "up"}, "👍", 2);
    offers({"star"}, "⭐"); // the emoji named star before the others that list it
    check(suggestEmoji({"cat"}, options)->kind == EmojiKind::Word, "an English keyword goes after the word");
    offersNothing({"face"}); // shared by too many emoji to name one
    offersNothing({"can"});  // also Vietnamese, so not English when typed
    // Letters, numbers and chat shorthand are typed all the time; none of them should bring up an emoji.
    offersNothing({"t"}); // t-rex
    offersNothing({"r"});
    offersNothing({"2"});
    offersNothing({"18"});
    offersNothing({"ng"}); // người
    offersNothing({"vs"}); // với
    offers({"t-rex"}, "🦖");
    offers({"cười"}, "😂"); // a Vietnamese keyword is unchanged
    options.englishEmojiKeywords = false;
    offersNothing({"cat"});
    offers({"love"}, "❤️"); // picked by hand, not one of the English ones
    options.emojiWordEdits = {{"cat", "🐱", true}};
    offers({"cat"}, "🐱"); // a word the user added stays when English is off
    options.englishEmojiKeywords = true;
    options.emojiWordEdits.clear();

    if (failures)
        std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
