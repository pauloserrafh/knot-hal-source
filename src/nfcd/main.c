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

#include <stdio.h>

#include "include/linux_log.h"
#include "include/gpio.h"
#include "include/time.h"

#define NEARD_SERVER "org.neard"
#define NEARD_INTERFACE "org.neard.Adapter"
#define NEARD_OBJECT "/org/neard/nfc0"
#define TAG_INTERFACE "org.neard.Tag"

#define BUTTON 23
#define LED 27
#define TIMEOUT 10

static GMainLoop *main_loop = NULL;
static gboolean opt_detach = TRUE;
static GDBusProxy *proxy_neard = NULL;
static GDBusObjectManager *manager = NULL;
static gboolean pressed = FALSE;
static guint mgmtwatch;
static gboolean tag_present = FALSE;

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

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static void on_object_removed(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
	gchar *owner;

	owner = g_dbus_object_manager_client_get_name_owner(
		G_DBUS_OBJECT_MANAGER_CLIENT (manager));
		g_print ("Removed object at %s (owner %s)\n",
			g_dbus_object_get_object_path(object), owner);

	tag_present = FALSE;
	/*
	 * TODO: Verify if the tag was successfully recorded with the info
	 * or if it is necessary to restart polling and record again.
	 */

	g_free (owner);
}

static void on_object_added(GDBusObjectManager *manager, GDBusObject *object,
							gpointer user_data)
{
	gchar *owner;
	const gchar *object_path;
	GVariant *response;
	GVariantBuilder builder;
	GVariant *dict;
	GError *gerr = NULL;
	GDBusProxy *proxy_tag = NULL;

	/*
	 * TODO: Verify if the object added is really a tag or something else
	 * e.g., an adapter.
	 */
	/* TODO: Copy MAC and keys to tag and send to nrfd */

	owner = g_dbus_object_manager_client_get_name_owner(
					G_DBUS_OBJECT_MANAGER_CLIENT(manager));

	object_path = g_dbus_object_get_object_path(object);

	hal_log_info("Added object at %s (owner %s)\n", object_path, owner);

	if(!strstr(object_path,"/tag"))
		goto done;

	if(strstr(object_path,"/record"))
		goto done;


	proxy_tag = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL, /* GDBusInterfaceInfo */
						NEARD_SERVER,
						object_path,
						TAG_INTERFACE,
						NULL, /* GCancellable */
						&gerr);
	if (!proxy_tag) {
		hal_log_error("Error creating proxy to tag: %s\n",
								gerr->message);
		g_error_free(gerr);
		goto done;
	}

	tag_present = TRUE;

	g_variant_builder_init(&builder, G_VARIANT_TYPE ("a{sv}"));

	g_variant_builder_add(&builder, "{sv}", "Type",
					g_variant_new("s", "Text"));

	g_variant_builder_add(&builder, "{sv}", "Encoding",
					g_variant_new("s", "UTF-8"));

	g_variant_builder_add(&builder, "{sv}", "Language",
					g_variant_new("s", "en"));

	g_variant_builder_add(&builder, "{sv}", "Representation",
					g_variant_new("s", "Hello,World"));

	dict = g_variant_builder_end(&builder);


	// builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	// g_variant_builder_add(builder, "{sv}", "Type",
	// 				g_variant_new("(s)", "Text"));

	// g_variant_builder_add(builder, "{sv}", "Encoding",
	// 				g_variant_new("(s)", "UTF-8"));

	// g_variant_builder_add(builder, "{sv}", "Language",
	// 				g_variant_new("(s)", "en-US"));

	// g_variant_builder_add(builder, "{sv}", "Representation",
	// 				g_variant_new("(s)", "Hello,World"));

	// dict = g_variant_builder_end(builder);

	// //dict = g_variant_new("a{sv}", builder);

	// g_variant_builder_unref(builder);

	response = g_dbus_proxy_call_sync(proxy_tag,
					"Write", g_variant_new_tuple(&dict, 1),
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, &gerr);

	if (gerr) {
		hal_log_error("Error2 %s\n", gerr->message);
		g_error_free(gerr);
		if(response)
			g_variant_unref(response);
		goto done;
	}

	g_object_unref(proxy_tag);
	g_variant_unref(response);
	hal_log_info("Wrote TAG!");

done:

	g_free(owner);
}

static long int attach_signal(void)
{
	int retval = 0;
	gulong signal_handler;

	signal_handler = g_signal_connect(manager, "object-added",
						G_CALLBACK(on_object_added),
						NULL);
	if (!signal_handler) {
		hal_log_error("Can't create handler for signal\n");
		retval = EXIT_FAILURE;
		goto done;
	}

	signal_handler = g_signal_connect(manager, "object-removed",
						G_CALLBACK(on_object_removed),
						NULL);
	if (!signal_handler) {
		hal_log_error("Can't create handler for signal\n");
		retval = EXIT_FAILURE;
	}

done:
	return retval;
}

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

	//hal_gpio_digital_write(LED, HIGH);
done:
	return retval;
}

static int stop_adapter(void)
{
	GError *gerr = NULL;
	int retval = 0;
	GVariant *response, *tmp;
	gboolean powered, polling;

	/* Check if still polling */
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

	if (polling) {
		/* Stop polling */
		response = g_dbus_proxy_call_sync(proxy_neard, "StopPollLoop",
					NULL,
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, &gerr);
		if (gerr) {
			hal_log_error("Error %s\n", gerr->message);
			retval = EXIT_FAILURE;
			g_error_free(gerr);
			goto done;
		}
		g_variant_unref(response);
		hal_log_info("Stop Polling\n");
	}

	/* Check if adapter is powered on */
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

	if (powered) {
		/* Power off adapter */
		response = g_dbus_proxy_call_sync(proxy_neard,
					"org.freedesktop.DBus.Properties.Set",
					g_variant_new("(ssv)", NEARD_INTERFACE,
					"Powered",
					g_variant_new_boolean(FALSE)),
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, &gerr);
		if (gerr) {
			hal_log_error("Error %s\n", gerr->message);
			retval = EXIT_FAILURE;
			g_error_free(gerr);
			goto done;
		}
		g_variant_unref(response);
		hal_log_info("Powered off\n");
	}
	//hal_gpio_digital_write(LED, LOW);

done:
	return retval;
}

static gboolean timed_out(gpointer user_data)
{
	/* If the tag is still present, do not turn off the adapter */
	if(tag_present)
		return TRUE;

	stop_adapter();
	return FALSE;
}

static gboolean on_button_press(GIOChannel *io, GIOCondition cond, gpointer user_data)
{
	int err;
	char input;

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	/* Clear stdin */
	scanf("%c", &input);


	/* The buttons have an external pull-up */
	/* TODO: Add debouncing */
	//if (!hal_gpio_digital_read(BUTTON)) {
		if (!pressed) {
			pressed = TRUE;
			err = start_adapter();
			if (err)
				goto done;

			g_timeout_add_seconds(TIMEOUT, timed_out, NULL);
		}
	//} else {
		pressed = FALSE;
	//}

done:
	/* Sleep to allow system to handle signals */
	hal_delay_us(100);
	return TRUE;
}

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

	manager = g_dbus_object_manager_client_new_for_bus_sync(
					G_BUS_TYPE_SYSTEM,
					G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
					"org.neard", "/", NULL, NULL, NULL,
					NULL, &gerr);
	if (gerr) {
		hal_log_error("Error %s\n", gerr->message);
		g_error_free(gerr);
		goto done;
	}

/**********************************************************************
	if (hal_gpio_setup()) {
		hal_log_error("IO SETUP ERROR\n");
		goto done;
	}
	hal_gpio_pin_mode(LED, OUTPUT);
	hal_gpio_pin_mode(BUTTON, INPUT);
	mgmtwatch = g_idle_add(on_button_press, NULL);
***********************************************************************/

	io = g_io_channel_unix_new(STDIN_FILENO);
	mgmtwatch = g_io_add_watch(io, cond, on_button_press, proxy_neard);
	g_io_channel_unref(io);

	/* Attach functions to signals for Interfaces Added and Removed */
	err = attach_signal();
	if (err)
		goto done;

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
	stop_adapter();
	hal_log_error("exiting ...");
	hal_log_close();

	if (manager)
		g_object_unref(manager);
	if (mgmtwatch)
		g_source_remove(mgmtwatch);
	if (proxy_neard)
		g_object_unref(proxy_neard);
	if (main_loop)
		g_main_loop_unref(main_loop);

	return retval;
}
