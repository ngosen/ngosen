/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/*
 * The typing core's C interface. Handles are opaque numbers, 0 meaning none; DeleteObject frees
 * any of them. Returned strings and arrays are allocated with malloc and freed by the caller.
 */

#ifndef BAMBOO_CORE_H
#define BAMBOO_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const char cchar;

typedef struct {
    bool        autoNonVnRestore;
    bool        ddFreeStyle;
    bool        macroEnabled;
    bool        autoCapitalizeMacro;
    bool        spellCheckWithDicts;
    const char* outputCharset;
    bool        modernStyle;
    bool        freeMarking;
    int         w2u;
    int         bracketTransform;
    const char* timeFormat;
    const char* dateFormat;
    bool        teencode;
} FcitxBambooEngineOption;

extern void      Init(void);
extern uint8_t   EngineProcessKeyEvent(uintptr_t engine, uint32_t keyVal, uint32_t state);
extern char*     EnginePullPreedit(uintptr_t engine);
extern char*     EnginePullRawText(uintptr_t engine);
extern char*     EnginePullComposed(uintptr_t engine);
extern void      EngineCommitPreedit(uintptr_t engine);
extern char*     EnginePullCommit(uintptr_t engine);
extern void      EngineSetOption(uintptr_t engine, FcitxBambooEngineOption* option);
extern uintptr_t NewEngine(cchar* name, uintptr_t dictHandle, uintptr_t tableHandle);
extern uintptr_t NewCustomEngine(char** definition, uintptr_t dictHandle, uintptr_t tableHandle);
extern uintptr_t NewMacroTable(char** definition);
extern void      DeleteObject(uintptr_t handle);
extern void      ResetEngine(uintptr_t engine);
extern void      EngineRebuildFromText(uintptr_t engine, cchar* text);
extern void      EngineReopenCommitted(uintptr_t engine, cchar* text);
extern char**    GetCharsetNames(void);
// letterCase: 0 keep, 1 upper, 2 lower, 3 sentence, 4 title. Null for an unknown charset or case.
extern char*     ConvertText(cchar* text, cchar* from, cchar* to, int letterCase, bool stripDiacritics);
extern char**    GetInputMethodNames(void);
extern uintptr_t NewDictionary(uintptr_t fd);
extern uintptr_t NewWordList(uintptr_t fd);
extern void      EngineSetEnglishWords(uintptr_t engine, uintptr_t list);
extern void      EngineSetKeepGuard(uintptr_t engine, uintptr_t list);
extern uintptr_t NewShorthand(uintptr_t fd);
extern void      EngineSetShorthand(uintptr_t engine, uintptr_t table);
extern void      ShorthandKeepIn(uintptr_t table, const char* path);
extern void      ShorthandSetLearning(uintptr_t table, uint8_t on);
extern char*     ShorthandForget(uintptr_t table);
extern char*     ShorthandRemoveFile(const char* path);
extern char*     ShorthandList(uintptr_t table);
extern char*     ShorthandExpandAgain(uintptr_t table, const char* key);
extern void      ShorthandAwaitUse(uintptr_t table);
extern char*     ShorthandTakeFirstUse(uintptr_t table);
extern void      EngineSetDropKeptTail(uintptr_t engine, uint8_t on);
extern char*     EnginePullDroppedWord(uintptr_t engine);
extern char*     EnginePullChanges(uintptr_t engine);
extern void      EngineSetFixing(uintptr_t engine, uint8_t margin);
extern void      EngineSetCountingPlainWords(uintptr_t engine, uint8_t on);
extern void      EngineTypedOn(uintptr_t engine);
extern void      EngineErasedBack(uintptr_t engine);
extern char*     EngineUndoFix(uintptr_t engine);
extern char*     EngineEarlierFixShown(uintptr_t engine);
extern char*     EngineUndoEarlierFix(uintptr_t engine);
extern char*     EngineRestoreText(uintptr_t engine, cchar* text);
extern void      EngineSetDiacritics(uintptr_t engine, uintptr_t diacritics);
extern void      EngineSetPrivate(uintptr_t engine, uint8_t on);
extern void      EngineSetWholeRun(uintptr_t engine, uint8_t on);
extern void      EngineEndWord(uintptr_t engine);
extern uintptr_t DiacriticsOpen(const char* table, const char* dictionary);
extern char*     DiacriticsProblem(uintptr_t diacritics);
extern uint8_t   DiacriticsSave(uintptr_t diacritics, uint8_t wait);
extern void      DiacriticsSetLearning(uintptr_t diacritics, uint8_t on);
extern char*     DiacriticsSaveNow(uintptr_t diacritics, uint8_t* written);
extern char*     DiacriticsForget(uintptr_t diacritics);
extern char*     DiacriticsRemoveFiles(const char* path);
extern uint8_t   EngineKeptAtLastKey(uintptr_t engine);
extern uint8_t   EngineShownAsTyped(uintptr_t engine);
// 1 when the input method can compose a word with the key, as VNI does with digits.
extern uint8_t EngineComposesWith(uintptr_t engine, uint32_t keyVal);
// 1 when text is one correctly spelled Vietnamese syllable exactly as written, marks included.
extern uint8_t IsVietnameseSyllable(cchar* text);

#ifdef __cplusplus
}
#endif

#endif
