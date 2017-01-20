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

static GMainLoop *main_loop = NULL;
static GDBusProxy *proxy_nrfd = NULL;
static GDBusProxy *proxy_neard = NULL;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{
	GError *error;
	GDBusProxyFlags flags;

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

	/* TODO: Monitor button press on GPIO */
	/* TODO: Write keys to nfc tag and to keys database */

	printf("KnOT HAL NFCd\n");
	g_main_loop_run(main_loop);

out:
	if (proxy_nrfd)
		g_object_unref(proxy_nrfd);
	if (proxy_neard)
		g_object_unref(proxy_neard);
	if (main_loop)
		g_main_loop_unref(main_loop);

	return 0;
}
