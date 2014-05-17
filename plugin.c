#include "common.h"

void qtloop_loop(void);
int qtloop_init(void);

struct qtloop qtloop;

struct uwsgi_option qtloop_options[] = {
	{"qtloop-gui", no_argument, 0, "initialize a QApplication instead of QCoreApplication", uwsgi_opt_true, &qtloop.gui, 0},
};

static void qtloop_setup() {
        uwsgi_register_loop( (char *) "qt", qtloop_loop);
}


struct uwsgi_plugin qtloop_plugin = {
        .name = "qtloop",
        .on_load = qtloop_setup,
	.options = qtloop_options,
	.init = qtloop_init,
};

