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
#include <arpa/inet.h>
#include <dbus/dbus-glib.h>
#include <NetworkManager.h>
#include <nm-client.h>
#include <nm-device.h>
#include <nm-setting-ip4-config.h>
#include <nm-setting-ip6-config.h>
#include <nm-remote-settings.h>
#include <nm-settings-interface.h>
#include <nm-utils.h>


#include "NMConfig.h"

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

static gchar *
device_state_to_string (NMDeviceState state)
{
    switch (state) {
    case NM_DEVICE_STATE_UNKNOWN:
        return "Unknown";
    case NM_DEVICE_STATE_UNMANAGED:
        return "Unmanaged";
    case NM_DEVICE_STATE_UNAVAILABLE:
        return "Unavailable";
    case NM_DEVICE_STATE_DISCONNECTED:
        return "Disconnected";
    case NM_DEVICE_STATE_PREPARE:
        return "Connecting (prepare)";
    case NM_DEVICE_STATE_CONFIG:
        return "Connecting (configuration)";
    case NM_DEVICE_STATE_NEED_AUTH:
        return "Connecting (need authentication)";
    case NM_DEVICE_STATE_IP_CONFIG:
        return "Connecting (getting IP configuration)";
    case NM_DEVICE_STATE_ACTIVATED:
        return "Connected";
    case NM_DEVICE_STATE_FAILED:
        return "Connection try failed";
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
print_ip4_addr (gpointer object, gpointer user_data)
{
	NMIP4Address * address = (NMIP4Address *) object;
	guint32 addr = nm_ip4_address_get_address (address);
	guint32 prefix = nm_ip4_address_get_prefix (address);
	guint32 netmask = nm_utils_ip4_prefix_to_netmask (prefix);
	guint32 gateway = nm_ip4_address_get_gateway (address);

	struct in_addr tmp_addr;
	char buf[INET_ADDRSTRLEN + 1];

	g_print ("%-9s ", "");

	tmp_addr.s_addr = addr;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("IPv4:%s  ", buf);

	tmp_addr.s_addr = netmask;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("Netmask:%s  ", buf);

	tmp_addr.s_addr = gateway;
	inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));
	g_print ("Gateway:%s\n", buf);
}

static void
print_ip6_addr (gpointer object, gpointer user_data)
{
	const struct in6_addr * tmp_addr;
	char buf[INET6_ADDRSTRLEN + 1];
	NMIP6Address * address = (NMIP6Address *) object;
	guint32 prefix = nm_ip6_address_get_prefix (address);

	tmp_addr = nm_ip6_address_get_address (address);

	inet_ntop (AF_INET6, &tmp_addr, buf, sizeof (buf));
	g_print ("%-9s IPv6:%s/%d\n", "", buf, prefix);
}

static void
print_ip4_info (NMIP4Config * ip4)
{
	GSList * addresses;
	const GArray * dns;
	const GPtrArray * domains;
	int i;

	if (!ip4)
		return;

	g_return_if_fail (NM_IS_IP4_CONFIG (ip4));

	addresses = (GSList *) nm_ip4_config_get_addresses (ip4);
	dns = nm_ip4_config_get_nameservers (ip4);
	domains = nm_ip4_config_get_domains (ip4);

	g_slist_foreach (addresses, (GFunc) print_ip4_addr, NULL);

	if ((domains && domains->len) || (dns && dns->len))
		g_print("%-9s ", "");

	if (dns && dns->len) {
		g_print("DNS:");

		for (i = 0; i < dns->len; i++) {
			struct in_addr tmp_addr;
			char buf[INET_ADDRSTRLEN + 1];
			guint32 addr = g_array_index(dns, guint32, i);

			tmp_addr.s_addr = addr;
			inet_ntop (AF_INET, &tmp_addr, buf, sizeof (buf));

			g_print("%s ", buf);
		}
		g_print (" ");
	}

	if (domains && domains->len) {
		g_print("Domains:");

		for (i = 0; i < domains->len; i++) {
			char * domain = (char *) g_ptr_array_index(domains, i);
			g_print("%s ", domain);
		}
	}

	if ((domains && domains->len) || (dns && dns->len))
		g_print("\n");

}

static void
print_ip6_info (NMIP6Config * ip6)
{
	GSList * addresses;
	GSList * dns;
	const GPtrArray * domains;
	int i;

	if (!ip6)
		return;

	g_return_if_fail (NM_IS_IP6_CONFIG (ip6));

	addresses = (GSList *) nm_ip6_config_get_addresses (ip6);
	dns = (GSList *) nm_ip6_config_get_nameservers (ip6);
	domains = nm_ip6_config_get_domains (ip6);

	g_slist_foreach (addresses, (GFunc) print_ip6_addr, NULL);

	if ((domains && domains->len) || (dns && dns->data))
		g_print("%-9s ", "");

	if (dns && dns->data) {
		g_print("DNS:");

		while(dns) {
			char buf[INET6_ADDRSTRLEN + 1];
			struct in6_addr * tmp_addr = dns->data;

			dns = g_slist_next (dns);
			inet_ntop (AF_INET6, &tmp_addr, buf, sizeof (buf));
			g_print("%s ", buf);

		}

		g_print (" ");
	}

	if (domains && domains->len) {
		g_print("Domains:");

		for (i = 0; i < domains->len; i++) {
			char * domain = (char *) g_ptr_array_index(domains, i);
			g_print("%s ", domain);
		}
	}

	if ((domains && domains->len) || (dns && dns->data))
		g_print("\n");

}

static void
show_generic_device_info (NMDevice * device)
{
	const char *ifname, *uid, *driver;
	gboolean managed;
	NMDeviceState state;
	NMIP4Config * ip4;
	NMIP6Config * ip6;

	g_return_if_fail (NM_IS_DEVICE (device));

	ifname = nm_device_get_iface (device);
	managed = nm_device_get_managed (device);

	if (managed) {
		state = nm_device_get_state (device);
		ip4 = nm_device_get_ip4_config (device);
		ip6 = nm_device_get_ip6_config (device);
		driver = nm_device_get_driver (device);
		uid = nm_device_get_udi (device);

		//TODO: show active connection name
		g_print("%-9s State:%s  Connection:%s\n", ifname, device_state_to_string(state), "Not implemented");

		print_ip4_info (ip4);

		print_ip6_info (ip6);

		if (driver || uid) {
			g_print("%-9s ", "");
			if (driver)
				g_print("Driver:%s  ", driver);
			if (uid)
				g_print("UID:%s", uid);
			g_print ("\n");
		}




	}
	else {
		g_print("%-9s Device is not managed by NetworkManager\n", ifname);
	}

	//
	g_print("\n");
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
    	show_generic_device_info (device);
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
		show_generic_device_info (device);
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

