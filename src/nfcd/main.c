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
#define LED 27

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

static int start_adapter(void)
{
	GError *gerr = NULL;
	int retval = 0;
	GVariant *response, *tmp;
	gboolean powered, polling;

	/* Check if adapter already powered on */
	response = g_dbus_proxy_call_sync(proxy_neard,
					"org.freedesktop.DBus.Properties.Get",
					g_variant_new("(ss)", NEARD_INTERFACE,
					"Powered"),
					G_DBUS_MESSAGE_FLAGS_NONE,
					-1, NULL, &gerr);

	if (gerr) {
		hal_log_error("Error %s\n", gerr->message);
		g_error_free(gerr);
		goto done;
	}

	g_variant_get_child(response, 0, "v", &tmp);
	powered = g_variant_get_boolean(tmp);

	g_variant_unref(response);
	g_variant_unref(tmp);

	if (!powered) {
		/* Power on adapter */
		response = g_dbus_proxy_call_sync(proxy_neard,
					"org.freedesktop.DBus.Properties.Set",
					g_variant_new("(ssv)", NEARD_INTERFACE,
					"Powered", g_variant_new_boolean(TRUE)),
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, &gerr);
		if (gerr) {
			hal_log_error("Error %s\n", gerr->message);
			retval = EXIT_FAILURE;
			g_error_free(gerr);
			goto done;
		}
		g_variant_unref(response);
		hal_log_info("Powered on\n");
	}

	/* Check if already polling */
	response = g_dbus_proxy_call_sync(proxy_neard,
					"org.freedesktop.DBus.Properties.Get",
					g_variant_new("(ss)", NEARD_INTERFACE,
					"Polling"),
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, &gerr);

	if (gerr) {
		hal_log_error("Error %s\n", gerr->message);
		g_error_free(gerr);
		goto done;
	}

	g_variant_get_child(response, 0, "v", &tmp);
	polling = g_variant_get_boolean(tmp);

	g_variant_unref(response);
	g_variant_unref(tmp);

	if (!polling) {
		/* Start polling */
		response = g_dbus_proxy_call_sync(proxy_neard, "StartPollLoop",
					g_variant_new("(s)", "Initiator"),
					G_DBUS_MESSAGE_FLAGS_NONE,
					-1, NULL, &gerr);
		if (gerr) {
			hal_log_error("Error %s\n", gerr->message);
			retval = EXIT_FAILURE;
			g_error_free(gerr);
			goto done;
		}
		g_variant_unref(response);
		hal_log_info("Polling\n");
	}

	hal_gpio_digital_write(LED, HIGH);
done:
	return retval;
}

static gboolean on_button_press(gpointer user_data)
{
	int err;

	/* The buttons have an external pull-up */
	/* TODO: Add debouncing */
	if (!hal_gpio_digital_read(BUTTON)) {
		if (!pressed) {
			pressed = TRUE;
			err = start_adapter();
			if (err)
				goto done;
			/* Attach function to TagFound signal */
			/* Copy MAC and keys to tag and send to nrfd */
			/* Power off adapter after timeout */
		}
	} else {
		pressed = FALSE;
	}

done:
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
	hal_gpio_pin_mode(LED, OUTPUT);
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
