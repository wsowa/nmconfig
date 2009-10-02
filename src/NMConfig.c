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


#include <glib.h>
#include <glib-object.h>
#include <NetworkManager.h>
#include <nm-client.h>
#include <nm-device.h>

#include "NMConfig.h"
#include "NMConfigDevicePrintHelper.h"

G_DEFINE_TYPE (NMConfig, nm_config, G_TYPE_OBJECT)

#define NM_CONFIG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_CONFIG, NMConfigPrivate))

typedef struct {
	GPtrArray * args;

	NMClient * client;
	GPtrArray * devices; /* array with NMDevice objects */
//	NMRemoteSettings * system_settings;
//	NMRemoteSettings * user_settings;

	guint parse_id;
} NMConfigPrivate;

enum {
	PROP_0,

	LAST_PROP
};

enum {
	FINISHED,

	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static gchar *
state_to_string (NMState state)
{
    switch (state) {
    case NM_STATE_UNKNOWN:
        return "Unknown";
    case NM_STATE_ASLEEP:
        return "Asleep";
    case NM_STATE_CONNECTING:
        return "Connecting";
    case NM_STATE_CONNECTED:
        return "Connected";
    case NM_STATE_DISCONNECTED:
        return "Disconnected";
    default:
        return "State not recognized";
    }

}

static GPtrArray *
get_devices_list (NMConfig * self) {
    NMConfigPrivate * priv = NM_CONFIG_GET_PRIVATE (self);

    g_return_val_if_fail (NM_IS_CONFIG (self), NULL);
    g_return_val_if_fail (priv->client, NULL);

    if (! priv->devices) {
        priv->devices = (GPtrArray *) nm_client_get_devices (priv->client);
    }

    return priv->devices;
}

static NMDevice *
get_device_by_ifname (NMConfig * self, const char * ifname)
{
    GPtrArray * devices;
    int i;

	g_return_val_if_fail (NM_IS_CONFIG (self), NULL);
	g_return_val_if_fail (ifname, NULL);

	devices = get_devices_list (self);
	for (i = 0; i < devices->len; i++) {
		NMDevice * device = NM_DEVICE (g_ptr_array_index (devices, i));

		if(!g_strcmp0 (nm_device_get_iface(device), ifname)) {
			return device;
		}
	}

	return NULL;
}

static void
show_nm_info (NMConfig * self)
{
	NMConfigPrivate * priv = NM_CONFIG_GET_PRIVATE (self);
	NMClient * client = priv->client;

	NMState nm_state;
	gboolean is_wireless_enabled, is_wireless_hw_enabled;

	g_return_if_fail (NM_IS_CONFIG (self));

	is_wireless_enabled = nm_client_wireless_get_enabled (client);
	is_wireless_hw_enabled = nm_client_wireless_hardware_get_enabled (client);
	nm_state = nm_client_get_state (client);

	g_print ("NetworkManager state:      %s\n", state_to_string(nm_state));
	g_print ("Wireless enabled:          %s\n", (is_wireless_enabled ? "Yes" : "No"));
	g_print ("Wireless hardware enabled: %s\n", (is_wireless_hw_enabled ? "Yes" : "No"));

	g_print ("\n");
}


static void
list_devices (NMConfig * self)
{
    GPtrArray * devices;
    int i;

    g_return_if_fail (NM_IS_CONFIG (self));

    devices = get_devices_list (self);

    for (i = 0; i < devices->len; i++) {
    	NMDevice * device = NM_DEVICE (g_ptr_array_index (devices, i));
    	nm_config_device_show_generic_info (device);
    }
}

static void
list_connections (NMConfig * self)
{

}

static gboolean
parse_command_line (gpointer user_data)
{
	NMConfig *self = NM_CONFIG (user_data);
	NMConfigPrivate *priv = NM_CONFIG_GET_PRIVATE (self);
	GPtrArray * args = priv->args;

	if (args->len == 0) {
		show_nm_info (self);
		list_devices (self);
		list_connections (self);
	}
	else if (args->len == 1) {
		NMDevice * device;
		const char * ifname = g_ptr_array_index(args, 0);

		device = get_device_by_ifname (self, ifname);
		if (!device) {
			g_printerr("NetworkManager dosn't know device: %s\n", ifname);
			g_signal_emit(self, signals[FINISHED], 0, 1);
			goto out;
		}
		nm_config_device_show_generic_info (device);
	}




	g_signal_emit(self, signals[FINISHED], 0, 0);

out:
	priv->parse_id = 0;
	return FALSE;
}


NMConfig *
nm_config_new (guint argc, gchar *argv[])
{
	NMConfig * nm_config;
	NMConfigPrivate *priv;
	GPtrArray * args;
	int i;

	g_assert (argv);

	args = g_ptr_array_sized_new (argc-1);
	for (i = 1; i < argc; i++) {
		g_ptr_array_add(args, argv[i]);
	}

	nm_config = (NMConfig *) g_object_new (NM_TYPE_CONFIG, NULL);

	if (nm_config) {
		priv = NM_CONFIG_GET_PRIVATE (nm_config);
		priv->args = args;
	}

	return nm_config;
}

static void
nm_config_init (NMConfig *self)
{
    NMConfigPrivate *priv = NM_CONFIG_GET_PRIVATE (self);

    priv->devices = NULL;
}

static GObject *
constructor (GType type,
             guint n_construct_params,
             GObjectConstructParam *construct_params)
{
	GObject *object;
	NMConfigPrivate *priv;

	gboolean is_nm_running;

	object = G_OBJECT_CLASS (nm_config_parent_class)->constructor (type, n_construct_params, construct_params);
	if (!object)
		return NULL;

	priv = NM_CONFIG_GET_PRIVATE (object);

	priv->client = nm_client_new ();
	is_nm_running = nm_client_get_manager_running (priv->client);

	if (!is_nm_running) {
		g_printerr("NetworkManager is not running\n");
		g_signal_emit(object, signals[FINISHED], 0, 1);
	}
	else
		priv->parse_id = g_idle_add (parse_command_line, object);

	return object;
}

static void
dispose (GObject *object)
{
	NMConfigPrivate *priv = NM_CONFIG_GET_PRIVATE (object);

	if (priv->parse_id)
		g_source_remove (priv->parse_id);

	if (priv->client)
		g_object_unref(priv->client);

	if (priv->args)
		g_ptr_array_free (priv->args, TRUE);

	G_OBJECT_CLASS (nm_config_parent_class)->dispose (object);
}

static void
nm_config_class_init (NMConfigClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (class, sizeof (NMConfigPrivate));

	/* Virtual methods */
	object_class->constructor = constructor;
	object_class->dispose = dispose;

	/* Properties */


	/* Signals */
	signals[FINISHED] =
			g_signal_new (NM_CONFIG_FINISHED,
			              G_OBJECT_CLASS_TYPE (object_class),
			              G_SIGNAL_RUN_LAST,
			              G_STRUCT_OFFSET (NMConfigClass, finished),
			              NULL, NULL,
			              g_cclosure_marshal_VOID__INT,
			              G_TYPE_NONE, 1, G_TYPE_INT);

}

