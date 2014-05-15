#include <uwsgi.h>

void qtloop_loop(void);

static void qtloop_setup() {
        uwsgi_register_loop( (char *) "qt", qtloop_loop);
}


struct uwsgi_plugin qtloop_plugin = {
        .name = "qtloop",
        .on_load = qtloop_setup,
};

