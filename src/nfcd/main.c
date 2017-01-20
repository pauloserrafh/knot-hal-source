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

static GMainLoop *main_loop = NULL;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	main_loop = g_main_loop_new(NULL, FALSE);

	/* TODO: Create proxies */
	/* TODO: Monitor button press on GPIO */
	/* TODO: Write keys to nfc tag and to keys database */

	printf("KnOT HAL NFCd\n");
	g_main_loop_run(main_loop);

	if (main_loop)
		g_main_loop_unref(main_loop);

	return 0;
}
