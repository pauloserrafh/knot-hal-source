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
#include <stdint.h>
#include <string.h>
#include "include/nrf24.h"
#include "include/time.h"

#define NRFD_SERVER "org.cesar.knot.nrf"
#define NRFD_INTERFACE "org.cesar.knot.nrf.manager"
#define NRFD_OBJECT "/org/cesar/nrf/nrf0"
/*
 * #define NEARD_SERVER_NAME "org.neard"
 * #define NEARD_ADAPTER_INTERFACE "org.neard.Adapter"
 * #define NEARD_TAG_INTERFACE "org.neard.Tag"
 * #define NEARD_OBJECT "/org/gtk/GDBus/TestObject"
 */

/* TODO: Remove temp server */
#define NEARD_SERVER "org.gtk.GDBus.TestServer"
#define NEARD_INTERFACE "org.gtk.GDBus.TestInterface"
#define NEARD_OBJECT "/org/gtk/GDBus/TestObject"
#define MAC_SIZE_BYTES 24

static GMainLoop *main_loop = NULL;
static GDBusProxy *proxy_nrfd = NULL;
static GDBusProxy *proxy_neard = NULL;
static guint mgmtwatch;
static uint32_t start_time;
static struct nrf24_mac addr;

static void set_nrf24MAC(void)
{
	uint8_t mac_mask = 4;

	memset(&addr, 0, sizeof(struct nrf24_mac));
	hal_getrandom(addr.address.b + mac_mask,
					sizeof(struct nrf24_mac) - mac_mask);
}

static void on_signal(GDBusProxy *proxy, gchar *sender_name,
		gchar *signal_name, GVariant *parameters, gpointer user_data)
{
	gchar *parameters_str;
	GDBusProxy *proxy_to_call = user_data;
	GDBusConnection *connection;
	const gchar *name_owner;
	GError *error;
	GVariant *reply;
	char key[] = "SECURITYKEY";
	char mac_str[MAC_SIZE_BYTES];

	error = NULL;
	parameters_str = g_variant_print(parameters, TRUE);
	g_print(" *** Received Signal: %s: %s\n", signal_name, parameters_str);

	/*
	 * FIXME: Only read/accept the data read if the button was pressed.
	 * Neard has a method to power on/off the adapter, turn ir on/off when
	 * the GPIO flag happens.
	 */
	if (!hal_timeout(hal_time_ms(), start_time, 10000)) {
		/* TODO: Generate the keys using hal_sec() */
		set_nrf24MAC();
		memset(mac_str, 0, sizeof(mac_str));
		nrf24_mac2str(&addr, mac_str);

		/* TODO: Act according to the received signal from neard */
		connection = g_dbus_proxy_get_connection(proxy_to_call);
		name_owner = g_dbus_proxy_get_name_owner(proxy_to_call);

		reply = g_dbus_connection_call_sync(connection, name_owner,
						NRFD_OBJECT, NRFD_INTERFACE,
						"UpdateDevice",
						g_variant_new("(ss)",
						key,mac_str), NULL,
						G_DBUS_MESSAGE_FLAGS_NONE, -1,
						NULL, &error);

		parameters_str = g_variant_print(reply, FALSE);
		g_print(" *** Method Response %s\n", parameters_str);

		g_free(parameters_str);
		/*
		 * TODO: Write keys to NFC Tag.
		 * Write Thing's public and private keys and MAC
		 * and GW public key.
		 */
	} else {
		printf("TIMEDOUT PRESS BUTTON AGAIN\n");
	}
}

static gboolean on_button_press(GIOChannel *io, GIOCondition cond,
							gpointer user_data)
{
	GDBusProxy *proxy = user_data;
	GDBusConnection *connection;
	const gchar *name_owner;
	char input;

	/* TODO: Power adapter on via neard */

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
		return FALSE;

	/* Clear stdin */
	scanf("%c", &input);

	printf("BUTTON PRESS\n");
	start_time = hal_time_ms();

	/* FIXME: Dummy call for test only. Remove when using neard. */
	connection = g_dbus_proxy_get_connection(proxy);
	name_owner = g_dbus_proxy_get_name_owner(proxy);
	g_dbus_connection_call(connection, name_owner, NEARD_OBJECT,
					NEARD_INTERFACE, "EmitSignal",
					g_variant_new("(d)", 10.0), NULL,
					G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED,
					-1, NULL, NULL, NULL);

	return TRUE;
}

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{
	GError *error;
	GDBusProxyFlags flags;
	GIOChannel *io = NULL;
	GIOCondition cond = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL;

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	flags = G_DBUS_PROXY_FLAGS_NONE;

	error = NULL;

	main_loop = g_main_loop_new(NULL, FALSE);

	proxy_nrfd = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
						flags,
						NULL, /* GDBusInterfaceInfo */
						NRFD_SERVER,
						NRFD_OBJECT,
						NRFD_INTERFACE,
						NULL, /* GCancellable */
						&error);

	if (!proxy_nrfd) {
		g_printerr("Error creating proxy nrfd: %s\n", error->message);
		g_error_free(error);
		goto out;
	}

	proxy_neard = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
						flags,
						NULL, /* GDBusInterfaceInfo */
						NEARD_SERVER,
						NEARD_OBJECT,
						NEARD_INTERFACE,
						NULL, /* GCancellable */
						&error);
	if (!proxy_neard) {
		g_printerr("Error creating proxy neard: %s\n", error->message);
		g_error_free(error);
		goto out;
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

	printf("KnOT HAL NFCd\n");
	g_main_loop_run(main_loop);

out:
	if (mgmtwatch)
		g_source_remove(mgmtwatch);
	if (proxy_nrfd)
		g_object_unref(proxy_nrfd);
	if (proxy_neard)
		g_object_unref(proxy_neard);
	if (main_loop)
		g_main_loop_unref(main_loop);

	return 0;
}
