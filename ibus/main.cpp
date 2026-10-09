/*
 * SPDX-FileCopyrightText: 2026 Ngó Sen contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ngosen-ibus-engine.h"

#include <cstdio>
#include <cstring>
#include <ibus.h>

int main(int argc, char** argv) {
    ibus_init();
    IBusBus* bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) {
        std::fprintf(stderr, "ibus-engine-ngosen: cannot connect to the IBus daemon\n");
        return 1;
    }
    g_signal_connect(bus, "disconnected", G_CALLBACK(+[](IBusBus*, gpointer) { ibus_quit(); }), nullptr);

    IBusFactory* factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ngosen::addEngine(factory);

    // The daemon starts the engine with --ibus from the component file; without it the engine
    // registers itself, so it can be run by hand against a daemon that has no component file.
    if (argc > 1 && std::strcmp(argv[1], "--ibus") == 0) {
        ibus_bus_request_name(bus, "org.freedesktop.IBus.NgoSen", 0);
    } else {
        IBusComponent* component = ibus_component_new("org.freedesktop.IBus.NgoSen", "Ngó Sen", NGOSEN_VERSION, "GPL-3.0-or-later", "Ngó Sen contributors",
                                                      "https://github.com/ngosen/ngosen", "", "ibus-ngosen");
        ibus_component_add_engine(component, ibus_engine_desc_new("ngosen", "Ngó Sen", "Vietnamese input method", "vi", "GPL-3.0-or-later", "Ngó Sen contributors", "", "us"));
        if (!ibus_bus_register_component(bus, component)) {
            std::fprintf(stderr, "ibus-engine-ngosen: the IBus daemon refused the engine\n");
            return 1;
        }
    }
    ibus_main();
    return 0;
}
