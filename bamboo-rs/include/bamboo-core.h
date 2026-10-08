/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/*
 * The typing core's C interface, matching the header cgo generated for the Go core. Handles are
 * opaque numbers, 0 meaning none; DeleteObject frees any of them. Returned strings and arrays
 * are allocated with malloc and freed by the caller.
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
} FcitxBambooEngineOption;

extern void      Init(void);
extern uint8_t   EngineProcessKeyEvent(uintptr_t engine, uint32_t keyVal, uint32_t state);
extern void      EngineSetRestoreKeyStroke(uintptr_t engine);
extern char*     EnginePullPreedit(uintptr_t engine);
extern void      EngineCommitPreedit(uintptr_t engine);
extern char*     EnginePullCommit(uintptr_t engine);
extern void      EngineSetMacroEnabled(uintptr_t engine, uint8_t enabled);
extern void      EngineSetOption(uintptr_t engine, FcitxBambooEngineOption* option);
extern uintptr_t NewEngine(cchar* name, uintptr_t dictHandle, uintptr_t tableHandle);
extern uintptr_t NewCustomEngine(char** definition, uintptr_t dictHandle, uintptr_t tableHandle);
extern uintptr_t NewMacroTable(char** definition);
extern void      DeleteObject(uintptr_t handle);
extern void      ResetEngine(uintptr_t engine);
extern void      EngineRebuildFromText(uintptr_t engine, cchar* text);
extern char**    GetCharsetNames(void);
extern char**    GetInputMethodNames(void);
extern uintptr_t NewDictionary(uintptr_t fd);

#ifdef __cplusplus
}
#endif

#endif
