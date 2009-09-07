/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * nmconfig -- NetworkManager CLI controlling utility
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2009 Witold Sowa <witold.sowa@gmail.com>
 */

#include <stdio.h>
#include <signal.h>
#include <glib.h>

#include "NMConfig.h"

static GMainLoop *loop = NULL;
gint return_value = 0;

static gboolean
quit_nmconfig (gpointer user_data, gint exit_code)
{
	g_main_loop_quit (loop);
	return_value = exit_code;
	return FALSE;
}

static void
signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM) {
		g_message ("Caught signal %d, shutting down...", signo);
		quit_nmconfig (NULL, 0);
	}
}

static void
setup_signals (void)
{
	struct sigaction action;
	sigset_t mask;

	sigemptyset (&mask);
	action.sa_handler = signal_handler;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGTERM,  &action, NULL);
	sigaction (SIGINT,  &action, NULL);
}

int main (int argc, char *argv[])
{
	NMConfig * nm_config;

	g_type_init ();

	loop = g_main_loop_new (NULL, FALSE);

	nm_config = nm_config_new (argc, argv);
	if (nm_config == NULL)
		return 1;

	g_signal_connect (nm_config, NM_CONFIG_FINISHED,
					  G_CALLBACK (quit_nmconfig),
					  NULL);

	setup_signals ();
	g_main_loop_run (loop);

	g_object_unref (G_OBJECT (nm_config));

	return return_value;
}
