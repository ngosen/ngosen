// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx-utils/event.h>

/**
 * @brief Headless Mock InputContext for Fcitx5-Lotus Integration Testing.
 *
 * Architecture & Rationale:
 * Fcitx5 input method engines interact with client applications (text editors,
 * browsers, terminals) through an abstract `fcitx::InputContext` interface. In a
 * live desktop environment, concrete implementations (like X11, Wayland, or D-Bus
 * frontends) handle IPC and render UI.
 *
 * In headless integration tests, we need to verify engine behavior (commits,
 * backspaces, forwarded keys, preedit updates) without running a display server
 * (X11/Wayland) or D-Bus daemon. `TestInputContext` subclasses `fcitx::InputContext`
 * directly, intercepting all engine output callbacks and recording them into
 * in-memory vectors for synchronous, deterministic assertions.
 *
 * Example — Writing a new integration test:
 * @code
 * int main() {
 *     // 1. Sandbox filesystem paths to /tmp/fcitx5-lotus-my-test
 *     configureTestPaths("fcitx5-lotus-my-test");
 *
 *     // 2. Initialize headless Fcitx instance and Lotus engine
 *     TestInstance testInstance;
 *     fcitx::LotusEngine engine(&testInstance.instance);
 *
 *     // 3. Set engine mode and input method
 *     fcitx::RawConfig config;
 *     config.setValueByPath("Mode", "Preedit"); // or "Surrounding", "Sen", "Off"
 *     config.setValueByPath("InputMethod", "Telex");
 *     engine.setConfig(config);
 *
 *     // 4. Create mock context with client capabilities and activate
 *     auto context = std::make_unique<TestInputContext>(&testInstance.instance);
 *     context->setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
 *     context->focusIn();
 *     fcitx::InputMethodEntry entry("lotus", "Lotus", "vi", "lotus");
 *     fcitx::InputContextEvent in(context.get(), fcitx::EventType::InputContextFocusIn);
 *     engine.activate(entry, in);
 *
 *     // 5. Send keystrokes to the engine
 *     fcitx::KeyEvent event(context.get(), fcitx::Key(FcitxKey_a), false);
 *     engine.keyEvent(entry, event);
 *
 *     // 6. Assert on recorded outputs
 *     // - Commits:        context->commits()
 *     // - Deletions:      context->deletes()
 *     // - Forwarded keys: context->forwarded()
 *     // - Client preedit: context->inputPanel().clientPreedit().toString()
 *     if (!event.accepted() || context->inputPanel().clientPreedit().toString() != "a") {
 *         return 1; // Test failed
 *     }
 *     return 0; // Test passed
 * }
 * @endcode
 *
 * CMake Registration (test/CMakeLists.txt):
 * @code
 * add_lotus_headless_test(my_test my-test.cpp "integration;myfeature")
 * @endcode
 */
class TestInputContext final : public fcitx::InputContext {
  public:
    explicit TestInputContext(fcitx::Instance* instance, const std::string& program = "test", std::string frontend = "test") :
        InputContext(instance->inputContextManager(), program), frontend_(std::move(frontend)) {
        // Fcitx5 lifecycle contract: notifies the InputContextManager that
        // this context is initialized and ready to receive events.
        created();
    }

    ~TestInputContext() override {
        // Fcitx5 lifecycle contract: detaches context and invalidates event slots.
        destroy();
    }

    const char* frontend() const override {
        return frontend_.c_str();
    }

    // Called by the engine when text is committed to the client application.
    void commitStringImpl(const std::string& text) override {
        commits_.push_back(text);
    }

    // Called when the engine requests surrounding text deletion (e.g. backspacing
    // prior characters during composition or word replacement).
    void deleteSurroundingTextImpl(int offset, unsigned int size) override {
        deletes_.emplace_back(offset, size);
    }

    // Called when a key event is not consumed by the engine and forwarded
    // back to the client application (passthrough mode).
    void forwardKeyImpl(const fcitx::ForwardKeyEvent& event) override {
        forwarded_.push_back(event);
    }

    // Called when inline preedit text or server candidate window changes.
    void updatePreeditImpl() override {
        ++preeditUpdates_;
    }

    // Verification helpers: inspect recorded engine actions
    void resetPreeditUpdateCount() {
        preeditUpdates_ = 0;
    }
    unsigned int preeditUpdates() const {
        return preeditUpdates_;
    }
    const std::vector<std::string>& commits() const {
        return commits_;
    }
    const auto& deletes() const {
        return deletes_;
    }
    const auto& forwarded() const {
        return forwarded_;
    }

  private:
    std::string                               frontend_;
    std::vector<std::string>                  commits_;
    std::vector<std::pair<int, unsigned int>> deletes_;
    std::vector<fcitx::ForwardKeyEvent>       forwarded_;
    unsigned int                              preeditUpdates_ = 0;
};

/**
 * @brief Sandboxes environment paths to isolate tests from host user configuration.
 *
 * Fcitx5 engines and Bamboo read/write user configuration, dictionaries, and cache
 * under $HOME, $XDG_CONFIG_HOME, $XDG_DATA_HOME, and $XDG_CACHE_HOME.
 * Calling `configureTestPaths` redirects all these paths to an ephemeral subdirectory
 * in the system temporary folder (`/tmp/<name>`).
 *
 * Guarantees:
 * 1. Clean slate: Previous runs or local user configs will not contaminate test results.
 * 2. Host safety: Tests will NEVER overwrite developer's personal fcitx5 settings.
 * 3. Parallel-safe: Each test target passes a unique test name for isolated folders.
 */
inline void configureTestPaths(const char* name) {
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "config/fcitx5/conf");
    std::filesystem::create_directories(root / "data");
    std::filesystem::create_directories(root / "cache");
    setenv("HOME", root.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
    setenv("XDG_DATA_HOME", (root / "data").c_str(), 1);
    setenv("XDG_CACHE_HOME", (root / "cache").c_str(), 1);
    // The engine spell-checks against lotus/vietnamese.cm.dict from the data dirs. Without a copy here
    // it falls back to the one the host happens to have installed: a clean build box has none, so a
    // valid word like "chư" is restored to "uw" on space and tests pass on a dev machine but fail in
    // the package %check.
    // Copy the source tree's dictionary so every run spell-checks against the code under test.
    std::filesystem::create_directories(root / "data/fcitx5/lotus");
    std::filesystem::copy_file(LOTUS_TEST_DICTIONARY, root / "data/fcitx5/lotus/vietnamese.cm.dict");
}

/**
 * @brief Headless Fcitx5 instance environment isolator.
 *
 * Instantiates a minimal, headless `fcitx::Instance` with:
 * - `--disable=all`: disables loading external modules (UI, D-Bus, Wayland/X11 frontends),
 *   allowing the instance to initialize in pure headless/CI containers without hanging.
 * - `registerDefaultLoader(nullptr)`: prevents dynamic plugin loader from scanning
 *   system directories (`/usr/lib/fcitx5`) for host-installed addons.
 */
struct TestInstance {
    TestInstance() : instance(2, argv) {
        instance.addonManager().registerDefaultLoader(nullptr);
        instance.initialize();
    }
    char            program[64]    = "lotus-headless-test";
    char            disableAll[16] = "--disable=all";
    char*           argv[3]        = {program, disableAll, nullptr};
    fcitx::Instance instance;
};

/**
 * @brief Runs the instance's event loop for `ms` milliseconds, then anything already due.
 *
 * Tests call `keyEvent()` directly, so timers the engine schedules with
 * `addTimeEvent()` (Uinput mode waits for the app between the last backspace
 * and the commit) never fire unless the loop runs, as it does in real fcitx5.
 *
 * `EventLoop::exec()` cannot be used for this: after `exit()` an sd-event loop
 * is finished and a second `exec()` throws (-ESTALE). fcitx does not expose a
 * single-step call, so the backend's own one is looked up at runtime; the
 * backend library is already loaded by fcitx5-utils:
 * - sd-event (fcitx5 built with systemd, e.g. Fedora): `sd_event_run`;
 * - libuv (fcitx5 built with -DUSE_SYSTEMD=Off, as the CI builds it on Arch):
 *   `uv_run` in UV_RUN_NOWAIT mode, which fires every timer already due.
 *   `nativeHandle()` is the `uv_loop_t*` there (fcitx5 event_libuv.cpp).
 */
inline void pumpEventLoop(fcitx::Instance& instance, int ms) {
    const std::string impl   = fcitx::EventLoop::impl();
    void*             handle = instance.eventLoop().nativeHandle();
    const uint64_t    end    = fcitx::now(CLOCK_MONOTONIC) + (static_cast<uint64_t>(ms) * 1000ULL);
    if (impl == "sd-event") {
        using SdEventRun      = int (*)(void*, uint64_t);
        static const auto run = reinterpret_cast<SdEventRun>(dlsym(RTLD_DEFAULT, "sd_event_run"));
        if (run == nullptr) {
            std::cerr << "pumpEventLoop: sd_event_run not found\n";
            std::abort();
        }
        for (uint64_t t = fcitx::now(CLOCK_MONOTONIC); t < end; t = fcitx::now(CLOCK_MONOTONIC)) {
            if (run(handle, end - t) < 0) {
                std::cerr << "pumpEventLoop: sd_event_run failed\n";
                std::abort();
            }
        }
        // The deadline can pass while the process is not scheduled; what is due by now still runs.
        for (int i = 0; i < 64; ++i) {
            const int dispatched = run(handle, 0);
            if (dispatched < 0) {
                std::cerr << "pumpEventLoop: sd_event_run failed\n";
                std::abort();
            }
            if (dispatched == 0)
                break;
        }
        return;
    }
    if (impl == "libuv") {
        using UvRun                   = int (*)(void*, int);
        static const auto run         = reinterpret_cast<UvRun>(dlsym(RTLD_DEFAULT, "uv_run"));
        constexpr int     uvRunNowait = 2; // UV_RUN_NOWAIT, stable across libuv 1.x
        if (run == nullptr) {
            std::cerr << "pumpEventLoop: uv_run not found\n";
            std::abort();
        }
        for (uint64_t t = fcitx::now(CLOCK_MONOTONIC); t < end; t = fcitx::now(CLOCK_MONOTONIC)) {
            run(handle, uvRunNowait);
            std::this_thread::sleep_for(std::chrono::microseconds(std::min<uint64_t>(1000, end - t)));
        }
        run(handle, uvRunNowait); // a timer due exactly at `end`
        return;
    }
    std::cerr << "pumpEventLoop: unsupported event loop '" << impl << "'\n";
    std::abort();
}
