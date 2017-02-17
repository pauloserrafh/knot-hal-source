/*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "include/linux_log.h"
#include "include/gpio.h"

#define NEARD_SERVER "org.neard"
#define NEARD_INTERFACE "org.neard.Adapter"
#define NEARD_OBJECT "/org/neard/nfc0"

#define BUTTON 23

static GMainLoop *main_loop = NULL;
static gboolean opt_detach = TRUE;
static GDBusProxy *proxy_neard = NULL;
static gboolean pressed = FALSE;
static guint mgmtwatch;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

/*
 * OPTIONAL: describe the valid values ranges
 * for tx and channel
 */
static GOptionEntry options[] = {

	{ "nodetach", 'n', G_OPTION_FLAG_REVERSE,
					G_OPTION_ARG_NONE, &opt_detach,
					"Logging in foreground" },
	{ NULL },
};

static gboolean on_button_press(gpointer user_data)
{
	/* The buttons have an external pull-up */
	/* TODO: Add debouncing */
	if (!hal_gpio_digital_read(BUTTON)) {
		if (!pressed) {
			pressed = TRUE;
			hal_log_info("BUTTON PRESS\n");
			/* TODO: Power tag on and poll using neard */
		}
	} else {
		pressed = FALSE;
	}

	return TRUE;
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *gerr = NULL;
	int err, retval = 0;
	GDBusProxyFlags flags;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &gerr)) {
		hal_log_error("Invalid arguments: %s\n", gerr->message);
		g_error_free(gerr);
		g_option_context_free(context);
		retval = EXIT_FAILURE;
		goto done;
	}

	g_option_context_free(context);

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	flags = G_DBUS_PROXY_FLAGS_NONE;

	main_loop = g_main_loop_new(NULL, FALSE);

	hal_log_init("nfcd", opt_detach);
	hal_log_info("KnOT HAL NFCd\n");

	if (opt_detach) {
		if (daemon(0, 0)) {
			hal_log_error("Can't start daemon!");
			retval = EXIT_FAILURE;
			goto done;
		}
	}

	proxy_neard = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
						flags,
						NULL, /* GDBusInterfaceInfo */
						NEARD_SERVER,
						NEARD_OBJECT,
						NEARD_INTERFACE,
						NULL, /* GCancellable */
						&gerr);
	if (!proxy_neard) {
		hal_log_error("Error creating proxy neard: %s\n",
								gerr->message);
		g_error_free(gerr);
		goto done;
	}

	if (hal_gpio_setup()) {
		hal_log_error("IO SETUP ERROR\n");
		goto done;
	}
	hal_gpio_pin_mode(BUTTON, INPUT);
	mgmtwatch = g_idle_add(on_button_press, NULL);

	/* TODO: Write keys to nfc tag and to keys database */

	/* Set user id to nobody */
	if (setuid(65534) != 0) {
		err = errno;
		hal_log_error("Set uid to nobody failed. %s(%d). Exiting...",
							strerror(err), err);
		retval = EXIT_FAILURE;
		goto done;
	}

	g_main_loop_run(main_loop);

done:
	hal_log_error("exiting ...");
	hal_log_close();

	if (mgmtwatch)
		g_source_remove(mgmtwatch);
	if (proxy_neard)
		g_object_unref(proxy_neard);
	if (main_loop)
		g_main_loop_unref(main_loop);

	return retval;
}
