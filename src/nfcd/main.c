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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "log.h"

#define NRFD_SERVER "org.cesar.knot.nrf"
#define NRFD_INTERFACE "org.cesar.knot.nrf.manager"
#define NRFD_OBJECT "/org/cesar/nrf/nrf0"

#define NEARD_SERVER "org.neard"
#define NEARD_INTERFACE "org.neard.Adapter"
#define NEARD_OBJECT "/org/gtk/GDBus/TestObject"


static GMainLoop *main_loop = NULL;
static gboolean opt_detach = TRUE;
static GDBusProxy *proxy_nrfd = NULL;
static GDBusProxy *proxy_neard = NULL;
static guint mgmtwatch;

static void on_signal(GDBusProxy *proxy, gchar *sender_name,
		gchar *signal_name, GVariant *parameters, gpointer user_data)
{
	gchar *parameters_str;
	GDBusProxy *proxy_to_call = user_data;
	GDBusConnection *connection;
	gchar *name_owner;
	GError *error;
	GVariant *reply;
	char mac[] = "MAC";
	char key[] = "SECURITYKEY";

	error = NULL;

	parameters_str = g_variant_print(parameters, TRUE);
	log_info(" *** Received Signal: %s: %s\n", signal_name, parameters_str);
	g_free(parameters_str);

	/*
	 * FIXME: Only read/accept the data read if the button was pressed.
	 * Neard has a method to power on/off the adapter, turn ir on/off when
	 * the GPIO flag happens.
	 */
	/* TODO: Act according to the received signal */
	connection = g_dbus_proxy_get_connection(proxy_to_call);
	name_owner = g_dbus_proxy_get_name_owner(proxy_to_call);

	reply = g_dbus_connection_call_sync(connection, name_owner,
					NRFD_OBJECT, NRFD_INTERFACE,
					"UpdateDevice", g_variant_new("(ss)",
					key, mac), NULL,
					G_DBUS_MESSAGE_FLAGS_NONE, -1, NULL,
					&error);

	parameters_str = g_variant_print(reply, FALSE);
	log_info(" *** Method Response %s\n", parameters_str);

	g_variant_unref(reply);
	g_free(name_owner);
	g_free(parameters_str);
}

static gboolean on_button_press(GIOChannel *io, GIOCondition cond,
							gpointer user_data)
{
	GDBusProxy *proxy = user_data;
	GDBusConnection *connection;
	gchar *name_owner;
	char input;

	/* TODO: Power adapter on via neard */
	/* TODO: Start timeout and turn off adapter after it overflows*/

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	/* Clear stdin */
	scanf("%c", &input);

	/* FIXME: Dummy call for test only. Remove when using neard. */
	connection = g_dbus_proxy_get_connection(proxy);
	name_owner = g_dbus_proxy_get_name_owner(proxy);
	g_dbus_connection_call(connection, name_owner, NEARD_OBJECT,
					NEARD_INTERFACE, "EmitSignal",
					g_variant_new("(d)", 10.0), NULL,
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, NULL, NULL);

	g_free(name_owner);

	return TRUE;
}

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

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *gerr = NULL;
	int err, retval = 0;
	GDBusProxyFlags flags;
	GIOChannel *io = NULL;
	GIOCondition cond = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &gerr)) {
		log_error("Invalid arguments: %s\n", gerr->message);
		g_error_free(gerr);
		g_option_context_free(context);
		return EXIT_FAILURE;
	}

	g_option_context_free(context);

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	flags = G_DBUS_PROXY_FLAGS_NONE;

	main_loop = g_main_loop_new(NULL, FALSE);

	log_init("nfcd", opt_detach);
	log_info("KnOT HAL NFCd\n");

	if (opt_detach) {
		if (daemon(0, 0)) {
			log_error("Can't start daemon!");
			retval = EXIT_FAILURE;
			goto done;
		}
	}

	proxy_nrfd = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
						flags,
						NULL, /* GDBusInterfaceInfo */
						NRFD_SERVER,
						NRFD_OBJECT,
						NRFD_INTERFACE,
						NULL, /* GCancellable */
						&gerr);

	if (!proxy_nrfd) {
		log_error("Error creating proxy nrfd: %s\n", gerr->message);
		g_error_free(gerr);
		goto done;
	}

	proxy_neard = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
						flags,
						NULL, /* GDBusInterfaceInfo */
						NEARD_SERVER,
						NEARD_OBJECT,
						NEARD_INTERFACE,
						NULL, /* GCancellable */
						&gerr);
	if (!proxy_neard) {
		log_error("Error creating proxy neard: %s\n", gerr->message);
		g_error_free(gerr);
		goto done;
	}

	g_signal_connect(proxy_neard, "g-signal",
			G_CALLBACK(on_signal), proxy_nrfd);

	/*
	 * FIXME: on_button_press should be called when the external button is
	 * pressed
	 */
	io = g_io_channel_unix_new(STDIN_FILENO);
	mgmtwatch = g_io_add_watch(io, cond, on_button_press, proxy_neard);
	g_io_channel_unref(io);

	/* TODO: Write keys to nfc tag and to keys database */

	/* Set user id to nobody */
	if (setuid(65534) != 0) {
		err = errno;
		log_error("Set uid to nobody failed. %s(%d). Exiting...",
							strerror(err), err);
		retval = EXIT_FAILURE;
		goto done;
	}

	g_main_loop_run(main_loop);

done:
	log_error("exiting ...");
	log_close();

	if (mgmtwatch)
		g_source_remove(mgmtwatch);
	if (proxy_nrfd)
		g_object_unref(proxy_nrfd);
	if (proxy_neard)
		g_object_unref(proxy_neard);
	if (main_loop)
		g_main_loop_unref(main_loop);

	return retval;
}
