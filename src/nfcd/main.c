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

#include "log.h"



static GMainLoop *main_loop = NULL;
static gboolean opt_detach = TRUE;

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

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &gerr)) {
		g_printerr("Invalid arguments: %s\n", gerr->message);
		g_error_free(gerr);
		g_option_context_free(context);
		return EXIT_FAILURE;
	}

	g_option_context_free(context);

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	main_loop = g_main_loop_new(NULL, FALSE);

	log_init("nfcd", opt_detach);
	log_info("KnOT HAL NFCd\n");

	/* Set user id to nobody */
	if (setuid(65534) != 0) {
		err = errno;
		log_error("Set uid to nobody failed. %s(%d). Exiting...",
							strerror(err), err);
		retval = EXIT_FAILURE;
		goto done;
	}

	if (opt_detach) {
		if (daemon(0, 0)) {
			log_error("Can't start daemon!");
			retval = EXIT_FAILURE;
			goto done;
		}
	}

	/* TODO: Create proxies */
	/* TODO: Monitor button press on GPIO */
	/* TODO: Write keys to nfc tag and to keys database */

	g_main_loop_run(main_loop);

done:
	log_error("exiting ...");
	log_close();

	if (main_loop)
		g_main_loop_unref(main_loop);

	return retval;
}
